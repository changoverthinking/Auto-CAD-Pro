#pragma once

#include "acp/geometry2d.hpp"

#include <optional>
#include <vector>

namespace acp::snap {

enum class Kind {
    Endpoint,
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

} // namespace acp::snap
