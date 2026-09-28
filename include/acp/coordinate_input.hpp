#pragma once

#include "acp/geometry2d.hpp"
#include <optional>
#include <string_view>

namespace acp::input {
// Millimeters; decimal point '.'; angles in degrees counterclockwise from +X.
// x,y is absolute. @dx,dy and @distance<angle require a reference point.
[[nodiscard]] std::optional<geo::Vec2> parse_coordinate(
    std::string_view text, std::optional<geo::Vec2> reference = std::nullopt) noexcept;
}
