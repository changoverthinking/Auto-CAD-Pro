#pragma once

#include "acp/geometry2d.hpp"
#include "acp/geometry3d.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace acp::brep {

enum class ShapeKind : std::uint8_t {
    Null = 0,
    Vertex,
    Edge,
    Wire,
    Face,
    Shell,
    Solid,
    Compound,
};

struct Shape {
    std::uint64_t id{};
    ShapeKind kind{ShapeKind::Null};
    std::shared_ptr<const void> native;

    [[nodiscard]] explicit operator bool() const noexcept {
        return id != 0 && kind != ShapeKind::Null;
    }
};

struct TessellationOptions {
    double linear_deflection_mm{1.0};
    double angular_deflection_rad{0.35};
};

class Kernel {
public:
    virtual ~Kernel() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual bool production_brep() const noexcept = 0;

    [[nodiscard]] virtual std::optional<Shape> extrude(
        const std::vector<geo::Vec2>& closed_profile,
        double height_mm,
        double base_z_mm = 0.0) = 0;

    [[nodiscard]] virtual std::optional<Shape> boolean_union(
        const Shape& a,
        const Shape& b) = 0;
    [[nodiscard]] virtual std::optional<Shape> boolean_cut(
        const Shape& a,
        const Shape& b) = 0;
    [[nodiscard]] virtual std::optional<Shape> boolean_intersection(
        const Shape& a,
        const Shape& b) = 0;

    [[nodiscard]] virtual std::optional<geo3d::Mesh> tessellate(
        const Shape& shape,
        TessellationOptions options = {}) const = 0;
};

// Always available. This is a compatibility backend used to keep the current
// Windows build green while the OpenCASCADE backend is provisioned. It is not
// a production BRep implementation and intentionally supports only extrusion
// plus tessellation of those extrusions.
[[nodiscard]] std::unique_ptr<Kernel> make_compatibility_kernel();

// Returns an OpenCASCADE-backed kernel when Auto CAD Pro is built with
// ACP_ENABLE_OPENCASCADE. Returns nullptr otherwise.
[[nodiscard]] std::unique_ptr<Kernel> make_opencascade_kernel();

// Prefers OpenCASCADE when available; otherwise returns the compatibility
// backend. Call production_brep() before enabling operations that require real
// topology/booleans.
[[nodiscard]] std::unique_ptr<Kernel> make_default_kernel();

} // namespace acp::brep
