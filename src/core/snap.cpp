#include "acp/snap.hpp"

#include <algorithm>
#include <array>

namespace acp::snap {

namespace {

std::optional<Candidate> pick_best(const std::vector<Candidate>& candidates, double aperture) noexcept {
    const Candidate* best = nullptr;
    for (const auto& candidate : candidates) {
        if (candidate.distance_to_cursor > aperture) {
            continue;
        }
        if (best == nullptr || candidate.distance_to_cursor < best->distance_to_cursor) {
            best = &candidate;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return *best;
}

} // namespace

std::optional<Candidate> best_for_segment(
    const geo::Segment& segment,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) noexcept {

    std::vector<Candidate> candidates{
        {segment.a, Kind::Endpoint, geo::distance(segment.a, cursor)},
        {segment.b, Kind::Endpoint, geo::distance(segment.b, cursor)},
        {geo::midpoint(segment), Kind::Midpoint, geo::distance(geo::midpoint(segment), cursor)}
    };

    if (include_nearest) {
        const auto nearest = geo::nearest_point(segment, cursor);
        candidates.push_back({nearest, Kind::Nearest, geo::distance(nearest, cursor)});
    }

    return pick_best(candidates, aperture);
}

std::optional<Candidate> best_for_circle(
    const geo::Circle& circle,
    geo::Vec2 cursor,
    double aperture) noexcept {

    const double d = geo::distance(circle.center, cursor);
    return pick_best({
        {circle.center, Kind::Center, d}
    }, aperture);
}

} // namespace acp::snap
