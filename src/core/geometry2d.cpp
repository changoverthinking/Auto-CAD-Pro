#include "acp/geometry2d.hpp"

#include <algorithm>

namespace acp::geo {

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
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        total += distance(points[i - 1], points[i]);
    }
    return total;
}

} // namespace acp::geo
