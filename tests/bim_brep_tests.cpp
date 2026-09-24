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
    return std::abs(a - b) < 1e-6;
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
    std::size_t plain_wall_triangles = 0;
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
        plain_wall_triangles = wall_geometry->render_mesh.triangles.size();
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

    Element door;
    door.kind = ElementKind::Door;
    door.level_id = level_1;
    door.width_mm = 900.0;
    door.height_mm = 2100.0;
    door.depth_mm = 40.0;
    door.sill_height_mm = 0.0;
    door.host_offset_normalized = 0.30;
    door.cut_clearance_mm = 2.0;
    const auto door_id = project.add_element(door);
    expect(project.set_host(door_id, wall_id), "door can be hosted by wall");

    Element window;
    window.kind = ElementKind::Window;
    window.level_id = level_1;
    window.width_mm = 1200.0;
    window.height_mm = 1200.0;
    window.depth_mm = 24.0;
    window.sill_height_mm = 900.0;
    window.host_offset_normalized = 0.72;
    window.cut_clearance_mm = 2.0;
    const auto window_id = project.add_element(window);
    expect(project.set_host(window_id, wall_id), "window can be hosted by wall");

    const auto hosted = project.hosted_by(wall_id);
    expect(hosted.size() == 2, "wall exposes two hosted BIM elements");
    if (hosted.size() == 2) {
        expect(hosted[0] == door_id && hosted[1] == window_id,
            "hosted BIM elements are returned in stable ID order");
    }

    const auto door_geometry = build_brep_geometry(project, door_id, *kernel);
    expect(door_geometry.has_value(), "hosted door panel produces derived BRep geometry");
    if (door_geometry.has_value()) {
        expect(door_geometry->element_id == door_id, "door ElementId preserved");
        expect(door_geometry->kind == ElementKind::Door, "door kind preserved");
        const auto size = acp::geo3d::bounds(door_geometry->render_mesh).size();
        expect(near(size.x, 900.0), "door width preserved on horizontal host");
        expect(near(size.y, 40.0), "door panel depth preserved");
        expect(near(size.z, 2100.0), "door height preserved");
    }

    const auto window_geometry = build_brep_geometry(project, window_id, *kernel);
    expect(window_geometry.has_value(), "hosted window panel produces derived BRep geometry");
    if (window_geometry.has_value()) {
        expect(window_geometry->element_id == window_id, "window ElementId preserved");
        expect(window_geometry->kind == ElementKind::Window, "window kind preserved");
        const auto bounds = acp::geo3d::bounds(window_geometry->render_mesh);
        const auto size = bounds.size();
        expect(near(size.x, 1200.0), "window width preserved on horizontal host");
        expect(near(size.y, 24.0), "window panel depth preserved");
        expect(near(size.z, 1200.0), "window height preserved");
        expect(near(bounds.min.z, 1000.0), "window sill follows host base plus sill height");
    }

    const auto opened_wall = build_brep_geometry(project, wall_id, *kernel);
    if (kernel->production_brep()) {
        expect(opened_wall.has_value(), "production BRep cuts hosted door/window openings");
        if (opened_wall.has_value()) {
            expect(acp::geo3d::valid_mesh(opened_wall->render_mesh),
                "wall with exact openings tessellates to valid render cache");
            expect(opened_wall->render_mesh.triangles.size() > plain_wall_triangles,
                "hosted openings change wall tessellation topology");
            const auto bounds = acp::geo3d::bounds(opened_wall->render_mesh);
            const auto size = bounds.size();
            expect(near(size.x, 5000.0) && near(size.y, 200.0) && near(size.z, 3000.0),
                "opening cuts preserve overall wall extents");
        }
    } else {
        expect(!opened_wall.has_value(),
            "compatibility backend fails closed instead of faking hosted opening cuts");
    }

    Element invalid_window;
    invalid_window.kind = ElementKind::Window;
    invalid_window.level_id = level_1;
    invalid_window.width_mm = 6000.0;
    invalid_window.height_mm = 1000.0;
    invalid_window.depth_mm = 24.0;
    invalid_window.sill_height_mm = 800.0;
    invalid_window.host_offset_normalized = 0.5;
    const auto invalid_window_id = project.add_element(invalid_window);
    expect(project.set_host(invalid_window_id, wall_id), "invalid window relationship itself can be stored");
    expect(!build_brep_geometry(project, invalid_window_id, *kernel).has_value(),
        "opening wider than its host wall is rejected during geometry derivation");
    expect(project.clear_host(invalid_window_id), "invalid hosted relationship can be cleared");

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
