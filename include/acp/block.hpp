#pragma once

#include "acp/document.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace acp {

using BlockId = std::uint32_t;
using BlockPrimitive = std::variant<LineEntity, CircleEntity, ArcEntity, PolylineEntity>;

struct BlockDefinition {
    BlockId id{};
    std::string name;
    geo::Vec2 base_point{};
    std::vector<BlockPrimitive> geometry;
};

struct BlockInstance {
    BlockId block_id{};
    geo::Vec2 insertion_point{};
    double rotation{};
    double scale{1.0};
};

class BlockLibrary {
public:
    [[nodiscard]] BlockId create(
        std::string name,
        geo::Vec2 base_point,
        std::vector<BlockPrimitive> geometry);

    [[nodiscard]] const BlockDefinition* find(BlockId id) const noexcept;
    [[nodiscard]] BlockDefinition* find(BlockId id) noexcept;
    [[nodiscard]] std::vector<BlockId> ids() const;

    bool rename(BlockId id, std::string name);
    [[nodiscard]] bool remove(BlockId id);

    [[nodiscard]] std::vector<BlockPrimitive> instantiate(const BlockInstance& instance) const;

private:
    [[nodiscard]] bool name_exists(const std::string& name, BlockId ignore_id = 0) const;

    BlockId next_id_{1};
    std::unordered_map<BlockId, BlockDefinition> blocks_;
};

} // namespace acp
