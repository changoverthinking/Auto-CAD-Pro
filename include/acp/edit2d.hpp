#pragma once

#include "acp/geometry2d.hpp"

#include <optional>

namespace acp::edit2d {

struct LineIntersection {
    geo::Vec2 point{};
    double lhs_parameter{};
    double rhs_parameter{};
};

[[nodiscard]] std::optional<LineIntersection> infinite_line_intersection(
    geo::Segment lhs,
    geo::Segment rhs,
    double eps = geo::kEpsilon) noexcept;

[[nodiscard]] std::optional<geo::Segment> offset_segment(
    geo::Segment source,
    double distance) noexcept;

bool trim_segment(
    geo::Segment& target,
    geo::Segment cutter,
    geo::Vec2 picked_side,
    double eps = geo::kEpsilon) noexcept;

bool extend_segment(
    geo::Segment& target,
    geo::Segment boundary,
    double eps = geo::kEpsilon) noexcept;

} // namespace acp::edit2d
