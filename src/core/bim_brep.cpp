#include "acp/bim_brep.hpp"

#include <cmath>
#include <optional>
#include <vector>

namespace acp::bim {
namespace {

constexpr double kGeometryTolerance = 1e-9;

struct WallFrame {
    geo::Vec2 start{};
    geo::Vec2 end{};
    double length{};
    double tangent_x{};
    double tangent_y{};
    double normal_x{};
    double normal_y{};
};

[[nodiscard]] std::optional<double> base_elevation_mm(
    const ProjectModel& project,
    const Element& element) {
    const auto* level = project.find_level(element.level_id);
    if (!level) {
        return std::nullopt;
    }
    return level->elevation_mm + element.base_offset_mm;
}

[[nodiscard]] std::optional<double> wall_height_mm(
    const ProjectModel& project,
    const Element& element,
    double base_z_mm) {
    if (element.top_level_id.has_value()) {
        const auto* top = project.find_level(*element.top_level_id);
        if (!top) {
            return std::nullopt;
        }
        const double height = top->elevation_mm + element.top_offset_mm - base_z_mm;
        if (height > 0.0) {
            return height;
        }
        return std::nullopt;
    }
    if (element.unconnected_height_mm > 0.0) {
        return element.unconnected_height_mm;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<WallFrame> wall_frame(const Element& wall) {
    if (wall.kind != ElementKind::Wall || !wall.location_curve.has_value() ||
        wall.thickness_mm <= 0.0) {
        return std::nullopt;
    }

    const auto start = wall.location_curve->start;
    const auto end = wall.location_curve->end;
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double length = std::hypot(dx, dy);
    if (length <= kGeometryTolerance) {
        return std::nullopt;
    }

    return WallFrame{
        start,
        end,
        length,
        dx / length,
        dy / length,
        -dy / length,
        dx / length,
    };
}

[[nodiscard]] std::optional<std::vector<geo::Vec2>> wall_profile(
    const Element& wall) {
    const auto frame = wall_frame(wall);
    if (!frame.has_value()) {
        return std::nullopt;
    }

    const double half = wall.thickness_mm * 0.5;
    const double nx = frame->normal_x * half;
    const double ny = frame->normal_y * half;

    return std::vector<geo::Vec2>{
        {frame->start.x + nx, frame->start.y + ny},
        {frame->end.x + nx, frame->end.y + ny},
        {frame->end.x - nx, frame->end.y - ny},
        {frame->start.x - nx, frame->start.y - ny},
    };
}

[[nodiscard]] bool valid_hosted_opening(
    const Element& opening,
    const Element& wall,
    double wall_height) noexcept {
    if ((opening.kind != ElementKind::Door && opening.kind != ElementKind::Window) ||
        opening.width_mm <= 0.0 || opening.height_mm <= 0.0 ||
        opening.sill_height_mm < 0.0 || opening.cut_clearance_mm < 0.0 ||
        opening.host_offset_normalized < 0.0 || opening.host_offset_normalized > 1.0) {
        return false;
    }

    const auto frame = wall_frame(wall);
    if (!frame.has_value()) {
        return false;
    }

    const double center_distance = opening.host_offset_normalized * frame->length;
    const double half_width = opening.width_mm * 0.5;
    if (center_distance - half_width < -kGeometryTolerance ||
        center_distance + half_width > frame->length + kGeometryTolerance) {
        return false;
    }

    if (opening.sill_height_mm + opening.height_mm >
        wall_height + kGeometryTolerance) {
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<std::vector<geo::Vec2>> opening_profile(
    const Element& opening,
    const Element& wall,
    double depth_mm) {
    const auto frame = wall_frame(wall);
    if (!frame.has_value() || depth_mm <= 0.0 || opening.width_mm <= 0.0 ||
        opening.host_offset_normalized < 0.0 || opening.host_offset_normalized > 1.0) {
        return std::nullopt;
    }

    const double center_distance = opening.host_offset_normalized * frame->length;
    const double half_width = opening.width_mm * 0.5;
    if (center_distance - half_width < -kGeometryTolerance ||
        center_distance + half_width > frame->length + kGeometryTolerance) {
        return std::nullopt;
    }

    const double cx = frame->start.x + frame->tangent_x * center_distance;
    const double cy = frame->start.y + frame->tangent_y * center_distance;
    const double tx = frame->tangent_x * half_width;
    const double ty = frame->tangent_y * half_width;
    const double nx = frame->normal_x * depth_mm * 0.5;
    const double ny = frame->normal_y * depth_mm * 0.5;

    return std::vector<geo::Vec2>{
        {cx - tx + nx, cy - ty + ny},
        {cx + tx + nx, cy + ty + ny},
        {cx + tx - nx, cy + ty - ny},
        {cx - tx - nx, cy - ty - ny},
    };
}

[[nodiscard]] std::optional<brep::Shape> wall_shape_with_hosted_openings(
    const ProjectModel& project,
    ElementId wall_id,
    const Element& wall,
    brep::Kernel& kernel,
    double base_z,
    double height) {
    const auto profile = wall_profile(wall);
    if (!profile.has_value()) {
        return std::nullopt;
    }

    auto shape = kernel.extrude(*profile, height, base_z);
    if (!shape.has_value()) {
        return std::nullopt;
    }

    const auto hosted_ids = project.hosted_by(wall_id);
    bool has_openings = false;
    for (const auto hosted_id : hosted_ids) {
        const auto* hosted = project.find_element(hosted_id);
        if (hosted && (hosted->kind == ElementKind::Door || hosted->kind == ElementKind::Window)) {
            has_openings = true;
            break;
        }
    }

    // Compatibility geometry must never pretend that a hosted opening has
    // been cut. Exact hosted openings are enabled only by a production BRep
    // kernel with real Boolean Cut support.
    if (has_openings && !kernel.production_brep()) {
        return std::nullopt;
    }

    for (const auto hosted_id : hosted_ids) {
        const auto* opening = project.find_element(hosted_id);
        if (!opening || (opening->kind != ElementKind::Door && opening->kind != ElementKind::Window)) {
            continue;
        }
        if (!valid_hosted_opening(*opening, wall, height)) {
            return std::nullopt;
        }

        const double cutter_depth =
            wall.thickness_mm + 2.0 * opening->cut_clearance_mm;
        const auto cutter_profile = opening_profile(*opening, wall, cutter_depth);
        if (!cutter_profile.has_value()) {
            return std::nullopt;
        }

        const double cutter_base_z = base_z + opening->sill_height_mm;
        const auto cutter = kernel.extrude(
            *cutter_profile,
            opening->height_mm,
            cutter_base_z);
        if (!cutter.has_value()) {
            return std::nullopt;
        }

        auto cut_shape = kernel.boolean_cut(*shape, *cutter);
        if (!cut_shape.has_value()) {
            return std::nullopt;
        }
        shape = std::move(cut_shape);
    }

    return shape;
}

[[nodiscard]] std::optional<brep::Shape> hosted_opening_panel_shape(
    const ProjectModel& project,
    ElementId opening_id,
    const Element& opening,
    brep::Kernel& kernel) {
    if ((opening.kind != ElementKind::Door && opening.kind != ElementKind::Window) ||
        opening.width_mm <= 0.0 || opening.height_mm <= 0.0 ||
        opening.depth_mm <= 0.0 || opening.sill_height_mm < 0.0) {
        return std::nullopt;
    }

    const auto host_id = project.host_of(opening_id);
    if (!host_id.has_value()) {
        return std::nullopt;
    }
    const auto* wall = project.find_element(*host_id);
    if (!wall || wall->kind != ElementKind::Wall) {
        return std::nullopt;
    }

    const auto wall_base = base_elevation_mm(project, *wall);
    if (!wall_base.has_value()) {
        return std::nullopt;
    }
    const auto wall_height = wall_height_mm(project, *wall, *wall_base);
    if (!wall_height.has_value() || !valid_hosted_opening(opening, *wall, *wall_height)) {
        return std::nullopt;
    }

    const auto profile = opening_profile(opening, *wall, opening.depth_mm);
    if (!profile.has_value()) {
        return std::nullopt;
    }
    return kernel.extrude(
        *profile,
        opening.height_mm,
        *wall_base + opening.sill_height_mm);
}

} // namespace

std::optional<DerivedBrepGeometry> build_brep_geometry(
    const ProjectModel& project,
    ElementId element_id,
    brep::Kernel& kernel,
    brep::TessellationOptions tessellation) {
    const auto* element = project.find_element(element_id);
    if (!element) {
        return std::nullopt;
    }

    std::optional<brep::Shape> shape;
    switch (element->kind) {
    case ElementKind::Wall: {
        const auto base_z = base_elevation_mm(project, *element);
        if (!base_z.has_value()) {
            return std::nullopt;
        }
        const auto height = wall_height_mm(project, *element, *base_z);
        if (!height.has_value()) {
            return std::nullopt;
        }
        shape = wall_shape_with_hosted_openings(
            project,
            element_id,
            *element,
            kernel,
            *base_z,
            *height);
        break;
    }
    case ElementKind::Slab: {
        const auto base_z = base_elevation_mm(project, *element);
        if (!base_z.has_value() || element->footprint.size() < 3 ||
            element->thickness_mm <= 0.0) {
            return std::nullopt;
        }
        shape = kernel.extrude(element->footprint, element->thickness_mm, *base_z);
        break;
    }
    case ElementKind::Door:
    case ElementKind::Window:
        shape = hosted_opening_panel_shape(project, element_id, *element, kernel);
        break;
    default:
        return std::nullopt;
    }

    if (!shape.has_value()) {
        return std::nullopt;
    }
    auto mesh = kernel.tessellate(*shape, tessellation);
    if (!mesh.has_value()) {
        return std::nullopt;
    }

    return DerivedBrepGeometry{
        element_id,
        element->kind,
        *shape,
        std::move(*mesh),
    };
}

} // namespace acp::bim
