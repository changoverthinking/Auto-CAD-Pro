#include "acp/architecture3d.hpp"

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

bool close(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) <= epsilon;
}

} // namespace

int main() {
    using acp::geo::Vec2;

    const acp::architecture3d::WallSpec wall{
        {0.0, 0.0},
        {5000.0, 0.0},
        200.0,
        3000.0,
        0.0
    };
    const auto wall_mesh = acp::architecture3d::make_wall(wall);
    expect(wall_mesh.has_value(), "valid wall builds a mesh");
    if (wall_mesh.has_value()) {
        expect(wall_mesh->vertices.size() == 8, "wall has eight vertices");
        expect(wall_mesh->triangles.size() == 12, "wall has twelve triangles");
        const auto bounds = acp::geo3d::bounds(*wall_mesh);
        expect(close(bounds.size().x, 5000.0), "wall length preserved");
        expect(close(bounds.size().y, 200.0), "wall thickness preserved");
        expect(close(bounds.size().z, 3000.0), "wall height preserved");
    }

    const acp::architecture3d::SlabSpec slab{
        {
            {0.0, 0.0},
            {4000.0, 0.0},
            {4000.0, 3000.0},
            {0.0, 3000.0}
        },
        180.0,
        0.0
    };
    const auto slab_mesh = acp::architecture3d::make_slab(slab);
    expect(slab_mesh.has_value(), "valid slab builds a mesh");
    if (slab_mesh.has_value()) {
        const auto bounds = acp::geo3d::bounds(*slab_mesh);
        expect(close(bounds.min.z, -180.0), "slab extrudes below top elevation");
        expect(close(bounds.max.z, 0.0), "slab top elevation preserved");
    }

    const acp::architecture3d::ColumnSpec column{
        {1000.0, 1000.0},
        400.0,
        600.0,
        3200.0,
        100.0,
        0.0
    };
    const auto column_mesh = acp::architecture3d::make_column(column);
    expect(column_mesh.has_value(), "valid column builds a mesh");
    if (column_mesh.has_value()) {
        const auto bounds = acp::geo3d::bounds(*column_mesh);
        expect(close(bounds.size().x, 400.0), "column width preserved");
        expect(close(bounds.size().y, 600.0), "column depth preserved");
        expect(close(bounds.min.z, 100.0), "column base elevation preserved");
        expect(close(bounds.max.z, 3300.0), "column top elevation preserved");
    }

    acp::Document document;
    const auto line_id = document.insert(
        acp::LineEntity{{{0.0, 0.0}, {5000.0, 0.0}}});
    expect(line_id != 0, "source wall line inserted");
    const auto walls = acp::architecture3d::walls_from_lines(
        document, 200.0, 3000.0);
    expect(walls.size() == 1, "visible line becomes one wall");

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
    const auto slabs = acp::architecture3d::slabs_from_closed_polylines(
        document, 180.0, 0.0);
    expect(slabs.size() == 1, "closed polyline becomes one slab");

    expect(
        !acp::architecture3d::make_wall(
            acp::architecture3d::WallSpec{{0.0, 0.0}, {0.0, 0.0}, 200.0, 3000.0, 0.0})
             .has_value(),
        "zero length wall rejected");
    expect(
        !acp::architecture3d::make_column(
            acp::architecture3d::ColumnSpec{{0.0, 0.0}, 0.0, 400.0, 3000.0, 0.0, 0.0})
             .has_value(),
        "zero width column rejected");

    if (failures == 0) {
        std::cout << "Architectural 3D tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
