#include "acp/architecture3d.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <variant>

namespace acp::architecture3d {
namespace {

bool finite(double value) noexcept {
    return std::isfinite(value);
}

std::vector<geo::Vec2> rectangle_around_segment(
    geo::Vec2 start,
    geo::Vec2 end,
    double thickness) {

    const geo::Vec2 delta = end - start;
    const double length = geo::length(delta);
    if (length <= geo::kEpsilon) {
        return {};
    }

    const geo::Vec2 normal{-delta.y / length, delta.x / length};
    const geo::Vec2 offset = normal * (thickness * 0.5);

    return {
        start + offset,
        end + offset,
        end - offset,
        start - offset
    };
}

std::vector<geo::Vec2> rotated_rectangle(
    geo::Vec2 center,
    double width,
    double depth,
    double rotation) {

    const double half_w = width * 0.5;
    const double half_d = depth * 0.5;
    const double c = std::cos(rotation);
    const double s = std::sin(rotation);

    auto transform = [&](geo::Vec2 local) {
        return geo::Vec2{
            center.x + local.x * c - local.y * s,
            center.y + local.x * s + local.y * c
        };
    };

    return {
        transform({-half_w, -half_d}),
        transform({ half_w, -half_d}),
        transform({ half_w,  half_d}),
        transform({-half_w,  half_d})
    };
}

std::vector<geo::Vec2> opening_rectangle(const WallOpeningSpec& opening) {
    const geo::Vec2 delta = opening.wall.end - opening.wall.start;
    const double length = geo::length(delta);
    if (length <= geo::kEpsilon) {
        return {};
    }

    const geo::Vec2 tangent = delta * (1.0 / length);
    const geo::Vec2 normal{-tangent.y, tangent.x};
    const geo::Vec2 center =
        opening.wall.start + delta * opening.center_offset;
    const geo::Vec2 half_along = tangent * (opening.width * 0.5);
    const double cut_thickness =
        opening.wall.thickness + opening.cut_clearance * 2.0;
    const geo::Vec2 half_across = normal * (cut_thickness * 0.5);

    return {
        center - half_along - half_across,
        center + half_along - half_across,
        center + half_along + half_across,
        center - half_along + half_across
    };
}

} // namespace

bool valid(const Level& level) noexcept {
    return !level.name.empty() && finite(level.elevation);
}

bool valid_levels(const std::vector<Level>& levels) noexcept {
    std::unordered_set<std::string> names;
    names.reserve(levels.size());
    for (const Level& level : levels) {
        if (!valid(level) || !names.insert(level.name).second) {
            return false;
        }
    }
    return true;
}

std::optional<double> level_elevation(
    const std::vector<Level>& levels,
    std::string_view name) noexcept {

    for (const Level& level : levels) {
        if (level.name == name && valid(level)) {
            return level.elevation;
        }
    }
    return std::nullopt;
}

bool valid(const WallSpec& wall) noexcept {
    return finite(wall.start.x) && finite(wall.start.y) &&
           finite(wall.end.x) && finite(wall.end.y) &&
           finite(wall.thickness) && wall.thickness > geo::kEpsilon &&
           finite(wall.height) && std::abs(wall.height) > geo::kEpsilon &&
           finite(wall.base_z) &&
           geo::distance(wall.start, wall.end) > geo::kEpsilon;
}

bool valid(const SlabSpec& slab) noexcept {
    if (!finite(slab.thickness) || slab.thickness <= geo::kEpsilon ||
        !finite(slab.top_z) || slab.boundary.size() < 3) {
        return false;
    }
    for (const geo::Vec2 point : slab.boundary) {
        if (!finite(point.x) || !finite(point.y)) {
            return false;
        }
    }
    return true;
}

bool valid(const ColumnSpec& column) noexcept {
    return finite(column.center.x) && finite(column.center.y) &&
           finite(column.width) && column.width > geo::kEpsilon &&
           finite(column.depth) && column.depth > geo::kEpsilon &&
           finite(column.height) && std::abs(column.height) > geo::kEpsilon &&
           finite(column.base_z) && finite(column.rotation);
}

bool valid(const BeamSpec& beam) noexcept {
    return finite(beam.start.x) && finite(beam.start.y) &&
           finite(beam.end.x) && finite(beam.end.y) &&
           geo::distance(beam.start, beam.end) > geo::kEpsilon &&
           finite(beam.width) && beam.width > geo::kEpsilon &&
           finite(beam.depth) && beam.depth > geo::kEpsilon &&
           finite(beam.top_z);
}

bool valid(const WallOpeningSpec& opening) noexcept {
    if (!valid(opening.wall) ||
        !finite(opening.center_offset) ||
        opening.center_offset < 0.0 || opening.center_offset > 1.0 ||
        !finite(opening.width) || opening.width <= geo::kEpsilon ||
        !finite(opening.height) || opening.height <= geo::kEpsilon ||
        !finite(opening.sill_height) || opening.sill_height < 0.0 ||
        !finite(opening.cut_clearance) || opening.cut_clearance < 0.0) {
        return false;
    }

    const double wall_length = geo::distance(opening.wall.start, opening.wall.end);
    const double center_distance = wall_length * opening.center_offset;
    const double half_width = opening.width * 0.5;
    const bool fits_length =
        center_distance - half_width >= -geo::kEpsilon &&
        center_distance + half_width <= wall_length + geo::kEpsilon;
    const bool fits_height =
        opening.sill_height + opening.height <=
        std::abs(opening.wall.height) + geo::kEpsilon;
    return fits_length && fits_height;
}

std::optional<geo3d::Mesh> make_wall(const WallSpec& wall) {
    if (!valid(wall)) {
        return std::nullopt;
    }
    return model3d::extrude_polygon(
        rectangle_around_segment(wall.start, wall.end, wall.thickness),
        wall.height,
        wall.base_z);
}

std::optional<geo3d::Mesh> make_slab(const SlabSpec& slab) {
    if (!valid(slab)) {
        return std::nullopt;
    }
    return model3d::extrude_polygon(
        slab.boundary,
        -slab.thickness,
        slab.top_z);
}

std::optional<geo3d::Mesh> make_column(const ColumnSpec& column) {
    if (!valid(column)) {
        return std::nullopt;
    }
    return model3d::extrude_polygon(
        rotated_rectangle(
            column.center,
            column.width,
            column.depth,
            column.rotation),
        column.height,
        column.base_z);
}

std::optional<geo3d::Mesh> make_beam(const BeamSpec& beam) {
    if (!valid(beam)) {
        return std::nullopt;
    }
    return model3d::extrude_polygon(
        rectangle_around_segment(beam.start, beam.end, beam.width),
        beam.depth,
        beam.top_z - beam.depth);
}

std::optional<geo3d::Mesh> make_wall_opening_volume(
    const WallOpeningSpec& opening) {

    if (!valid(opening)) {
        return std::nullopt;
    }
    const double direction = opening.wall.height >= 0.0 ? 1.0 : -1.0;
    const double base_z =
        opening.wall.base_z + direction * opening.sill_height;
    return model3d::extrude_polygon(
        opening_rectangle(opening),
        direction * opening.height,
        base_z);
}

model3d::Scene walls_from_lines(
    const Document& document,
    double thickness,
    double height,
    double base_z) {

    model3d::Scene scene;
    if (!finite(thickness) || thickness <= geo::kEpsilon ||
        !finite(height) || std::abs(height) <= geo::kEpsilon ||
        !finite(base_z)) {
        return scene;
    }

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        const auto* line = entity == nullptr
            ? nullptr
            : std::get_if<LineEntity>(entity);
        if (line == nullptr) {
            continue;
        }

        auto mesh = make_wall(WallSpec{
            line->segment.a,
            line->segment.b,
            thickness,
            height,
            base_z});
        if (!mesh.has_value()) {
            continue;
        }
        (void)scene.insert(
            std::move(*mesh),
            "Wall_" + std::to_string(id),
            document.effective_color(id),
            model3d::ObjectKind::Wall,
            id);
    }
    return scene;
}

