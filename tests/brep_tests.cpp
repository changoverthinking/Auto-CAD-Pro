#include "acp/brep.hpp"
#include "acp/geometry3d.hpp"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
} // namespace

int main() {
    using acp::geo::Vec2;

    auto kernel = acp::brep::make_default_kernel();
    expect(kernel != nullptr, "default kernel must exist");
    if (!kernel) {
        return 1;
    }

    const std::vector<Vec2> rectangle{
        {0.0, 0.0},
        {5000.0, 0.0},
        {5000.0, 200.0},
        {0.0, 200.0},
    };

    const auto wall = kernel->extrude(rectangle, 3000.0, 100.0);
    expect(wall.has_value(), "rectangular wall profile must extrude");
    if (wall.has_value()) {
        expect(wall->kind == acp::brep::ShapeKind::Solid, "extrusion yields Solid");
        expect(wall->id != 0, "shape has stable nonzero runtime id");

        const auto mesh = kernel->tessellate(*wall);
        expect(mesh.has_value(), "solid must tessellate");
        if (mesh.has_value()) {
            expect(acp::geo3d::valid_mesh(*mesh), "tessellation must be valid mesh");
            const auto bounds = acp::geo3d::bounds(*mesh);
            expect(bounds.initialized, "tessellation bounds initialized");
            const auto size = bounds.size();
            expect(std::abs(size.x - 5000.0) < 1e-9, "wall length preserved in mm");
            expect(std::abs(size.y - 200.0) < 1e-9, "wall thickness preserved in mm");
            expect(std::abs(size.z - 3000.0) < 1e-9, "wall height preserved in mm");
            expect(std::abs(bounds.min.z - 100.0) < 1e-9, "base elevation preserved");
        }
    }

    const auto invalid_height = kernel->extrude(rectangle, 0.0);
    expect(!invalid_height.has_value(), "zero height extrusion rejected");

    const std::vector<Vec2> degenerate{{0.0, 0.0}, {10.0, 0.0}};
    const auto invalid_profile = kernel->extrude(degenerate, 100.0);
    expect(!invalid_profile.has_value(), "degenerate profile rejected");

    if (!kernel->production_brep() && wall.has_value()) {
        expect(!kernel->boolean_union(*wall, *wall).has_value(),
            "compatibility backend must not fake boolean union");
        expect(!kernel->boolean_cut(*wall, *wall).has_value(),
            "compatibility backend must not fake boolean cut");
        expect(!kernel->boolean_intersection(*wall, *wall).has_value(),
            "compatibility backend must not fake boolean intersection");
    }

    if (failures == 0) {
        std::cout << "BRep contract tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
