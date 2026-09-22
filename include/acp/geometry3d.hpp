#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace acp::geo3d {

struct Vec3 {
    double x{};
    double y{};
    double z{};

    friend bool operator==(const Vec3&, const Vec3&) = default;
};

[[nodiscard]] Vec3 operator+(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 operator-(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 operator*(Vec3 value, double scale) noexcept;
[[nodiscard]] Vec3 operator/(Vec3 value, double scale) noexcept;

[[nodiscard]] double dot(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept;
[[nodiscard]] double length(Vec3 value) noexcept;
[[nodiscard]] Vec3 normalized(Vec3 value) noexcept;

struct Mat4 {
    std::array<double, 16> values{};

    [[nodiscard]] static Mat4 identity() noexcept;
    [[nodiscard]] static Mat4 translation(Vec3 offset) noexcept;
    [[nodiscard]] static Mat4 scale(Vec3 factors) noexcept;
    [[nodiscard]] static Mat4 rotation_x(double radians) noexcept;
    [[nodiscard]] static Mat4 rotation_y(double radians) noexcept;
    [[nodiscard]] static Mat4 rotation_z(double radians) noexcept;
};

[[nodiscard]] Mat4 operator*(const Mat4& a, const Mat4& b) noexcept;
[[nodiscard]] Vec3 transform_point(const Mat4& matrix, Vec3 point) noexcept;

struct Triangle {
    std::uint32_t a{};
    std::uint32_t b{};
    std::uint32_t c{};

    friend bool operator==(const Triangle&, const Triangle&) = default;
};

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
};

[[nodiscard]] bool valid_mesh(const Mesh& mesh) noexcept;

struct Aabb3 {
    Vec3 min{};
    Vec3 max{};
    bool initialized{false};

    void expand(Vec3 point) noexcept;
    void expand(const Aabb3& other) noexcept;
    [[nodiscard]] Vec3 center() const noexcept;
    [[nodiscard]] Vec3 size() const noexcept;
};

[[nodiscard]] Aabb3 bounds(const Mesh& mesh) noexcept;
[[nodiscard]] Mesh transformed(const Mesh& mesh, const Mat4& matrix);

} // namespace acp::geo3d