model3d::Scene beams_from_lines(
    const Document& document,
    double width,
    double depth,
    double top_z) {

    model3d::Scene scene;
    if (!finite(width) || width <= geo::kEpsilon ||
        !finite(depth) || depth <= geo::kEpsilon || !finite(top_z)) {
        return scene;
    }

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        const auto* line = entity == nullptr
            ? nullptr
            : std::get_if<LineEntity>(entity);
        if (line == nullptr) {
            continue;
        }

        auto mesh = make_beam(BeamSpec{
            line->segment.a,
            line->segment.b,
            width,
            depth,
            top_z});
        if (!mesh.has_value()) {
            continue;
        }
        (void)scene.insert(
            std::move(*mesh),
            "Beam_" + std::to_string(id),
            document.effective_color(id),
            model3d::ObjectKind::Beam,
            id);
    }
    return scene;
}

model3d::Scene slabs_from_closed_polylines(
    const Document& document,
    double thickness,
    double top_z) {

    model3d::Scene scene;
    if (!finite(thickness) || thickness <= geo::kEpsilon || !finite(top_z)) {
        return scene;
    }

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        const auto* polyline = entity == nullptr
            ? nullptr
            : std::get_if<PolylineEntity>(entity);
        if (polyline == nullptr || !polyline->closed || polyline->points.size() < 3) {
            continue;
        }

        auto mesh = make_slab(SlabSpec{polyline->points, thickness, top_z});
        if (!mesh.has_value()) {
            continue;
        }
        (void)scene.insert(
            std::move(*mesh),
            "Slab_" + std::to_string(id),
            document.effective_color(id),
            model3d::ObjectKind::Slab,
            id);
    }
    return scene;
}

} // namespace acp::architecture3d
