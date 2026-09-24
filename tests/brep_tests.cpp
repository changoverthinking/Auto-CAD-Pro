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
            expect(std::abs(size.x - 5000.0) < 1e-6, "wall length preserved in mm");
            expect(std::abs(size.y - 200.0) < 1e-6, "wall thickness preserved in mm");
            expect(std::abs(size.z - 3000.0) < 1e-6, "wall height preserved in mm");
            expect(std::abs(bounds.min.z - 100.0) < 1e-6, "base elevation preserved");
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

    if (kernel->production_brep() && wall.has_value()) {
        expect(kernel->name().find("OpenCASCADE") != std::string::npos,
            "production backend identifies OpenCASCADE");

        const std::vector<Vec2> overlapping_rectangle{
            {2500.0, 0.0},
            {7500.0, 0.0},
            {7500.0, 200.0},
            {2500.0, 200.0},
        };
        const auto other = kernel->extrude(overlapping_rectangle, 3000.0, 100.0);
        expect(other.has_value(), "second OCCT solid must extrude");
        if (other.has_value()) {
            const auto fused = kernel->boolean_union(*wall, *other);
            const auto cut = kernel->boolean_cut(*wall, *other);
            const auto common = kernel->boolean_intersection(*wall, *other);
            expect(fused.has_value(), "OpenCASCADE union must succeed");
            expect(cut.has_value(), "OpenCASCADE cut must succeed");
            expect(common.has_value(), "OpenCASCADE intersection must succeed");

            if (fused.has_value()) {
                const auto mesh = kernel->tessellate(*fused);
                expect(mesh.has_value() && acp::geo3d::valid_mesh(*mesh),
                    "OpenCASCADE union must tessellate");
            }
            if (cut.has_value()) {
                const auto mesh = kernel->tessellate(*cut);
                expect(mesh.has_value() && acp::geo3d::valid_mesh(*mesh),
                    "OpenCASCADE cut must tessellate");
            }
            if (common.has_value()) {
                const auto mesh = kernel->tessellate(*common);
                expect(mesh.has_value() && acp::geo3d::valid_mesh(*mesh),
                    "OpenCASCADE intersection must tessellate");
            }
        }
    }

    if (failures == 0) {
        std::cout << "BRep contract tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
