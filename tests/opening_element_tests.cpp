#include "acp/architecture3d.hpp"
#include "acp/obj.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool close(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) <= epsilon;
}

} // namespace

int main() {
    namespace arch = acp::architecture3d;

    const arch::WallSpec wall{
        {0.0, 0.0},
        {5000.0, 0.0},
        200.0,
        3000.0,
        0.0
    };

    const arch::WallOpeningSpec door{
        wall,
        arch::OpeningKind::Door,
        0.5,
        900.0,
        2100.0,
        0.0,
        2.0
    };
    const auto door_panel = arch::make_opening_panel(door, 40.0);
    expect(door_panel.has_value(), "door panel builds");
    if (door_panel.has_value()) {
        const auto bounds = acp::geo3d::bounds(*door_panel);
        expect(close(bounds.size().x, 900.0), "door panel width preserved");
        expect(close(bounds.size().y, 40.0), "door panel depth preserved");
        expect(close(bounds.size().z, 2100.0), "door panel height preserved");
    }

    const arch::WallOpeningSpec window{
        wall,
        arch::OpeningKind::Window,
        0.75,
        1200.0,
        1200.0,
        900.0,
        2.0
    };
    const auto window_panel = arch::make_opening_panel(window, 24.0);
    expect(window_panel.has_value(), "window panel builds");
    if (window_panel.has_value()) {
        const auto bounds = acp::geo3d::bounds(*window_panel);
        expect(close(bounds.size().x, 1200.0), "window panel width preserved");
        expect(close(bounds.size().y, 24.0), "window panel depth preserved");
        expect(close(bounds.min.z, 900.0), "window panel sill elevation preserved");
        expect(close(bounds.max.z, 2100.0), "window panel head elevation preserved");
    }

    expect(
        !arch::make_opening_panel(window, 0.0).has_value(),
        "zero-depth opening panel rejected");

    acp::model3d::Scene scene;
    auto host_mesh = arch::make_wall_with_openings(wall, {door, window});
    expect(host_mesh.has_value(), "host wall with openings builds");
    const auto host_id = host_mesh.has_value()
        ? scene.insert(
              std::move(*host_mesh),
              "HostWall",
              {200, 205, 214},
              acp::model3d::ObjectKind::Wall)
        : 0;
    expect(host_id != 0, "host wall inserts into scene");

    const auto door_id = arch::add_opening_object(
        scene,
        door,
        40.0,
        "MainEntry",
        {160, 190, 215},
        std::nullopt,
        host_id);
    const auto window_id = arch::add_opening_object(
        scene,
        window,
        24.0,
        "LivingGlazing",
        {160, 190, 215},
        std::nullopt,
        host_id);
    expect(door_id != 0 && window_id != 0, "door and window insert into scene");
    expect(scene.size() == 3, "hosted opening scene contains wall and two openings");

    const auto* door_object = scene.find(door_id);
    expect(door_object != nullptr, "door scene object exists");
    if (door_object != nullptr) {
        expect(
            door_object->kind == acp::model3d::ObjectKind::Door,
            "door carries semantic Door kind");
        expect(door_object->name == "MainEntry", "door keeps supplied scene name");
        expect(door_object->host_object_id == host_id, "door keeps host wall relationship");
    }

    const auto* window_object = scene.find(window_id);
    expect(window_object != nullptr, "window scene object exists");
    if (window_object != nullptr) {
        expect(
            window_object->kind == acp::model3d::ObjectKind::Window,
            "window carries semantic Window kind");
        expect(window_object->name == "LivingGlazing", "window keeps supplied scene name");
        expect(window_object->host_object_id == host_id, "window keeps host wall relationship");
    }

    expect(
        arch::add_opening_object(
            scene,
            door,
            40.0,
            "InvalidHost",
            {160, 190, 215},
            std::nullopt,
            999999) == 0,
        "opening with missing host is rejected");

    const std::string obj = acp::obj::serialize(scene);
    expect(
        obj.find("o Door_MainEntry\n") != std::string::npos,
        "OBJ prefixes custom door name with semantic kind");
    expect(
        obj.find("o Window_LivingGlazing\n") != std::string::npos,
        "OBJ prefixes custom window name with semantic kind");

    expect(scene.erase(host_id), "host wall can be erased");
    door_object = scene.find(door_id);
    window_object = scene.find(window_id);
    expect(
        door_object != nullptr && !door_object->host_object_id.has_value(),
        "erasing host clears door host relationship");
    expect(
        window_object != nullptr && !window_object->host_object_id.has_value(),
        "erasing host clears window host relationship");

    if (failures == 0) {
        std::cout << "Opening element tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
