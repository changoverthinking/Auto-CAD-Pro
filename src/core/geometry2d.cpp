#include "acp/geometry2d.hpp"

#include <algorithm>
#include <numbers>

namespace acp::geo {

namespace {
double normalize_positive_angle(double radians) noexcept {
    const double full = 2.0 * std::numbers::pi;
    double value = std::fmod(radians, full);
    if (value < 0.0) {
        value += full;
    }
    return value;
}
}

double dot(Vec2 a, Vec2 b) noexcept {
    return a.x * b.x + a.y * b.y;
}

double cross(Vec2 a, Vec2 b) noexcept {
    return a.x * b.y - a.y * b.x;
}

double length(Vec2 v) noexcept {
    return std::hypot(v.x, v.y);
}

double distance(Vec2 a, Vec2 b) noexcept {
    return length(a - b);
}

bool nearly_equal(double a, double b, double eps) noexcept {
    return std::abs(a - b) <= eps;
}

bool nearly_equal(Vec2 a, Vec2 b, double eps) noexcept {
    return distance(a, b) <= eps;
}

Vec2 midpoint(const Segment& s) noexcept {
    return {(s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5};
}

Vec2 nearest_point(const Segment& s, Vec2 p) noexcept {
    const Vec2 ab = s.b - s.a;
    const double denom = dot(ab, ab);
    if (denom <= kEpsilon) {
        return s.a;
    }

    const double t = std::clamp(dot(p - s.a, ab) / denom, 0.0, 1.0);
    return s.a + ab * t;
}

std::optional<Vec2> segment_intersection(const Segment& lhs, const Segment& rhs, double eps) noexcept {
    const Vec2 r = lhs.b - lhs.a;
    const Vec2 s = rhs.b - rhs.a;
    const double denom = cross(r, s);
    const Vec2 qp = rhs.a - lhs.a;

    if (std::abs(denom) <= eps) {
        return std::nullopt;
    }

    const double t = cross(qp, s) / denom;
    const double u = cross(qp, r) / denom;

    if (t < -eps || t > 1.0 + eps || u < -eps || u > 1.0 + eps) {
        return std::nullopt;
    }

    return lhs.a + r * t;
}

double polyline_length(const std::vector<Vec2>& points) noexcept {
    return polyline_length(points, false);
}

double polyline_length(const std::vector<Vec2>& points, bool closed) noexcept {
    if (points.empty()) {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        total += distance(points[i - 1], points[i]);
    }

    if (closed && points.size() > 2) {
        total += distance(points.back(), points.front());
    }

    return total;
}

bool valid_arc(const Arc& arc) noexcept {
    return std::isfinite(arc.radius) &&
           std::isfinite(arc.start_angle) &&
           std::isfinite(arc.end_angle) &&
           arc.radius > kEpsilon;
}

double arc_sweep(const Arc& arc) noexcept {
    if (!valid_arc(arc)) {
        return 0.0;
    }

    const double full = 2.0 * std::numbers::pi;
    if (arc.counter_clockwise) {
        const double sweep = normalize_positive_angle(arc.end_angle - arc.start_angle);
        return nearly_equal(sweep, 0.0) ? full : sweep;
    }

    const double sweep = normalize_positive_angle(arc.start_angle - arc.end_angle);
    return nearly_equal(sweep, 0.0) ? full : sweep;
}

double arc_length(const Arc& arc) noexcept {
    return valid_arc(arc) ? arc.radius * arc_sweep(arc) : 0.0;
}

Vec2 arc_start_point(const Arc& arc) noexcept {
    return {
        arc.center.x + arc.radius * std::cos(arc.start_angle),
        arc.center.y + arc.radius * std::sin(arc.start_angle)
    };
}

Vec2 arc_end_point(const Arc& arc) noexcept {
    return {
        arc.center.x + arc.radius * std::cos(arc.end_angle),
        arc.center.y + arc.radius * std::sin(arc.end_angle)
    };
}

std::vector<Vec2> rectangle_from_corners(Vec2 first, Vec2 opposite) {
    return {
        first,
        {opposite.x, first.y},
        opposite,
        {first.x, opposite.y}
    };
}

} // namespace acp::geo
