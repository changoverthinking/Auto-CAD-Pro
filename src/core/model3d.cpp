#include "acp/model3d.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace acp::model3d {
namespace {

double signed_area(const std::vector<geo::Vec2>& polygon) noexcept {
    double area = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5;
}

bool finite_polygon(const std::vector<geo::Vec2>& polygon) noexcept {
    if (polygon.size() < 3) {
        return false;
    }
    for (const auto& point : polygon) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            return false;
        }
    }
    return std::abs(signed_area(polygon)) > 1e-12;
}

double triangle_cross(
    geo::Vec2 a,
    geo::Vec2 b,
    geo::Vec2 c) noexcept {
    return geo::cross(b - a, c - a);
}

bool point_in_triangle(
    geo::Vec2 point,
    geo::Vec2 a,
    geo::Vec2 b,
    geo::Vec2 c) noexcept {

    constexpr double epsilon = 1e-12;
    const double ab = triangle_cross(a, b, point);
    const double bc = triangle_cross(b, c, point);
    const double ca = triangle_cross(c, a, point);
    const bool has_negative =
        ab < -epsilon || bc < -epsilon || ca < -epsilon;
    const bool has_positive =
        ab > epsilon || bc > epsilon || ca > epsilon;
    return !(has_negative && has_positive);
}

std::optional<std::vector<geo3d::Triangle>> triangulate_polygon(
    const std::vector<geo::Vec2>& polygon) {

    if (!finite_polygon(polygon)) {
        return std::nullopt;
    }

    const bool ccw = signed_area(polygon) > 0.0;
    std::vector<std::uint32_t> remaining;
    remaining.reserve(polygon.size());
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        remaining.push_back(static_cast<std::uint32_t>(i));
    }

    std::vector<geo3d::Triangle> triangles;
    triangles.reserve(polygon.size() - 2);

    std::size_t guard = 0;
    const std::size_t guard_limit = polygon.size() * polygon.size() * 2;
    while (remaining.size() > 3 && guard++ < guard_limit) {
        bool clipped = false;

        for (std::size_t i = 0; i < remaining.size(); ++i) {
            const std::uint32_t prev =
                remaining[(i + remaining.size() - 1) % remaining.size()];
            const std::uint32_t current = remaining[i];
            const std::uint32_t next = remaining[(i + 1) % remaining.size()];

            const double corner = triangle_cross(
                polygon[prev], polygon[current], polygon[next]);
            if ((ccw && corner <= 1e-12) ||
                (!ccw && corner >= -1e-12)) {
                continue;
            }

            bool contains_vertex = false;
            for (const std::uint32_t candidate : remaining) {
                if (candidate == prev ||
                    candidate == current ||
                    candidate == next) {
                    continue;
                }
                if (point_in_triangle(
                        polygon[candidate],
                        polygon[prev],
                        polygon[current],
                        polygon[next])) {
                    contains_vertex = true;
                    break;
                }
            }
            if (contains_vertex) {
                continue;
            }

            triangles.push_back({prev, current, next});
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }

        if (!clipped) {
            return std::nullopt;
        }
    }

    if (remaining.size() != 3) {
        return std::nullopt;
    }
    triangles.push_back({remaining[0], remaining[1], remaining[2]});
    return triangles;
}

} // namespace

ObjectId Scene::insert(
    geo3d::Mesh mesh,
    std::string name,
    RgbColor color,
    ObjectKind kind,
    std::optional<EntityId> source_entity_id) {

    if (!geo3d::valid_mesh(mesh)) {
        return 0;
    }
    while (objects_.contains(next_id_)) {
        ++next_id_;
    }
    const ObjectId id = next_id_++;
    objects_.emplace(
        id,
        Object3D{
            id,
            std::move(name),
            std::move(mesh),
            color,
            true,
            kind,
            source_entity_id});
    return id;
}

const Object3D* Scene::find(ObjectId id) const noexcept {
    const auto it = objects_.find(id);
    return it == objects_.end() ? nullptr : &it->second;
}

Object3D* Scene::find(ObjectId id) noexcept {
    const auto it = objects_.find(id);
    return it == objects_.end() ? nullptr : &it->second;
}

