#include "acp/project_model.hpp"

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
}

int main() {
    using namespace acp::bim;

    ProjectModel project;
    const LevelId ground = project.add_level("Ground", 0.0);
    const LevelId level1 = project.add_level("Level 1", 3000.0);
    expect(ground != kInvalidLevelId && level1 != kInvalidLevelId, "levels get stable IDs");
    expect(project.level_ids().size() == 2, "two levels registered");

    ParameterSet material_params;
    material_params["density_kg_m3"] = 2400.0;
    const MaterialId concrete = project.add_material("Concrete", material_params);
    expect(project.find_material(concrete) != nullptr, "material lookup works");

    ParameterSet wall_type_params;
    wall_type_params["fire_rating_min"] = std::int64_t{120};
    const ElementTypeId wall_type = project.add_type(
        ElementKind::Wall,
        "Concrete 200",
        wall_type_params,
        {concrete});

    Element wall;
    wall.kind = ElementKind::Wall;
    wall.type_id = wall_type;
    wall.level_id = ground;
    wall.top_level_id = level1;
    wall.thickness_mm = 200.0;
    wall.location_curve = LocationCurve{{0.0, 0.0}, {5000.0, 0.0}};
    wall.materials = {concrete};
    wall.structural = true;
    const ElementId wall_id = project.add_element(wall);
    expect(wall_id != kInvalidElementId, "wall gets stable ID");

    Element door;
    door.kind = ElementKind::Door;
    door.level_id = ground;
    door.parameters["width_mm"] = 900.0;
    door.parameters["height_mm"] = 2100.0;
    const ElementId door_id = project.add_element(door);
    expect(project.set_host(door_id, wall_id), "door can be hosted by wall");
    expect(project.host_of(door_id).value_or(0) == wall_id, "host relationship is retained");
    expect(!project.set_host(wall_id, wall_id), "self hosting is rejected");

    const Element* stored_wall = project.find_element(wall_id);
    expect(stored_wall != nullptr, "wall lookup works");
    if (stored_wall != nullptr) {
        expect(stored_wall->kind == ElementKind::Wall, "wall semantic kind is canonical");
        expect(std::abs(stored_wall->thickness_mm - 200.0) < 1e-12, "wall thickness is retained");
        expect(stored_wall->location_curve.has_value(), "wall location curve is retained");
    }

    Element imported;
    imported.id = 5000;
    imported.kind = ElementKind::GenericModel;
    const ElementId imported_id = project.add_element(imported);
    expect(imported_id == 5000, "explicit stable ID is preserved");
    expect(project.add_element(imported) == kInvalidElementId, "duplicate stable ID is rejected");

    Element generated;
    generated.kind = ElementKind::Column;
    const ElementId generated_id = project.add_element(generated);
    expect(generated_id > imported_id, "generated IDs advance beyond imported IDs");

    expect(project.erase_element(wall_id), "host wall can be erased");
    expect(!project.host_of(door_id).has_value(), "dangling host relationships are removed");
    expect(project.find_element(door_id) != nullptr, "hosted element remains after host deletion");

    if (failures == 0) {
        std::cout << "ProjectModel tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
