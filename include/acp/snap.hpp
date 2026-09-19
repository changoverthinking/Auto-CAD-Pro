#pragma once

#include "acp/geometry2d.hpp"

#include <optional>
#include <vector>

namespace acp::snap {

enum class Kind {
    Endpoint,
    Intersection,
    Midpoint,
    Center,
    Nearest
};

struct Candidate {
    geo::Vec2 point;
    Kind kind;
    double distance_to_cursor{};
};

[[nodiscard]] std::optional<Candidate> best_for_segment(
    const geo::Segment& segment,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest = true) noexcept;

[[nodiscard]] std::optional<Candidate> best_for_circle(
    const geo::Circle& circle,
    geo::Vec2 cursor,
    double aperture) noexcept;

[[nodiscard]] std::optional<Candidate> best_for_arc(
    const geo::Arc& arc,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest = true) noexcept;

[[nodiscard]] std::optional<Candidate> intersection_for_segments(
    const geo::Segment& lhs,
    const geo::Segment& rhs,
    geo::Vec2 cursor,
    double aperture) noexcept;

} // namespace acp::snap
