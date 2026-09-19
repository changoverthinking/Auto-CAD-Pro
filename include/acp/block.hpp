#pragma once

#include "acp/document.hpp"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace acp {

using BlockPrimitive = std::variant<LineEntity, CircleEntity, ArcEntity, PolylineEntity>;

struct BlockDefinition {
    BlockId id{};
    std::string name;
    geo::Vec2 base_point{};
    std::vector<BlockPrimitive> geometry;
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

    [[nodiscard]] std::vector<BlockPrimitive> instantiate(const BlockReferenceEntity& instance) const;

private:
    [[nodiscard]] bool name_exists(const std::string& name, BlockId ignore_id = 0) const;

    BlockId next_id_{1};
    std::unordered_map<BlockId, BlockDefinition> blocks_;
};

} // namespace acp
