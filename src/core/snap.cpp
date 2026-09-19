#include "acp/snap.hpp"

#include <vector>

namespace acp::snap {

namespace {

int priority(Kind kind) noexcept {
    switch (kind) {
        case Kind::Endpoint: return 0;
        case Kind::Intersection: return 0;
        case Kind::Midpoint: return 1;
        case Kind::Center: return 1;
        case Kind::Nearest: return 10;
    }
    return 100;
}

std::optional<Candidate> pick_best(const std::vector<Candidate>& candidates, double aperture) noexcept {
    const Candidate* best = nullptr;
    for (const auto& candidate : candidates) {
        if (candidate.distance_to_cursor > aperture) continue;
        if (best == nullptr ||
            priority(candidate.kind) < priority(best->kind) ||
            (priority(candidate.kind) == priority(best->kind) &&
             candidate.distance_to_cursor < best->distance_to_cursor)) {
            best = &candidate;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<Candidate>{*best};
}

} // namespace

std::optional<Candidate> best_for_segment(
    const geo::Segment& segment,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) noexcept {
    const auto middle = geo::midpoint(segment);
    std::vector<Candidate> candidates{
        {segment.a, Kind::Endpoint, geo::distance(segment.a, cursor)},
        {segment.b, Kind::Endpoint, geo::distance(segment.b, cursor)},
        {middle, Kind::Midpoint, geo::distance(middle, cursor)}
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
    return pick_best({
        {circle.center, Kind::Center, geo::distance(circle.center, cursor)}
    }, aperture);
}

std::optional<Candidate> best_for_arc(
    const geo::Arc& arc,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) noexcept {
    if (!geo::valid_arc(arc)) return std::nullopt;

    const auto start = geo::arc_start_point(arc);
    const auto end = geo::arc_end_point(arc);
    std::vector<Candidate> candidates{
        {start, Kind::Endpoint, geo::distance(start, cursor)},
        {end, Kind::Endpoint, geo::distance(end, cursor)},
        {arc.center, Kind::Center, geo::distance(arc.center, cursor)}
    };
    if (include_nearest) {
        const auto nearest = geo::nearest_point(arc, cursor);
        candidates.push_back({nearest, Kind::Nearest, geo::distance(nearest, cursor)});
    }
    return pick_best(candidates, aperture);
}

std::optional<Candidate> intersection_for_segments(
    const geo::Segment& lhs,
    const geo::Segment& rhs,
    geo::Vec2 cursor,
    double aperture) noexcept {
    const auto point = geo::segment_intersection(lhs, rhs);
    if (!point.has_value()) return std::nullopt;
    return pick_best({{*point, Kind::Intersection, geo::distance(*point, cursor)}}, aperture);
}

} // namespace acp::snap
