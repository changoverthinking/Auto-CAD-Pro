#pragma once

#include "acp/model3d.hpp"

#include <vector>

namespace acp::viewport3d {

struct Camera {
    double yaw{0.7853981633974483};
    double pitch{0.5235987755982988};
    double zoom{40.0};
    double pan_x{};
    double pan_y{};
};

struct ProjectedPoint {
    double x{};
    double y{};
    double depth{};
};

struct ProjectedTriangle {
    ProjectedPoint a;
    ProjectedPoint b;
    ProjectedPoint c;
    RgbColor color{};
    double depth{};
    double light{1.0};
};

[[nodiscard]] ProjectedPoint project_point(
    geo3d::Vec3 point,
    geo3d::Vec3 scene_center,
    const Camera& camera,
    double viewport_width,
    double viewport_height) noexcept;

[[nodiscard]] std::vector<ProjectedTriangle> project_scene(
    const model3d::Scene& scene,
    const Camera& camera,
    double viewport_width,
    double viewport_height);

[[nodiscard]] Camera fit_camera(
    const model3d::Scene& scene,
    Camera camera,
    double viewport_width,
    double viewport_height,
    double padding_fraction = 0.10) noexcept;

} // namespace acp::viewport3d
