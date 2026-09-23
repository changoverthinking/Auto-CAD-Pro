#include "acp/architecture3d_advanced.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <variant>

namespace acp::architecture3d {
namespace {

bool finite(double value) noexcept {
    return std::isfinite(value);
}

bool append_mesh(geo3d::Mesh& destination, const geo3d::Mesh& source) {
    if (!geo3d::valid_mesh(source)) {
        return false;
    }
    if (destination.vertices.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) -
            source.vertices.size()) {
        return false;
    }
    const auto offset = static_cast<std::uint32_t>(destination.vertices.size());
    destination.vertices.insert(
        destination.vertices.end(), source.vertices.begin(), source.vertices.end());
    for (const geo3d::Triangle triangle : source.triangles) {
        destination.triangles.push_back({
            triangle.a + offset,
            triangle.b + offset,
            triangle.c + offset});
    }
    return true;
}

std::optional<geo::Vec2> line_intersection(
    geo::Vec2 point_a,
    geo::Vec2 direction_a,
    geo::Vec2 point_b,
    geo::Vec2 direction_b) noexcept {

    const double denominator = geo::cross(direction_a, direction_b);
    if (!finite(denominator) || std::abs(denominator) <= geo::kEpsilon) {
        return std::nullopt;
    }
    const geo::Vec2 delta = point_b - point_a;
    const double t = geo::cross(delta, direction_b) / denominator;
    return point_a + direction_a * t;
}

struct OrientedWall {
    geo::Vec2 joint{};
    geo::Vec2 far{};
    double thickness{};
    double height{};
    double base_z{};
};

std::optional<OrientedWall> orient_to_joint(
    const WallSpec& wall,
    geo::Vec2 joint,
    double tolerance) noexcept {

    if (!valid(wall) || !finite(tolerance) || tolerance < 0.0) {
        return std::nullopt;
    }
    if (geo::distance(wall.start, joint) <= tolerance) {
        return OrientedWall{joint, wall.end, wall.thickness, wall.height, wall.base_z};
    }
    if (geo::distance(wall.end, joint) <= tolerance) {
        return OrientedWall{joint, wall.start, wall.thickness, wall.height, wall.base_z};
    }
    return std::nullopt;
}

std::optional<geo::Vec2> shared_endpoint(
    const WallSpec& first,
    const WallSpec& second,
    double tolerance) noexcept {

    const geo::Vec2 candidates_first[]{first.start, first.end};
    const geo::Vec2 candidates_second[]{second.start, second.end};
    for (const geo::Vec2 a : candidates_first) {
        for (const geo::Vec2 b : candidates_second) {
            if (geo::distance(a, b) <= tolerance) {
                return (a + b) * 0.5;
            }
        }
    }
    return std::nullopt;
}

std::optional<geo3d::Mesh> wall_from_miter_polygon(
    const OrientedWall& wall,
    geo::Vec2 plus_joint,
    geo::Vec2 minus_joint) {

    const geo::Vec2 delta = wall.far - wall.joint;
    const double length = geo::length(delta);
    if (length <= geo::kEpsilon) {
        return std::nullopt;
    }
    const geo::Vec2 tangent = delta * (1.0 / length);
    const geo::Vec2 normal{-tangent.y, tangent.x};
    const geo::Vec2 half_offset = normal * (wall.thickness * 0.5);
    const std::vector<geo::Vec2> polygon{
        plus_joint,
        wall.far + half_offset,
        wall.far - half_offset,
        minus_joint
    };
    return model3d::extrude_polygon(polygon, wall.height, wall.base_z);
}

} // namespace

std::optional<geo3d::Mesh> make_opening_frame(
    const WallOpeningSpec& opening,
    double profile_width,
    double frame_depth) {

    if (!valid(opening) ||
        !finite(profile_width) || profile_width <= geo::kEpsilon ||
        !finite(frame_depth) || frame_depth <= geo::kEpsilon ||
        profile_width * 2.0 >= opening.width - geo::kEpsilon ||
        profile_width * 2.0 >= opening.height - geo::kEpsilon) {
        return std::nullopt;
    }

    const geo::Vec2 delta = opening.wall.end - opening.wall.start;
    const double wall_length = geo::length(delta);
    const geo::Vec2 tangent = delta * (1.0 / wall_length);
    const geo::Vec2 center = opening.wall.start +
        tangent * (opening.center_offset * wall_length);
    const double direction = opening.wall.height >= 0.0 ? 1.0 : -1.0;
    const double base_z = opening.wall.base_z + direction * opening.sill_height;

    auto make_vertical = [&](double offset) {
        const geo::Vec2 rail_center = center + tangent * offset;
        const geo::Vec2 half_span = tangent * (profile_width * 0.5);
        return model3d::extrude_polygon(
            {
                rail_center - half_span + geo::Vec2{0.0, 0.0},
                rail_center + half_span + geo::Vec2{0.0, 0.0},
                rail_center + half_span + geo::Vec2{0.0, 0.0},
                rail_center - half_span + geo::Vec2{0.0, 0.0}
            },
            direction * opening.height,
            base_z);
    };

    // Use short wall-aligned segments so depth follows the host wall normal.
    auto rail_mesh = [&](double center_offset_along,
                         double rail_width,
                         double rail_height,
                         double local_base) -> std::optional<geo3d::Mesh> {
        const geo::Vec2 rail_center = center + tangent * center_offset_along;
        const geo::Vec2 half = tangent * (rail_width * 0.5);
        const geo::Vec2 normal{-tangent.y, tangent.x};
        const geo::Vec2 depth = normal * (frame_depth * 0.5);
        return model3d::extrude_polygon(
            {
                rail_center - half - depth,
                rail_center + half - depth,
                rail_center + half + depth,
                rail_center - half + depth
            },
            direction * rail_height,
            base_z + direction * local_base);
    };

    geo3d::Mesh result;
    const double side_offset = opening.width * 0.5 - profile_width * 0.5;
    const auto left = rail_mesh(-side_offset, profile_width, opening.height, 0.0);
    const auto right = rail_mesh(side_offset, profile_width, opening.height, 0.0);
    const auto top = rail_mesh(
        0.0,
        opening.width - 2.0 * profile_width,
        profile_width,
        opening.height - profile_width);
    if (!left.has_value() || !right.has_value() || !top.has_value() ||
        !append_mesh(result, *left) ||
        !append_mesh(result, *right) ||
        !append_mesh(result, *top)) {
        return std::nullopt;
    }

    if (opening.kind == OpeningKind::Window) {
        const auto bottom = rail_mesh(
            0.0,
            opening.width - 2.0 * profile_width,
            profile_width,
            0.0);
        if (!bottom.has_value() || !append_mesh(result, *bottom)) {
            return std::nullopt;
        }
    }

    return geo3d::valid_mesh(result)
        ? std::optional<geo3d::Mesh>{std::move(result)}
        : std::nullopt;
}

