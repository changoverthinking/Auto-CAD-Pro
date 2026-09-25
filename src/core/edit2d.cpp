#include "acp/edit2d.hpp"

#include <algorithm>
#include <cmath>

namespace acp::edit2d {

std::optional<LineIntersection> infinite_line_intersection(
    geo::Segment lhs,
    geo::Segment rhs,
    double eps) noexcept {

    const geo::Vec2 r = lhs.b - lhs.a;
    const geo::Vec2 s = rhs.b - rhs.a;
    const double lhs_len_sq = geo::dot(r, r);
    const double rhs_len_sq = geo::dot(s, s);

    if (lhs_len_sq <= eps || rhs_len_sq <= eps) {
        return std::nullopt;
    }

    const double denom = geo::cross(r, s);
    if (std::abs(denom) <= eps) {
        return std::nullopt;
    }

    const geo::Vec2 qp = rhs.a - lhs.a;
    const double t = geo::cross(qp, s) / denom;
    const double u = geo::cross(qp, r) / denom;

    return LineIntersection{lhs.a + r * t, t, u};
}

std::optional<geo::Segment> offset_segment(
    geo::Segment source,
    double distance) noexcept {

    if (!std::isfinite(distance)) {
        return std::nullopt;
    }

    const geo::Vec2 direction = source.b - source.a;
    const double len = geo::length(direction);
    if (len <= geo::kEpsilon) {
        return std::nullopt;
    }

    const geo::Vec2 normal{-direction.y / len, direction.x / len};
    const geo::Vec2 delta = normal * distance;
    return geo::Segment{source.a + delta, source.b + delta};
}

std::optional<geo::Circle> offset_circle(geo::Circle source, double distance) noexcept {
    const double radius = source.radius + distance;
    if (!std::isfinite(source.center.x) || !std::isfinite(source.center.y) ||
        !std::isfinite(source.radius) || source.radius <= geo::kEpsilon ||
        !std::isfinite(distance) || !std::isfinite(radius) || radius <= geo::kEpsilon) return std::nullopt;
    source.radius = radius;
    return source;
}

std::optional<geo::Arc> offset_arc(geo::Arc source, double distance) noexcept {
    if (!geo::valid_arc(source)) return std::nullopt;
    const auto circle = offset_circle({source.center,source.radius},distance);
    if (!circle) return std::nullopt;
    source.radius = circle->radius;
    return source;
}

std::optional<Entity> offset_through_point(const Entity& source, geo::Vec2 point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return std::nullopt;
    if (const auto* line = std::get_if<LineEntity>(&source)) {
        const auto direction = line->segment.b - line->segment.a;
        const double length = geo::length(direction);
        if (!std::isfinite(length) || length <= geo::kEpsilon) return std::nullopt;
        const auto result = offset_segment(line->segment, geo::cross(direction,point-line->segment.a)/length);
        if (result) return LineEntity{*result};
    } else if (const auto* circle = std::get_if<CircleEntity>(&source)) {
        const auto result = offset_circle(circle->circle,geo::distance(point,circle->circle.center)-circle->circle.radius);
        if (result) return CircleEntity{*result};
    } else if (const auto* arc = std::get_if<ArcEntity>(&source)) {
        const auto result = offset_arc(arc->arc,geo::distance(point,arc->arc.center)-arc->arc.radius);
        if (result) return ArcEntity{*result};
    }
    return std::nullopt;
}

bool trim_segment(
    geo::Segment& target,
    geo::Segment cutter,
    geo::Vec2 picked_side,
    double eps) noexcept {

    const auto hit = infinite_line_intersection(target, cutter, eps);
    if (!hit.has_value()) {
        return false;
    }

    if (hit->lhs_parameter <= eps || hit->lhs_parameter >= 1.0 - eps ||
        hit->rhs_parameter < -eps || hit->rhs_parameter > 1.0 + eps) {
        return false;
    }

    const geo::Vec2 direction = target.b - target.a;
    const double denom = geo::dot(direction, direction);
    if (denom <= eps) {
        return false;
    }

    const double picked_parameter =
        geo::dot(picked_side - target.a, direction) / denom;

    if (picked_parameter < hit->lhs_parameter) {
        target.a = hit->point;
    } else {
        target.b = hit->point;
    }

    return true;
}

bool extend_segment(
    geo::Segment& target,
    geo::Segment boundary,
    double eps) noexcept {

    const auto hit = infinite_line_intersection(target, boundary, eps);
    if (!hit.has_value()) {
        return false;
    }

    if (hit->rhs_parameter < -eps || hit->rhs_parameter > 1.0 + eps) {
        return false;
    }

    if (hit->lhs_parameter < -eps) {
        target.a = hit->point;
        return true;
    }

    if (hit->lhs_parameter > 1.0 + eps) {
        target.b = hit->point;
        return true;
    }

    return false;
}

} // namespace acp::edit2d
