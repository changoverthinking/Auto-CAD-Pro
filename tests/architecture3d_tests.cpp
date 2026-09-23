#include "acp/architecture3d.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

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
    using acp::geo::Vec2;
    namespace arch = acp::architecture3d;

    const std::vector<arch::Level> levels{
        {"Ground", 0.0},
        {"Level 2", 3200.0},
        {"Roof", 6400.0}
    };
    expect(arch::valid_levels(levels), "unique finite levels are valid");
    const auto level2 = arch::level_elevation(levels, "Level 2");
    expect(level2.has_value() && close(*level2, 3200.0), "level elevation resolves by name");
    expect(!arch::level_elevation(levels, "Missing").has_value(), "missing level does not resolve");
    expect(
        !arch::valid_levels({{"Ground", 0.0}, {"Ground", 3200.0}}),
        "duplicate level names rejected");
    expect(
        !arch::valid(arch::Level{"Bad", std::numeric_limits<double>::infinity()}),
        "non-finite level elevation rejected");

    const arch::WallSpec wall{
        {0.0, 0.0},
        {5000.0, 0.0},
        200.0,
        3000.0,
        0.0
    };
    const auto wall_mesh = arch::make_wall(wall);
    expect(wall_mesh.has_value(), "valid wall builds a mesh");
    if (wall_mesh.has_value()) {
        expect(wall_mesh->vertices.size() == 8, "wall has eight vertices");
        expect(wall_mesh->triangles.size() == 12, "wall has twelve triangles");
        const auto bounds = acp::geo3d::bounds(*wall_mesh);
        expect(close(bounds.size().x, 5000.0), "wall length preserved");
        expect(close(bounds.size().y, 200.0), "wall thickness preserved");
        expect(close(bounds.size().z, 3000.0), "wall height preserved");
    }

    const arch::WallOpeningSpec door{
        wall,
        arch::OpeningKind::Door,
        0.5,
        900.0,
        2100.0,
        0.0,
        2.0
    };
    expect(arch::valid(door), "door opening inside wall is valid");
    const auto door_cut = arch::make_wall_opening_volume(door);
    expect(door_cut.has_value(), "door opening creates a cutter volume");
    if (door_cut.has_value()) {
        const auto bounds = acp::geo3d::bounds(*door_cut);
        expect(close(bounds.size().x, 900.0), "door cutter preserves width");
        expect(close(bounds.size().y, 204.0), "door cutter crosses full wall thickness with clearance");
        expect(close(bounds.size().z, 2100.0), "door cutter preserves height");
        expect(close(bounds.min.z, 0.0), "door begins at wall base");
    }

    const auto wall_with_door = arch::make_wall_with_openings(wall, {door});
    expect(wall_with_door.has_value(), "wall with hosted door builds");
    if (wall_with_door.has_value()) {
        expect(wall_with_door->vertices.size() == 24, "door wall decomposes into two piers and lintel");
        expect(wall_with_door->triangles.size() == 36, "door wall piece faces are complete");
        const auto bounds = acp::geo3d::bounds(*wall_with_door);
        expect(close(bounds.size().x, 5000.0), "door wall keeps full span");
        expect(close(bounds.size().y, 200.0), "door wall keeps host thickness");
        expect(close(bounds.size().z, 3000.0), "door wall keeps host height");
    }

    const arch::WallOpeningSpec window{
        wall,
        arch::OpeningKind::Window,
        0.6,
        1200.0,
        1200.0,
        900.0,
        2.0
    };
    expect(arch::valid(window), "window opening inside wall is valid");
    const auto window_cut = arch::make_wall_opening_volume(window);
    expect(window_cut.has_value(), "window opening creates a cutter volume");
    if (window_cut.has_value()) {
        const auto bounds = acp::geo3d::bounds(*window_cut);
        expect(close(bounds.min.z, 900.0), "window sill elevation preserved");
        expect(close(bounds.max.z, 2100.0), "window head elevation preserved");
    }

    const auto wall_with_window = arch::make_wall_with_openings(wall, {window});
    expect(wall_with_window.has_value(), "wall with hosted window builds");
    if (wall_with_window.has_value()) {
        expect(wall_with_window->vertices.size() == 32, "window wall has piers sill and head pieces");
        expect(wall_with_window->triangles.size() == 48, "window wall piece faces are complete");
    }

    const arch::WallOpeningSpec left_door{
        wall,
        arch::OpeningKind::Door,
        0.2,
        800.0,
        2100.0,
        0.0,
        2.0
    };
    const arch::WallOpeningSpec right_window{
        wall,
        arch::OpeningKind::Window,
        0.72,
        1000.0,
        1200.0,
        900.0,
        2.0
    };
    expect(
        arch::make_wall_with_openings(wall, {left_door, right_window}).has_value(),
        "multiple non-overlapping hosted openings build");

    auto overlapping_window = right_window;
    overlapping_window.center_offset = 0.27;
    overlapping_window.width = 1000.0;
    expect(
        !arch::make_wall_with_openings(wall, {left_door, overlapping_window}).has_value(),
        "overlapping hosted openings rejected");

    auto invalid_opening = window;
    invalid_opening.center_offset = 0.02;
    invalid_opening.width = 1000.0;
    expect(!arch::valid(invalid_opening), "opening extending past wall end rejected");
    invalid_opening = window;
    invalid_opening.sill_height = 2200.0;
    invalid_opening.height = 1200.0;
    expect(!arch::valid(invalid_opening), "opening extending above wall rejected");

    const arch::SlabSpec slab{
        {
            {0.0, 0.0},
            {4000.0, 0.0},
            {4000.0, 3000.0},
            {0.0, 3000.0}
        },
        180.0,
        0.0
    };
    const auto slab_mesh = arch::make_slab(slab);
    expect(slab_mesh.has_value(), "valid slab builds a mesh");
    if (slab_mesh.has_value()) {
        const auto bounds = acp::geo3d::bounds(*slab_mesh);
        expect(close(bounds.min.z, -180.0), "slab extrudes below top elevation");
        expect(close(bounds.max.z, 0.0), "slab top elevation preserved");
    }

    const arch::SlabSpec concave_slab{
        {
            {0.0, 0.0},
            {5000.0, 0.0},
            {5000.0, 2000.0},
            {2500.0, 2000.0},
            {2500.0, 4500.0},
            {0.0, 4500.0}
        },
        200.0,
        500.0
    };
    const auto concave_mesh = arch::make_slab(concave_slab);
    expect(concave_mesh.has_value(), "concave L-shaped slab triangulates");
    if (concave_mesh.has_value()) {
        expect(concave_mesh->vertices.size() == 12, "concave slab has paired vertices");
        expect(concave_mesh->triangles.size() == 20, "concave slab has correct face count");
        const auto bounds = acp::geo3d::bounds(*concave_mesh);
        expect(close(bounds.min.z, 300.0), "concave slab bottom elevation preserved");
        expect(close(bounds.max.z, 500.0), "concave slab top elevation preserved");
    }

    const arch::ColumnSpec column{
        {1000.0, 1000.0},
        400.0,
        600.0,
        3200.0,
        100.0,
        0.0
    };
    const auto column_mesh = arch::make_column(column);
    expect(column_mesh.has_value(), "valid column builds a mesh");
    if (column_mesh.has_value()) {
        const auto bounds = acp::geo3d::bounds(*column_mesh);
        expect(close(bounds.size().x, 400.0), "column width preserved");
        expect(close(bounds.size().y, 600.0), "column depth preserved");
        expect(close(bounds.min.z, 100.0), "column base elevation preserved");
        expect(close(bounds.max.z, 3300.0), "column top elevation preserved");
    }

    const arch::BeamSpec beam{
        {0.0, 0.0},
        {5000.0, 0.0},
        300.0,
        500.0,
        3000.0
    };
    const auto beam_mesh = arch::make_beam(beam);
    expect(beam_mesh.has_value(), "valid beam builds a mesh");
    if (beam_mesh.has_value()) {
        const auto bounds = acp::geo3d::bounds(*beam_mesh);
        expect(close(bounds.size().x, 5000.0), "beam span preserved");
        expect(close(bounds.size().y, 300.0), "beam width preserved");
        expect(close(bounds.size().z, 500.0), "beam depth preserved");
        expect(close(bounds.min.z, 2500.0), "beam bottom elevation preserved");
        expect(close(bounds.max.z, 3000.0), "beam top elevation preserved");
    }

    acp::Document document;
    const auto line_id = document.insert(
        acp::LineEntity{{{0.0, 0.0}, {5000.0, 0.0}}});
    expect(line_id != 0, "source wall line inserted");
    const auto walls = arch::walls_from_lines(document, 200.0, 3000.0);
    expect(walls.size() == 1, "visible line becomes one wall");
    const auto beams = arch::beams_from_lines(document, 300.0, 500.0, 3000.0);
    expect(beams.size() == 1, "visible line becomes one beam");
    if (beams.size() == 1) {
        const auto ids = beams.ids();
        const auto* object = beams.find(ids.front());
        expect(object != nullptr, "beam scene object exists");
        if (object != nullptr) {
            expect(object->kind == acp::model3d::ObjectKind::Beam, "beam scene carries semantic kind");
            expect(object->source_entity_id == line_id, "beam keeps 2D source entity linkage");
        }
    }

    const auto poly_id = document.insert(
        acp::PolylineEntity{
            {
                {0.0, 0.0},
                {4000.0, 0.0},
                {4000.0, 3000.0},
                {0.0, 3000.0}
            },
            true
        });
    expect(poly_id != 0, "source slab polyline inserted");
    const auto slabs = arch::slabs_from_closed_polylines(document, 180.0, 0.0);
    expect(slabs.size() == 1, "closed polyline becomes one slab");

    expect(
        !arch::make_wall(
            arch::WallSpec{{0.0, 0.0}, {0.0, 0.0}, 200.0, 3000.0, 0.0})
             .has_value(),
        "zero length wall rejected");
    expect(
        !arch::make_column(
            arch::ColumnSpec{{0.0, 0.0}, 0.0, 400.0, 3000.0, 0.0, 0.0})
             .has_value(),
        "zero width column rejected");
    expect(
        !arch::make_beam(
            arch::BeamSpec{{0.0, 0.0}, {0.0, 0.0}, 300.0, 500.0, 3000.0})
             .has_value(),
        "zero length beam rejected");

    if (failures == 0) {
        std::cout << "Architectural 3D tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
