#pragma once

#include "acp/geometry2d.hpp"

#include <cstdint>

namespace acp {

using BlockId = std::uint32_t;

struct BlockReferenceEntity {
    BlockId block_id{};
    geo::Vec2 insertion_point{};
    double rotation{};
    double scale{1.0};
};

} // namespace acp
