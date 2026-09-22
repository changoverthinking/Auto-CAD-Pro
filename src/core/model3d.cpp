#include "acp/model3d.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

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

} // namespace

ObjectId Scene::insert(geo3d::Mesh mesh, std::string name, RgbColor color) {
    if (!geo3d::valid_mesh(mesh)) {
        return 0;
    }
    while (objects_.contains(next_id_)) {
        ++next_id_;
    }
    const ObjectId id = next_id_++;
    objects_.emplace(
        id,
        Object3D{id, std::move(name), std::move(mesh), color, true});
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

    const std::size_t count = polygon.size();
    geo3d::Mesh mesh;
    mesh.vertices.reserve(count * 2);
    mesh.triangles.reserve((count - 2) * 2 + count * 2);

    const double top_z = base_z + height;
    for (const auto& point : polygon) {
        mesh.vertices.push_back({point.x, point.y, base_z});
    }
    for (const auto& point : polygon) {
        mesh.vertices.push_back({point.x, point.y, top_z});
    }

    const bool ccw = signed_area(polygon) > 0.0;
    for (std::size_t i = 1; i + 1 < count; ++i) {
        const auto a = static_cast<std::uint32_t>(0);
        const auto b = static_cast<std::uint32_t>(i);
        const auto c = static_cast<std::uint32_t>(i + 1);
        const auto ta = static_cast<std::uint32_t>(count);
        const auto tb = static_cast<std::uint32_t>(count + i);
        const auto tc = static_cast<std::uint32_t>(count + i + 1);

        if (ccw == (height > 0.0)) {
            mesh.triangles.push_back({a, c, b});
            mesh.triangles.push_back({ta, tb, tc});
        } else {
            mesh.triangles.push_back({a, b, c});
            mesh.triangles.push_back({ta, tc, tb});
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t next = (i + 1) % count;
        const auto b0 = static_cast<std::uint32_t>(i);
        const auto b1 = static_cast<std::uint32_t>(next);
        const auto t0 = static_cast<std::uint32_t>(count + i);
        const auto t1 = static_cast<std::uint32_t>(count + next);

        if (ccw == (height > 0.0)) {
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
            document.effective_color(id));
    }
    return result;
}

} // namespace acp::model3d
