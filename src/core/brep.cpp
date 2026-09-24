#include "acp/brep.hpp"

#include "acp/model3d.hpp"

#include <memory>
#include <unordered_map>

namespace acp::brep {
namespace {

struct CompatibilityShapeData {
    geo3d::Mesh mesh;
};

class CompatibilityKernel final : public Kernel {
public:
    [[nodiscard]] std::string name() const override {
        return "AutoCADPro Compatibility BRep";
    }

    [[nodiscard]] bool production_brep() const noexcept override {
        return false;
    }

    [[nodiscard]] std::optional<Shape> extrude(
        const std::vector<geo::Vec2>& closed_profile,
        double height_mm,
        double base_z_mm) override {

        const auto mesh = model3d::extrude_polygon(
            closed_profile,
            height_mm,
            base_z_mm);
        if (!mesh.has_value()) {
            return std::nullopt;
        }

        auto data = std::make_shared<CompatibilityShapeData>();
        data->mesh = *mesh;

        Shape shape;
        shape.id = next_id_++;
        shape.kind = ShapeKind::Solid;
        shape.native = std::move(data);
        return shape;
    }

    [[nodiscard]] std::optional<Shape> boolean_union(
        const Shape&,
        const Shape&) override {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<Shape> boolean_cut(
        const Shape&,
        const Shape&) override {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<Shape> boolean_intersection(
        const Shape&,
        const Shape&) override {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<geo3d::Mesh> tessellate(
        const Shape& shape,
        TessellationOptions options) const override {

        if (!shape || shape.kind != ShapeKind::Solid || !shape.native) {
            return std::nullopt;
        }
        if (options.linear_deflection_mm <= 0.0 ||
            options.angular_deflection_rad <= 0.0) {
            return std::nullopt;
        }

        const auto data = std::static_pointer_cast<const CompatibilityShapeData>(
            shape.native);
        if (!data || !geo3d::valid_mesh(data->mesh)) {
            return std::nullopt;
        }
        return data->mesh;
    }

private:
    std::uint64_t next_id_{1};
};

} // namespace

std::unique_ptr<Kernel> make_compatibility_kernel() {
    return std::make_unique<CompatibilityKernel>();
}

#ifndef ACP_ENABLE_OPENCASCADE
std::unique_ptr<Kernel> make_opencascade_kernel() {
    return nullptr;
}
#endif

std::unique_ptr<Kernel> make_default_kernel() {
    if (auto kernel = make_opencascade_kernel()) {
        return kernel;
    }
    return make_compatibility_kernel();
}

} // namespace acp::brep