std::vector<ObjectId> Scene::ids() const {
    std::vector<ObjectId> result;
    result.reserve(objects_.size());
    for (const auto& [id, object] : objects_) {
        (void)object;
        result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool Scene::erase(ObjectId id) noexcept {
    return objects_.erase(id) == 1;
}

void Scene::clear() noexcept {
    objects_.clear();
    next_id_ = 1;
}

geo3d::Aabb3 Scene::bounds() const noexcept {
    geo3d::Aabb3 result;
    for (const auto& [id, object] : objects_) {
        (void)id;
        if (object.visible) {
            result.expand(geo3d::bounds(object.mesh));
        }
    }
    return result;
}

std::optional<geo3d::Mesh> extrude_polygon(
    const std::vector<geo::Vec2>& polygon,
    double height,
    double base_z) {

    if (!finite_polygon(polygon) ||
        !std::isfinite(height) ||
        !std::isfinite(base_z) ||
        std::abs(height) <= 1e-12) {
        return std::nullopt;
    }

    const auto cap = triangulate_polygon(polygon);
    if (!cap.has_value()) {
        return std::nullopt;
    }

    const std::size_t count = polygon.size();
    geo3d::Mesh mesh;
    mesh.vertices.reserve(count * 2);
    mesh.triangles.reserve(cap->size() * 2 + count * 2);

    const double top_z = base_z + height;
    for (const auto& point : polygon) {
        mesh.vertices.push_back({point.x, point.y, base_z});
    }
    for (const auto& point : polygon) {
        mesh.vertices.push_back({point.x, point.y, top_z});
    }

    const bool ccw = signed_area(polygon) > 0.0;
    const bool top_uses_polygon_winding = ccw == (height > 0.0);

    for (const geo3d::Triangle triangle : *cap) {
        const geo3d::Triangle bottom = top_uses_polygon_winding
            ? geo3d::Triangle{triangle.a, triangle.c, triangle.b}
            : triangle;
        const geo3d::Triangle top = top_uses_polygon_winding
            ? geo3d::Triangle{
                  static_cast<std::uint32_t>(count + triangle.a),
                  static_cast<std::uint32_t>(count + triangle.b),
                  static_cast<std::uint32_t>(count + triangle.c)}
            : geo3d::Triangle{
                  static_cast<std::uint32_t>(count + triangle.a),
                  static_cast<std::uint32_t>(count + triangle.c),
                  static_cast<std::uint32_t>(count + triangle.b)};
        mesh.triangles.push_back(bottom);
        mesh.triangles.push_back(top);
    }

    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t next = (i + 1) % count;
        const auto b0 = static_cast<std::uint32_t>(i);
        const auto b1 = static_cast<std::uint32_t>(next);
        const auto t0 = static_cast<std::uint32_t>(count + i);
        const auto t1 = static_cast<std::uint32_t>(count + next);

        if (top_uses_polygon_winding) {
            mesh.triangles.push_back({b0, b1, t1});
            mesh.triangles.push_back({b0, t1, t0});
        } else {
            mesh.triangles.push_back({b0, t1, b1});
            mesh.triangles.push_back({b0, t0, t1});
        }
    }

    return geo3d::valid_mesh(mesh)
        ? std::optional<geo3d::Mesh>{std::move(mesh)}
        : std::nullopt;
}

Scene extrude_closed_polylines(
    const Document& document,
    double height,
    double base_z) {

    Scene result;
    if (!std::isfinite(height) || std::abs(height) <= 1e-12) {
        return result;
    }

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        const auto* polyline =
            entity == nullptr
                ? nullptr
                : std::get_if<PolylineEntity>(entity);
        if (polyline == nullptr ||
            !polyline->closed ||
            polyline->points.size() < 3) {
            continue;
        }

        auto mesh = extrude_polygon(polyline->points, height, base_z);
        if (!mesh.has_value()) {
            continue;
        }
        (void)result.insert(
            std::move(*mesh),
            "Polyline_" + std::to_string(id),
            document.effective_color(id),
            ObjectKind::Generic,
            id);
    }
    return result;
}

} // namespace acp::model3d
