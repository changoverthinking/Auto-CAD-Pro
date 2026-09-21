#pragma once

#include "acp/geometry2d.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace acp {

struct HatchEntity {
    std::vector<geo::Vec2> boundary;
    std::string pattern{"ANSI31"};
    double angle{};
    double spacing{1.0};
    bool solid{false};
};

namespace hatch {

[[nodiscard]] bool valid(const HatchEntity& hatch) noexcept;
[[nodiscard]] double perimeter(const HatchEntity& hatch) noexcept;
[[nodiscard]] std::vector<geo::Segment> pattern_segments(
    const HatchEntity& hatch,
    std::size_t max_segments = 20000);

} // namespace hatch
} // namespace acp
