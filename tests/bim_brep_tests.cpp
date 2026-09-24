#include "acp/bim_brep.hpp"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool near(double a, double b) {
    return std::abs(a - b) < 1e-9;
}
} // namespace

int main() {
    using namespace acp::bim;

    ProjectModel project;
    const auto level_1 = project.add_level("Level 1", 0.0);
    const auto level_2 = project.add_level("Level 2", 3200.0);

    Element wall;
    wall.kind = ElementKind::Wall;
    wall.level_id = level_1;
    wall.top_level_id = level_2;
    wall.base_offset_mm = 100.0;
    wall.top_offset_mm = -100.0;
    wall.thickness_mm = 200.0;
    wall.location_curve = LocationCurve{{0.0, 0.0}, {5000.0, 0.0}};
    const auto wall_id = project.add_element(wall);

    Element slab;
    slab.kind = ElementKind::Slab;
    slab.level_id = level_2;
    slab.base_offset_mm = 0.0;
    slab.thickness_mm = 180.0;
    slab.footprint = {
        {0.0, 0.0},
        {6000.0, 0.0},
        {6000.0, 4000.0},
        {0.0, 4000.0},
    };
    const auto slab_id = project.add_element(slab);

    auto kernel = acp::brep::make_default_kernel();
    expect(kernel != nullptr, "default BRep kernel exists");
    if (!kernel) {
        return 1;
    }

    const auto wall_geometry = build_brep_geometry(project, wall_id, *kernel);
    expect(wall_geometry.has_value(), "wall semantic element produces BRep geometry");
    if (wall_geometry.has_value()) {
        expect(wall_geometry->element_id == wall_id, "wall ElementId preserved");
        expect(wall_geometry->kind == ElementKind::Wall, "wall kind preserved");
        expect(wall_geometry->shape.kind == acp::brep::ShapeKind::Solid, "wall is BRep solid");
        const auto bounds = acp::geo3d::bounds(wall_geometry->render_mesh);
        const auto size = bounds.size();
        expect(near(size.x, 5000.0), "wall length derived from centerline");
        expect(near(size.y, 200.0), "wall thickness derived from semantic thickness");
        expect(near(size.z, 3000.0), "wall height derived from levels and offsets");
        expect(near(bounds.min.z, 100.0), "wall base elevation derived from level and offset");
    }

    const auto slab_geometry = build_brep_geometry(project, slab_id, *kernel);
    expect(slab_geometry.has_value(), "slab footprint produces BRep geometry");
    if (slab_geometry.has_value()) {
        expect(slab_geometry->element_id == slab_id, "slab ElementId preserved");
        expect(slab_geometry->kind == ElementKind::Slab, "slab kind preserved");
        const auto bounds = acp::geo3d::bounds(slab_geometry->render_mesh);
        const auto size = bounds.size();
        expect(near(size.x, 6000.0), "slab footprint width preserved");
        expect(near(size.y, 4000.0), "slab footprint depth preserved");
        expect(near(size.z, 180.0), "slab thickness preserved");
        expect(near(bounds.min.z, 3200.0), "slab elevation follows host level");
    }

    Element invalid_wall;
    invalid_wall.kind = ElementKind::Wall;
    invalid_wall.level_id = level_1;
    invalid_wall.thickness_mm = 200.0;
    invalid_wall.unconnected_height_mm = 3000.0;
    const auto invalid_wall_id = project.add_element(invalid_wall);
    expect(!build_brep_geometry(project, invalid_wall_id, *kernel).has_value(),
        "wall without location curve is rejected");

    Element invalid_slab;
    invalid_slab.kind = ElementKind::Slab;
    invalid_slab.level_id = level_1;
    invalid_slab.thickness_mm = 200.0;
    invalid_slab.footprint = {{0.0, 0.0}, {1000.0, 0.0}};
    const auto invalid_slab_id = project.add_element(invalid_slab);
    expect(!build_brep_geometry(project, invalid_slab_id, *kernel).has_value(),
        "slab with degenerate footprint is rejected");

    if (failures == 0) {
        std::cout << "BIM BRep derivation tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
