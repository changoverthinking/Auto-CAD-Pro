#include "acp/snap.hpp"

#include "acp/annotation.hpp"
#include "acp/block.hpp"
#include "acp/document.hpp"
#include "acp/hatch.hpp"

#include <cmath>
#include <type_traits>
#include <variant>
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

std::optional<Candidate> pick_best(
    const std::vector<Candidate>& candidates,
    double aperture) noexcept {

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
    return best == nullptr
        ? std::nullopt
        : std::optional<Candidate>{*best};
}

void consider(
    std::optional<Candidate>& best,
    const std::optional<Candidate>& candidate) noexcept {

    if (!candidate.has_value()) {
        return;
    }
    if (!best.has_value() ||
        priority(candidate->kind) < priority(best->kind) ||
        (priority(candidate->kind) == priority(best->kind) &&
         candidate->distance_to_cursor < best->distance_to_cursor)) {
        best = candidate;
    }
}

void add_near_segment(
    std::vector<geo::Segment>& nearby,
    const geo::Segment& segment,
    geo::Vec2 cursor,
    double aperture) {

    const geo::Vec2 nearest = geo::nearest_point(segment, cursor);
    if (geo::distance(nearest, cursor) <= aperture) {
        nearby.push_back(segment);
    }
}

void consider_segment(
    std::optional<Candidate>& best,
    std::vector<geo::Segment>& nearby,
    const geo::Segment& segment,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) {

    consider(best, best_for_segment(
        segment, cursor, aperture, include_nearest));
    add_near_segment(nearby, segment, cursor, aperture);
}

void consider_polyline(
    std::optional<Candidate>& best,
    std::vector<geo::Segment>& nearby,
    const PolylineEntity& value,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) {

    if (value.points.size() < 2) {
        return;
    }
    for (std::size_t i = 1; i < value.points.size(); ++i) {
        consider_segment(
            best, nearby,
            {value.points[i - 1], value.points[i]},
            cursor, aperture, include_nearest);
    }
    if (value.closed && value.points.size() > 2) {
        consider_segment(
            best, nearby,
            {value.points.back(), value.points.front()},
            cursor, aperture, include_nearest);
    }
}

void consider_primitive(
    std::optional<Candidate>& best,
    std::vector<geo::Segment>& nearby,
    const BlockPrimitive& primitive,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) {

    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            consider_segment(
                best, nearby, value.segment,
                cursor, aperture, include_nearest);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            consider(best, best_for_circle(
                value.circle, cursor, aperture));
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            consider(best, best_for_arc(
                value.arc, cursor, aperture, include_nearest));
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            consider_polyline(
                best, nearby, value,
                cursor, aperture, include_nearest);
        }
    }, primitive);
}

void consider_entity(
    std::optional<Candidate>& best,
    std::vector<geo::Segment>& nearby,
    const Entity& entity,
    const BlockLibrary* blocks,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) {

    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            consider_segment(
                best, nearby, value.segment,
                cursor, aperture, include_nearest);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            consider(best, best_for_circle(
                value.circle, cursor, aperture));
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            consider(best, best_for_arc(
                value.arc, cursor, aperture, include_nearest));
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            consider_polyline(
                best, nearby, value,
                cursor, aperture, include_nearest);
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            if (blocks == nullptr) {
                return;
            }
            for (const auto& primitive : blocks->instantiate(value)) {
                consider_primitive(
                    best, nearby, primitive,
                    cursor, aperture, include_nearest);
            }
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            if (!annotation::valid_linear_dimension(value)) {
                return;
            }
            consider_segment(
                best, nearby, annotation::dimension_line(value),
                cursor, aperture, include_nearest);
            consider_segment(
                best, nearby, annotation::first_extension_line(value),
                cursor, aperture, include_nearest);
            consider_segment(
                best, nearby, annotation::second_extension_line(value),
                cursor, aperture, include_nearest);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            if (!hatch::valid(value)) {
                return;
            }
            consider_polyline(
                best, nearby,
                PolylineEntity{value.boundary, true},
                cursor, aperture, include_nearest);
        }
    }, entity);
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
        candidates.push_back({
            nearest, Kind::Nearest, geo::distance(nearest, cursor)});
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
        candidates.push_back({
            nearest, Kind::Nearest, geo::distance(nearest, cursor)});
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
    return pick_best({
        {*point, Kind::Intersection, geo::distance(*point, cursor)}
    }, aperture);
}

std::optional<Candidate> best_for_document(
    const Document& document,
    const BlockLibrary* blocks,
    geo::Vec2 cursor,
    double aperture,
    bool include_nearest) noexcept {

    if (!std::isfinite(aperture) || aperture < 0.0) {
        return std::nullopt;
    }

    std::optional<Candidate> best;
    std::vector<geo::Segment> nearby_segments;

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        if (entity == nullptr) {
            continue;
        }
        consider_entity(
            best, nearby_segments, *entity, blocks,
            cursor, aperture, include_nearest);
    }

    for (std::size_t i = 0; i < nearby_segments.size(); ++i) {
        for (std::size_t j = i + 1; j < nearby_segments.size(); ++j) {
            consider(best, intersection_for_segments(
                nearby_segments[i], nearby_segments[j],
                cursor, aperture));
        }
    }

    return best;
}

} // namespace acp::snap
