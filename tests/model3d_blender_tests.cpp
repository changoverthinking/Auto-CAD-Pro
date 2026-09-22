#include "acp/blender_bridge.hpp"
#include "acp/document.hpp"
#include "acp/model3d.hpp"
#include "acp/obj.hpp"
#include "acp/viewport3d.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::size_t count_lines_with_prefix(
    const std::string& value,
    const std::string& prefix) {

    std::size_t count = 0;
    std::size_t start = 0;
    while (start < value.size()) {
        const std::size_t end = value.find('\n', start);
        const std::size_t length =
            end == std::string::npos ? value.size() - start : end - start;
        if (value.compare(start, prefix.size(), prefix) == 0) {
            ++count;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return count;
}

} // namespace

int main() {
    using acp::geo::Vec2;

    const std::vector<Vec2> square{
        {0.0, 0.0},
        {4.0, 0.0},
        {4.0, 3.0},
        {0.0, 3.0}
    };

    const auto mesh = acp::model3d::extrude_polygon(square, 2.5);
    expect(mesh.has_value(), "square must extrude");
    if (mesh.has_value()) {
        expect(mesh->vertices.size() == 8, "extruded square has 8 vertices");
        expect(mesh->triangles.size() == 12, "extruded square has 12 triangles");

        const auto bounds = acp::geo3d::bounds(*mesh);
        expect(bounds.initialized, "mesh bounds initialized");
        expect(std::abs(bounds.size().x - 4.0) < 1e-9, "bounds X");
        expect(std::abs(bounds.size().y - 3.0) < 1e-9, "bounds Y");
        expect(std::abs(bounds.size().z - 2.5) < 1e-9, "bounds Z");
    }

    acp::Document document;
    const acp::EntityId polyline_id = document.insert(
        acp::PolylineEntity{square, true});
    expect(polyline_id != 0, "2D source polyline inserted");

    const auto scene =
        acp::model3d::extrude_closed_polylines(document, 2.5);
    expect(scene.size() == 1, "closed 2D polyline becomes one 3D object");

    acp::viewport3d::Camera camera{};
    camera = acp::viewport3d::fit_camera(scene, camera, 1000.0, 700.0);
    expect(camera.zoom > 0.0, "3D camera fit returns positive zoom");
    const auto projected =
        acp::viewport3d::project_scene(scene, camera, 1000.0, 700.0);
    expect(projected.size() == 12, "viewport projects every triangle");
    if (!projected.empty()) {
        expect(std::isfinite(projected.front().a.x), "projected X is finite");
        expect(std::isfinite(projected.front().a.y), "projected Y is finite");
        expect(
            projected.front().light >= 0.28 &&
            projected.front().light <= 1.0,
            "viewport light factor is clamped");
    }

    const std::string obj = acp::obj::serialize(scene);
    expect(count_lines_with_prefix(obj, "v ") == 8, "OBJ has 8 vertices");
    expect(count_lines_with_prefix(obj, "f ") == 12, "OBJ has 12 faces");
    expect(obj.find("o Polyline_") != std::string::npos, "OBJ names source object");

    const std::string script = acp::blender::import_script(
        "C:/cad/test scene.obj",
        std::filesystem::path{"C:/cad/test scene.blend"});
    expect(
        script.find("scene.unit_settings.system = 'METRIC'") != std::string::npos,
        "Blender bridge sets metric units");
    expect(
        script.find("bpy.ops.wm.obj_import") != std::string::npos,
        "Blender 4 OBJ import path present");
    expect(
        script.find("bpy.ops.import_scene.obj") != std::string::npos,
        "legacy Blender OBJ import fallback present");
    expect(
        script.find("bpy.ops.wm.save_as_mainfile") != std::string::npos,
        "Blender save path supported");

    const std::string command = acp::blender::command_line(
        "C:/Program Files/Blender Foundation/Blender/blender.exe",
        "C:/cad/import_auto_cad_pro.py");
    expect(
        command.find("--python") != std::string::npos,
        "Blender command uses generated Python script");

    if (failures == 0) {
        std::cout << "3D/Blender tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
