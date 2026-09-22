#include "acp/viewport3d.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace acp::viewport3d {
namespace {

ProjectedPoint project_relative(
    geo3d::Vec3 point,
    const Camera& camera,
    double viewport_width,
    double viewport_height) noexcept {

    const double cy = std::cos(camera.yaw);
    const double sy = std::sin(camera.yaw);
    const double cp = std::cos(camera.pitch);
    const double sp = std::sin(camera.pitch);

    const double yaw_x = cy * point.x - sy * point.y;
    const double yaw_y = sy * point.x + cy * point.y;
    const double camera_y = cp * yaw_y - sp * point.z;
    const double depth = sp * yaw_y + cp * point.z;

    return {
        viewport_width * 0.5 + camera.pan_x + yaw_x * camera.zoom,
        viewport_height * 0.5 + camera.pan_y - camera_y * camera.zoom,
        depth
    };
}

double face_light(
    geo3d::Vec3 a,
    geo3d::Vec3 b,
    geo3d::Vec3 c) noexcept {

    const geo3d::Vec3 normal =
        geo3d::normalized(geo3d::cross(b - a, c - a));
    const geo3d::Vec3 light =
        geo3d::normalized({-0.35, -0.55, 1.0});
    const double diffuse = std::abs(geo3d::dot(normal, light));
    return std::clamp(0.28 + diffuse * 0.72, 0.28, 1.0);
}

} // namespace

ProjectedPoint project_point(
    geo3d::Vec3 point,
    geo3d::Vec3 scene_center,
    const Camera& camera,
    double viewport_width,
    double viewport_height) noexcept {

    return project_relative(
        point - scene_center,
        camera,
        viewport_width,
        viewport_height);
}

std::vector<ProjectedTriangle> project_scene(
    const model3d::Scene& scene,
    const Camera& camera,
    double viewport_width,
    double viewport_height) {

    std::vector<ProjectedTriangle> result;
    const geo3d::Aabb3 scene_bounds = scene.bounds();
    if (!scene_bounds.initialized ||
        viewport_width <= 0.0 ||
        viewport_height <= 0.0 ||
        !std::isfinite(camera.zoom) ||
        camera.zoom <= 0.0) {
        return result;
    }

    const geo3d::Vec3 center = scene_bounds.center();

    for (const model3d::ObjectId id : scene.ids()) {
        const model3d::Object3D* object = scene.find(id);
        if (object == nullptr || !object->visible) {
            continue;
        }

        result.reserve(result.size() + object->mesh.triangles.size());
        for (const geo3d::Triangle triangle : object->mesh.triangles) {
            const geo3d::Vec3 va = object->mesh.vertices[triangle.a];
            const geo3d::Vec3 vb = object->mesh.vertices[triangle.b];
            const geo3d::Vec3 vc = object->mesh.vertices[triangle.c];

            const ProjectedPoint a =
                project_point(va, center, camera, viewport_width, viewport_height);
            const ProjectedPoint b =
                project_point(vb, center, camera, viewport_width, viewport_height);
            const ProjectedPoint c =
                project_point(vc, center, camera, viewport_width, viewport_height);

            result.push_back({
                a,
                b,
                c,
                object->color,
                (a.depth + b.depth + c.depth) / 3.0,
                face_light(va, vb, vc)
            });
        }
    }

    std::stable_sort(
        result.begin(),
        result.end(),
        [](const ProjectedTriangle& a, const ProjectedTriangle& b) {
            return a.depth < b.depth;
        });
    return result;
}

Camera fit_camera(
    const model3d::Scene& scene,
    Camera camera,
    double viewport_width,
    double viewport_height,
    double padding_fraction) noexcept {

    const geo3d::Aabb3 scene_bounds = scene.bounds();
    if (!scene_bounds.initialized ||
        viewport_width <= 1.0 ||
        viewport_height <= 1.0) {
        return camera;
    }

    padding_fraction = std::clamp(padding_fraction, 0.0, 0.45);
    const geo3d::Vec3 center = scene_bounds.center();

    const geo3d::Vec3 corners[] = {
        {scene_bounds.min.x, scene_bounds.min.y, scene_bounds.min.z},
        {scene_bounds.max.x, scene_bounds.min.y, scene_bounds.min.z},
        {scene_bounds.min.x, scene_bounds.max.y, scene_bounds.min.z},
        {scene_bounds.max.x, scene_bounds.max.y, scene_bounds.min.z},
        {scene_bounds.min.x, scene_bounds.min.y, scene_bounds.max.z},
        {scene_bounds.max.x, scene_bounds.min.y, scene_bounds.max.z},
        {scene_bounds.min.x, scene_bounds.max.y, scene_bounds.max.z},
        {scene_bounds.max.x, scene_bounds.max.y, scene_bounds.max.z}
    };

    Camera unit_camera = camera;
    unit_camera.zoom = 1.0;
    unit_camera.pan_x = 0.0;
    unit_camera.pan_y = 0.0;

    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();

    for (const geo3d::Vec3 corner : corners) {
        const ProjectedPoint p =
            project_relative(
                corner - center,
                unit_camera,
                0.0,
                0.0);
        min_x = std::min(min_x, p.x);
        min_y = std::min(min_y, p.y);
        max_x = std::max(max_x, p.x);
        max_y = std::max(max_y, p.y);
    }

    const double projected_width = std::max(max_x - min_x, 1e-9);
    const double projected_height = std::max(max_y - min_y, 1e-9);
    const double usable_width = viewport_width * (1.0 - 2.0 * padding_fraction);
    const double usable_height = viewport_height * (1.0 - 2.0 * padding_fraction);

    camera.zoom = std::max(
        1e-6,
        std::min(
            usable_width / projected_width,
            usable_height / projected_height));
    camera.pan_x = 0.0;
    camera.pan_y = 0.0;
    return camera;
}

} // namespace acp::viewport3d
