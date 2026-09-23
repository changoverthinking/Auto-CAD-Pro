#include "acp/architecture3d_advanced.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace acp::architecture3d {
namespace {

bool finite(double value) noexcept {
    return std::isfinite(value);
}

std::optional<geo::Vec2> far_endpoint(
    const WallSpec& wall,
    geo::Vec2 joint,
    double tolerance) noexcept {

    if (geo::distance(wall.start, joint) <= tolerance) {
        return wall.end;
    }
    if (geo::distance(wall.end, joint) <= tolerance) {
        return wall.start;
    }
    return std::nullopt;
}

bool copy_scene_objects(
    model3d::Scene& destination,
    const model3d::Scene& source,
    const std::string& prefix) {

    for (const model3d::ObjectId id : source.ids()) {
        const auto* object = source.find(id);
        if (object == nullptr) {
            return false;
        }
        const model3d::ObjectId inserted = destination.insert(
            object->mesh,
            prefix + object->name,
            object->color,
            object->kind,
            object->source_entity_id);
        if (inserted == 0) {
            return false;
        }
    }
    return true;
}

bool unique_level_elevations(const std::vector<Level>& levels) noexcept {
    for (std::size_t i = 0; i < levels.size(); ++i) {
        for (std::size_t j = i + 1; j < levels.size(); ++j) {
            if (std::abs(levels[i].elevation - levels[j].elevation) <= geo::kEpsilon) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

std::optional<WallJunctionMeshes> make_orthogonal_wall_junction(
    const std::vector<WallSpec>& walls,
    double endpoint_tolerance,
    double angular_tolerance) {

    const auto info = classify_wall_junction(
        walls, endpoint_tolerance, angular_tolerance);
    if (!info.has_value() ||
        (info->kind != WallJunctionKind::T && info->kind != WallJunctionKind::X)) {
        return std::nullopt;
    }

    double horizontal_half_thickness = 0.0;
    double vertical_half_thickness = 0.0;
    struct BranchInput {
        WallSpec wall;
        geo::Vec2 far{};
        geo::Vec2 direction{};
        bool horizontal{};
    };
    std::vector<BranchInput> inputs;
    inputs.reserve(walls.size());

    for (const WallSpec& wall : walls) {
        const auto far = far_endpoint(wall, info->joint, endpoint_tolerance);
        if (!far.has_value()) {
            return std::nullopt;
        }
        const geo::Vec2 delta = *far - info->joint;
        const double length = geo::length(delta);
        if (length <= geo::kEpsilon) {
            return std::nullopt;
        }
        const geo::Vec2 direction = delta * (1.0 / length);
        const bool horizontal =
            std::abs(direction.y) <= angular_tolerance &&
            std::abs(std::abs(direction.x) - 1.0) <= angular_tolerance;
        const bool vertical =
            std::abs(direction.x) <= angular_tolerance &&
            std::abs(std::abs(direction.y) - 1.0) <= angular_tolerance;
        if (!horizontal && !vertical) {
            return std::nullopt;
        }

        if (horizontal) {
            horizontal_half_thickness =
                std::max(horizontal_half_thickness, wall.thickness * 0.5);
        } else {
            vertical_half_thickness =
                std::max(vertical_half_thickness, wall.thickness * 0.5);
        }
        inputs.push_back(BranchInput{wall, *far, direction, horizontal});
    }

    if (horizontal_half_thickness <= geo::kEpsilon ||
        vertical_half_thickness <= geo::kEpsilon) {
        return std::nullopt;
    }

    const WallSpec& reference = walls.front();
    const std::vector<geo::Vec2> hub_boundary{
        {info->joint.x - vertical_half_thickness,
         info->joint.y - horizontal_half_thickness},
        {info->joint.x + vertical_half_thickness,
         info->joint.y - horizontal_half_thickness},
        {info->joint.x + vertical_half_thickness,
         info->joint.y + horizontal_half_thickness},
        {info->joint.x - vertical_half_thickness,
         info->joint.y + horizontal_half_thickness}
    };
    auto hub = model3d::extrude_polygon(
        hub_boundary,
        reference.height,
        reference.base_z);
    if (!hub.has_value()) {
        return std::nullopt;
    }

    WallJunctionMeshes result;
    result.kind = info->kind;
    result.hub = std::move(*hub);
    result.branches.reserve(inputs.size());

    for (const BranchInput& input : inputs) {
        const double trim_distance = input.horizontal
            ? vertical_half_thickness
            : horizontal_half_thickness;
        if (geo::distance(info->joint, input.far) <= trim_distance + geo::kEpsilon) {
            return std::nullopt;
        }
        const geo::Vec2 trimmed =
            info->joint + input.direction * trim_distance;
        WallSpec branch{
            trimmed,
            input.far,
            input.wall.thickness,
            input.wall.height,
            input.wall.base_z};
        auto mesh = make_wall(branch);
        if (!mesh.has_value()) {
            return std::nullopt;
        }
        result.branches.push_back(std::move(*mesh));
    }

    return result;
}

model3d::Scene build_multistorey_scene(
    const Document& document,
    const std::vector<Level>& levels,
    const MultiStoreyOptions& options) {

    model3d::Scene result;
    if (!valid_levels(levels) || levels.size() < 2 ||
        !unique_level_elevations(levels) ||
        !finite(options.wall_thickness) ||
        options.wall_thickness <= geo::kEpsilon ||
        !finite(options.slab_thickness) ||
        options.slab_thickness <= geo::kEpsilon) {
        return result;
    }

    std::vector<Level> sorted = levels;
    std::sort(
        sorted.begin(), sorted.end(),
        [](const Level& a, const Level& b) {
            return a.elevation < b.elevation;
        });

    for (std::size_t i = 0; i + 1 < sorted.size(); ++i) {
        const Level& base = sorted[i];
        const auto story = walls_for_story(
            document, sorted, base.name, options.wall_thickness);
        if (!copy_scene_objects(
                result, story, base.name + "::")) {
            return {};
        }

        if (options.include_slabs) {
            const auto slab = slabs_at_level(
                document, sorted, base.name, options.slab_thickness);
            if (!copy_scene_objects(
                    result, slab, base.name + "::")) {
                return {};
            }
        }
    }

    if (options.include_slabs && options.include_top_level_slab) {
        const Level& top = sorted.back();
        const auto slab = slabs_at_level(
            document, sorted, top.name, options.slab_thickness);
        if (!copy_scene_objects(
                result, slab, top.name + "::")) {
            return {};
        }
    }

    return result;
}

} // namespace acp::architecture3d
