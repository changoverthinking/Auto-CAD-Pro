#include "acp/block.hpp"

#include "acp/transform.hpp"

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
    const BlockInstance& instance) {

    std::visit([&](auto& value) {
        using T = std::decay_t<decltype(value)>;

        const auto transform_point = [&](geo::Vec2 point) {
            geo::Vec2 result = base_point + (point - base_point) * instance.scale;
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
            value.arc.start_angle += instance.rotation;
            value.arc.end_angle += instance.rotation;
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

    while (blocks_.contains(next_id_)) {
        ++next_id_;
    }

    const BlockId id = next_id_++;
    blocks_.emplace(id, BlockDefinition{id, std::move(name), base_point, std::move(geometry)});
    return id;
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

std::vector<BlockPrimitive> BlockLibrary::instantiate(const BlockInstance& instance) const {
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
