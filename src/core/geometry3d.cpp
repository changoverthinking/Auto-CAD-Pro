#include "acp/geometry3d.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace acp::geo3d {

Vec3 operator+(Vec3 a, Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(Vec3 a, Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 operator*(Vec3 value, double scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 operator/(Vec3 value, double scale) noexcept {
    return scale == 0.0 ? Vec3{} : value * (1.0 / scale);
}

double dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double length(Vec3 value) noexcept {
    return std::sqrt(dot(value, value));
}

Vec3 normalized(Vec3 value) noexcept {
    const double magnitude = length(value);
    if (!std::isfinite(magnitude) ||
        magnitude <= std::numeric_limits<double>::epsilon()) {
        return {};
    }
    return value / magnitude;
}

Mat4 Mat4::identity() noexcept {
    Mat4 result{};
    result.values = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };
    return result;
}

Mat4 Mat4::translation(Vec3 offset) noexcept {
    Mat4 result = identity();
    result.values[3] = offset.x;
    result.values[7] = offset.y;
    result.values[11] = offset.z;
    return result;
}

Mat4 Mat4::scale(Vec3 factors) noexcept {
    Mat4 result = identity();
    result.values[0] = factors.x;
    result.values[5] = factors.y;
    result.values[10] = factors.z;
    return result;
}

Mat4 Mat4::rotation_x(double radians) noexcept {
    Mat4 result = identity();
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    result.values[5] = c;
    result.values[6] = -s;
    result.values[9] = s;
    result.values[10] = c;
    return result;
}

Mat4 Mat4::rotation_y(double radians) noexcept {
    Mat4 result = identity();
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    result.values[0] = c;
    result.values[2] = s;
    result.values[8] = -s;
    result.values[10] = c;
    return result;
}

Mat4 Mat4::rotation_z(double radians) noexcept {
    Mat4 result = identity();
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    result.values[0] = c;
    result.values[1] = -s;
    result.values[4] = s;
    result.values[5] = c;
    return result;
}

Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    Mat4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            double value = 0.0;
            for (int k = 0; k < 4; ++k) {
                value += a.values[static_cast<std::size_t>(row * 4 + k)] *
                         b.values[static_cast<std::size_t>(k * 4 + col)];
            }
            result.values[static_cast<std::size_t>(row * 4 + col)] = value;
        }
    }
    return result;
}

Vec3 transform_point(const Mat4& matrix, Vec3 point) noexcept {
    const auto& m = matrix.values;
    const double x = m[0] * point.x + m[1] * point.y + m[2] * point.z + m[3];
    const double y = m[4] * point.x + m[5] * point.y + m[6] * point.z + m[7];
    const double z = m[8] * point.x + m[9] * point.y + m[10] * point.z + m[11];
    const double w = m[12] * point.x + m[13] * point.y + m[14] * point.z + m[15];
    if (std::isfinite(w) && std::abs(w) > std::numeric_limits<double>::epsilon() &&
        std::abs(w - 1.0) > std::numeric_limits<double>::epsilon()) {
        return {x / w, y / w, z / w};
    }
    return {x, y, z};
}

bool valid_mesh(const Mesh& mesh) noexcept {
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        return false;
    }
    for (const Vec3 vertex : mesh.vertices) {
        if (!std::isfinite(vertex.x) ||
            !std::isfinite(vertex.y) ||
            !std::isfinite(vertex.z)) {
            return false;
        }
    }
    for (const Triangle triangle : mesh.triangles) {
        if (triangle.a >= mesh.vertices.size() ||
            triangle.b >= mesh.vertices.size() ||
            triangle.c >= mesh.vertices.size() ||
            triangle.a == triangle.b ||
            triangle.b == triangle.c ||
            triangle.a == triangle.c) {
            return false;
        }
    }
    return true;
}

void Aabb3::expand(Vec3 point) noexcept {
    if (!initialized) {
        min = point;
        max = point;
        initialized = true;
        return;
    }
    min.x = std::min(min.x, point.x);
    min.y = std::min(min.y, point.y);
    min.z = std::min(min.z, point.z);
    max.x = std::max(max.x, point.x);
    max.y = std::max(max.y, point.y);
    max.z = std::max(max.z, point.z);
}

void Aabb3::expand(const Aabb3& other) noexcept {
    if (!other.initialized) {
        return;
    }
    expand(other.min);
    expand(other.max);
}

Vec3 Aabb3::center() const noexcept {
    return initialized ? (min + max) * 0.5 : Vec3{};
}

Vec3 Aabb3::size() const noexcept {
    return initialized ? max - min : Vec3{};
}

Aabb3 bounds(const Mesh& mesh) noexcept {
    Aabb3 result;
    for (const Vec3 vertex : mesh.vertices) {
        result.expand(vertex);
    }
    return result;
}

Mesh transformed(const Mesh& mesh, const Mat4& matrix) {
    Mesh result = mesh;
    for (Vec3& vertex : result.vertices) {
        vertex = transform_point(matrix, vertex);
    }
    return result;
}

} // namespace acp::geo3d
