#pragma once

#include <cmath>
#include <optional>
#include <vector>

namespace acp::geo {

inline constexpr double kEpsilon = 1e-9;

struct Vec2 {
    double x{};
    double y{};

    [[nodiscard]] Vec2 operator+(const Vec2& other) const noexcept { return {x + other.x, y + other.y}; }
    [[nodiscard]] Vec2 operator-(const Vec2& other) const noexcept { return {x - other.x, y - other.y}; }
    [[nodiscard]] Vec2 operator*(double s) const noexcept { return {x * s, y * s}; }
};

struct Segment {
    Vec2 a;
    Vec2 b;
};

struct Circle {
    Vec2 center;
    double radius{};
};

[[nodiscard]] double dot(Vec2 a, Vec2 b) noexcept;
[[nodiscard]] double cross(Vec2 a, Vec2 b) noexcept;
[[nodiscard]] double length(Vec2 v) noexcept;
[[nodiscard]] double distance(Vec2 a, Vec2 b) noexcept;
[[nodiscard]] bool nearly_equal(double a, double b, double eps = kEpsilon) noexcept;
[[nodiscard]] bool nearly_equal(Vec2 a, Vec2 b, double eps = kEpsilon) noexcept;
[[nodiscard]] Vec2 midpoint(const Segment& s) noexcept;
[[nodiscard]] Vec2 nearest_point(const Segment& s, Vec2 p) noexcept;
[[nodiscard]] std::optional<Vec2> segment_intersection(const Segment& lhs, const Segment& rhs, double eps = kEpsilon) noexcept;
[[nodiscard]] double polyline_length(const std::vector<Vec2>& points) noexcept;

} // namespace acp::geo
