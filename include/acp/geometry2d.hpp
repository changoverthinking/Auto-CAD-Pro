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

struct Arc {
    Vec2 center;
    double radius{};
    double start_angle{};
    double end_angle{};
    bool counter_clockwise{true};
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
[[nodiscard]] double polyline_length(const std::vector<Vec2>& points, bool closed) noexcept;

[[nodiscard]] bool valid_arc(const Arc& arc) noexcept;
[[nodiscard]] double arc_sweep(const Arc& arc) noexcept;
[[nodiscard]] double arc_length(const Arc& arc) noexcept;
[[nodiscard]] Vec2 arc_start_point(const Arc& arc) noexcept;
[[nodiscard]] Vec2 arc_end_point(const Arc& arc) noexcept;

[[nodiscard]] std::vector<Vec2> rectangle_from_corners(Vec2 first, Vec2 opposite);

} // namespace acp::geo
