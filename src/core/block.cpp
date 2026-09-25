#include "acp/block.hpp"

#include "acp/transform.hpp"
#include "acp/id_allocation.hpp"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>
#include <variant>

namespace acp {

namespace {

void transform_primitive(
    BlockPrimitive& primitive,
    geo::Vec2 base_point,
    const BlockReferenceEntity& instance) {

    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;

        const auto transform_point = [&](geo::Vec2 point) {
            auto local = (point - base_point) * instance.scale;
            if (instance.mirrored) local.y = -local.y;
            geo::Vec2 result = base_point + local;
            result = transform::rotate_point(result, base_point, instance.rotation);
            return instance.insertion_point + (result - base_point);
        };

        if constexpr (std::is_same_v<T, LineEntity>) {
            value.segment.a = transform_point(value.segment.a);
            value.segment.b = transform_point(value.segment.b);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            value.circle.center = transform_point(value.circle.center);
            value.circle.radius *= instance.scale;
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            value.arc.center = transform_point(value.arc.center);
            value.arc.radius *= instance.scale;
            const double orientation = instance.mirrored ? -1.0 : 1.0;
            value.arc.start_angle = orientation * value.arc.start_angle + instance.rotation;
            value.arc.end_angle = orientation * value.arc.end_angle + instance.rotation;
            if (instance.mirrored) value.arc.counter_clockwise = !value.arc.counter_clockwise;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            for (auto& point : value.points) {
                point = transform_point(point);
            }
        }
    }, primitive);
}

} // namespace

BlockId BlockLibrary::create(
    std::string name,
    geo::Vec2 base_point,
    std::vector<BlockPrimitive> geometry) {

    if (name.empty() || name_exists(name) || geometry.empty()) {
        return 0;
    }

    const BlockId id = detail::allocate_id(blocks_, next_id_);
    if (id == 0) return 0;
    blocks_.emplace(id, BlockDefinition{id, std::move(name), base_point, std::move(geometry)});
    return id;
}

bool BlockLibrary::insert_with_id(BlockDefinition definition) {
    if (definition.id == 0 ||
        definition.name.empty() ||
        definition.geometry.empty() ||
        blocks_.contains(definition.id) ||
        name_exists(definition.name)) {
        return false;
    }

    const BlockId id = definition.id;
    blocks_.emplace(id, std::move(definition));
    if (id >= next_id_) {
        next_id_ = id + 1;
    }
    return true;
}

const BlockDefinition* BlockLibrary::find(BlockId id) const noexcept {
    const auto it = blocks_.find(id);
    return it == blocks_.end() ? nullptr : &it->second;
}

BlockDefinition* BlockLibrary::find(BlockId id) noexcept {
    const auto it = blocks_.find(id);
    return it == blocks_.end() ? nullptr : &it->second;
}

std::vector<BlockId> BlockLibrary::ids() const {
    std::vector<BlockId> result;
    result.reserve(blocks_.size());
    for (const auto& entry : blocks_) {
        result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool BlockLibrary::rename(BlockId id, std::string name) {
    auto* block = find(id);
    if (block == nullptr || name.empty() || name_exists(name, id)) {
        return false;
    }
    block->name = std::move(name);
    return true;
}

bool BlockLibrary::remove(BlockId id) {
    return blocks_.erase(id) == 1;
}

std::vector<BlockPrimitive> BlockLibrary::instantiate(const BlockReferenceEntity& instance) const {
    const auto* block = find(instance.block_id);
    if (block == nullptr ||
        !std::isfinite(instance.scale) ||
        instance.scale <= geo::kEpsilon ||
        !std::isfinite(instance.rotation)) {
        return {};
    }

    auto result = block->geometry;
    for (auto& primitive : result) {
        transform_primitive(primitive, block->base_point, instance);
    }
    return result;
}

bool BlockLibrary::name_exists(const std::string& name, BlockId ignore_id) const {
    for (const auto& entry : blocks_) {
        if (entry.first != ignore_id && entry.second.name == name) {
            return true;
        }
    }
    return false;
}

} // namespace acp
