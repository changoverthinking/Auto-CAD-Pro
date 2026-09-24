#include "acp/bim_brep.hpp"

#include <cmath>
#include <vector>

namespace acp::bim {
namespace {

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

[[nodiscard]] std::optional<std::vector<geo::Vec2>> wall_profile(
    const Element& element) {
    if (!element.location_curve.has_value() || element.thickness_mm <= 0.0) {
        return std::nullopt;
    }

    const auto start = element.location_curve->start;
    const auto end = element.location_curve->end;
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double length = std::hypot(dx, dy);
    if (length <= 1e-9) {
        return std::nullopt;
    }

    const double half = element.thickness_mm * 0.5;
    const double nx = -dy / length * half;
    const double ny = dx / length * half;

    return std::vector<geo::Vec2>{
        {start.x + nx, start.y + ny},
        {end.x + nx, end.y + ny},
        {end.x - nx, end.y - ny},
        {start.x - nx, start.y - ny},
    };
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

    const auto base_z = base_elevation_mm(project, *element);
    if (!base_z.has_value()) {
        return std::nullopt;
    }

    std::optional<brep::Shape> shape;
    switch (element->kind) {
    case ElementKind::Wall: {
        const auto profile = wall_profile(*element);
        const auto height = wall_height_mm(project, *element, *base_z);
        if (!profile.has_value() || !height.has_value()) {
            return std::nullopt;
        }
        shape = kernel.extrude(*profile, *height, *base_z);
        break;
    }
    case ElementKind::Slab:
        if (element->footprint.size() < 3 || element->thickness_mm <= 0.0) {
            return std::nullopt;
        }
        shape = kernel.extrude(element->footprint, element->thickness_mm, *base_z);
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
