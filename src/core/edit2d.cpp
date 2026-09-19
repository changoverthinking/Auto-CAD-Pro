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