std::optional<WallJoinMeshes> make_mitered_wall_pair(
    const WallSpec& first,
    const WallSpec& second,
    double endpoint_tolerance) {

    if (!valid(first) || !valid(second) ||
        !finite(endpoint_tolerance) || endpoint_tolerance < 0.0 ||
        std::abs(first.base_z - second.base_z) > endpoint_tolerance ||
        std::abs(first.height - second.height) > endpoint_tolerance) {
        return std::nullopt;
    }

    const auto joint = shared_endpoint(first, second, endpoint_tolerance);
    if (!joint.has_value()) {
        return std::nullopt;
    }
    const auto a = orient_to_joint(first, *joint, endpoint_tolerance);
    const auto b = orient_to_joint(second, *joint, endpoint_tolerance);
    if (!a.has_value() || !b.has_value()) {
        return std::nullopt;
    }

    const geo::Vec2 da_raw = a->far - a->joint;
    const geo::Vec2 db_raw = b->far - b->joint;
    const double la = geo::length(da_raw);
    const double lb = geo::length(db_raw);
    if (la <= geo::kEpsilon || lb <= geo::kEpsilon) {
        return std::nullopt;
    }
    const geo::Vec2 da = da_raw * (1.0 / la);
    const geo::Vec2 db = db_raw * (1.0 / lb);
    if (std::abs(geo::cross(da, db)) <= 1e-8) {
        return std::nullopt;
    }

    const geo::Vec2 na{-da.y, da.x};
    const geo::Vec2 nb{-db.y, db.x};
    const auto plus = line_intersection(
        *joint + na * (a->thickness * 0.5), da,
        *joint + nb * (b->thickness * 0.5), db);
    const auto minus = line_intersection(
        *joint - na * (a->thickness * 0.5), da,
        *joint - nb * (b->thickness * 0.5), db);
    if (!plus.has_value() || !minus.has_value()) {
        return std::nullopt;
    }

    auto first_mesh = wall_from_miter_polygon(*a, *plus, *minus);
    auto second_mesh = wall_from_miter_polygon(*b, *plus, *minus);
    if (!first_mesh.has_value() || !second_mesh.has_value()) {
        return std::nullopt;
    }
    return WallJoinMeshes{std::move(*first_mesh), std::move(*second_mesh)};
}

std::optional<StorySpan> story_span(
    const std::vector<Level>& levels,
    std::string_view base_level_name) noexcept {

    if (!valid_levels(levels)) {
        return std::nullopt;
    }
    const auto base = level_elevation(levels, base_level_name);
    if (!base.has_value()) {
        return std::nullopt;
    }
    std::optional<double> next;
    for (const Level& level : levels) {
        if (level.elevation > *base + geo::kEpsilon &&
            (!next.has_value() || level.elevation < *next)) {
            next = level.elevation;
        }
    }
    if (!next.has_value()) {
        return std::nullopt;
    }
    return StorySpan{*base, *next};
}

model3d::Scene walls_for_story(
    const Document& document,
    const std::vector<Level>& levels,
    std::string_view base_level_name,
    double thickness) {

    const auto span = story_span(levels, base_level_name);
    if (!span.has_value()) {
        return {};
    }
    return walls_from_lines(document, thickness, span->height(), span->base_z);
}

model3d::Scene slabs_at_level(
    const Document& document,
    const std::vector<Level>& levels,
    std::string_view level_name,
    double thickness) {

    const auto elevation = level_elevation(levels, level_name);
    if (!elevation.has_value()) {
        return {};
    }
    return slabs_from_closed_polylines(document, thickness, *elevation);
}

} // namespace acp::architecture3d
