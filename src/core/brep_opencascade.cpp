#include "acp/brep.hpp"

#ifdef ACP_ENABLE_OPENCASCADE

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace acp::brep {
namespace {

struct OcctShapeData {
    TopoDS_Shape shape;
};

[[nodiscard]] ShapeKind to_shape_kind(TopAbs_ShapeEnum kind) noexcept {
    switch (kind) {
    case TopAbs_VERTEX:
        return ShapeKind::Vertex;
    case TopAbs_EDGE:
        return ShapeKind::Edge;
    case TopAbs_WIRE:
        return ShapeKind::Wire;
    case TopAbs_FACE:
        return ShapeKind::Face;
    case TopAbs_SHELL:
        return ShapeKind::Shell;
    case TopAbs_SOLID:
    case TopAbs_COMPSOLID:
        return ShapeKind::Solid;
    case TopAbs_COMPOUND:
        return ShapeKind::Compound;
    default:
        return ShapeKind::Null;
    }
}

[[nodiscard]] const OcctShapeData* native_data(const Shape& shape) noexcept {
    if (!shape || !shape.native) {
        return nullptr;
    }
    return static_cast<const OcctShapeData*>(shape.native.get());
}

class OpenCascadeKernel final : public Kernel {
public:
    [[nodiscard]] std::string name() const override {
        return "OpenCASCADE Technology";
    }

    [[nodiscard]] bool production_brep() const noexcept override {
        return true;
    }

    [[nodiscard]] std::optional<Shape> extrude(
        const std::vector<geo::Vec2>& closed_profile,
        double height_mm,
        double base_z_mm) override {

        if (closed_profile.size() < 3 || height_mm <= 0.0) {
            return std::nullopt;
        }

        try {
            BRepBuilderAPI_MakePolygon polygon;
            for (const auto& point : closed_profile) {
                polygon.Add(gp_Pnt(point.x, point.y, base_z_mm));
            }
            polygon.Close();
            if (!polygon.IsDone()) {
                return std::nullopt;
            }

            BRepBuilderAPI_MakeFace face(polygon.Wire());
            if (!face.IsDone()) {
                return std::nullopt;
            }

            BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height_mm), true);
            prism.Build();
            if (!prism.IsDone() || prism.Shape().IsNull()) {
                return std::nullopt;
            }
            return wrap(prism.Shape());
        }
        catch (const Standard_Failure&) {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<Shape> boolean_union(
        const Shape& a,
        const Shape& b) override {
        return boolean_op<BRepAlgoAPI_Fuse>(a, b);
    }

    [[nodiscard]] std::optional<Shape> boolean_cut(
        const Shape& a,
        const Shape& b) override {
        return boolean_op<BRepAlgoAPI_Cut>(a, b);
    }

    [[nodiscard]] std::optional<Shape> boolean_intersection(
        const Shape& a,
        const Shape& b) override {
        return boolean_op<BRepAlgoAPI_Common>(a, b);
    }

    [[nodiscard]] std::optional<geo3d::Mesh> tessellate(
        const Shape& shape,
        TessellationOptions options) const override {

        if (options.linear_deflection_mm <= 0.0 ||
            options.angular_deflection_rad <= 0.0) {
            return std::nullopt;
        }

        const auto* data = native_data(shape);
        if (data == nullptr || data->shape.IsNull()) {
            return std::nullopt;
        }

        try {
            BRepMesh_IncrementalMesh mesher(
                data->shape,
                options.linear_deflection_mm,
                false,
                options.angular_deflection_rad,
                true);
            mesher.Perform();

            geo3d::Mesh mesh;
            for (TopExp_Explorer explorer(data->shape, TopAbs_FACE);
                 explorer.More();
                 explorer.Next()) {

                const TopoDS_Face face = TopoDS::Face(explorer.Current());
                TopLoc_Location location;
                const auto triangulation = BRep_Tool::Triangulation(face, location);
                if (triangulation.IsNull() ||
                    triangulation->NbNodes() <= 0 ||
                    triangulation->NbTriangles() <= 0) {
                    continue;
                }

                const std::size_t base_vertex = mesh.vertices.size();
                if (base_vertex + static_cast<std::size_t>(triangulation->NbNodes()) >
                    static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                    return std::nullopt;
                }

                const gp_Trsf transform = location.Transformation();
                mesh.vertices.reserve(
                    mesh.vertices.size() + static_cast<std::size_t>(triangulation->NbNodes()));
                mesh.triangles.reserve(
                    mesh.triangles.size() + static_cast<std::size_t>(triangulation->NbTriangles()));

                for (int node_index = 1;
                     node_index <= triangulation->NbNodes();
                     ++node_index) {
                    const gp_Pnt point = triangulation->Node(node_index).Transformed(transform);
                    mesh.vertices.push_back({point.X(), point.Y(), point.Z()});
                }

                const bool reversed = face.Orientation() == TopAbs_REVERSED;
                for (int triangle_index = 1;
                     triangle_index <= triangulation->NbTriangles();
                     ++triangle_index) {
                    int a = 0;
                    int b = 0;
                    int c = 0;
                    triangulation->Triangle(triangle_index).Get(a, b, c);
                    if (reversed) {
                        std::swap(b, c);
                    }

                    mesh.triangles.push_back({
                        static_cast<std::uint32_t>(base_vertex + static_cast<std::size_t>(a - 1)),
                        static_cast<std::uint32_t>(base_vertex + static_cast<std::size_t>(b - 1)),
                        static_cast<std::uint32_t>(base_vertex + static_cast<std::size_t>(c - 1)),
                    });
                }
            }

            if (!geo3d::valid_mesh(mesh)) {
                return std::nullopt;
            }
            return mesh;
        }
        catch (const Standard_Failure&) {
            return std::nullopt;
        }
    }

private:
    template <typename Operation>
    [[nodiscard]] std::optional<Shape> boolean_op(
        const Shape& a,
        const Shape& b) {

        const auto* a_data = native_data(a);
        const auto* b_data = native_data(b);
        if (a_data == nullptr || b_data == nullptr ||
            a_data->shape.IsNull() || b_data->shape.IsNull()) {
            return std::nullopt;
        }

        try {
            Operation operation(a_data->shape, b_data->shape);
            operation.Build();
            if (!operation.IsDone() || operation.Shape().IsNull()) {
                return std::nullopt;
            }
            return wrap(operation.Shape());
        }
        catch (const Standard_Failure&) {
            return std::nullopt;
        }
    }

    [[nodiscard]] Shape wrap(const TopoDS_Shape& native_shape) {
        if (native_shape.IsNull()) {
            return {};
        }

        auto data = std::make_shared<OcctShapeData>();
        data->shape = native_shape;

        Shape shape;
        shape.id = next_id_++;
        shape.kind = to_shape_kind(native_shape.ShapeType());
        shape.native = std::move(data);
        if (shape.kind == ShapeKind::Null) {
            return {};
        }
        return shape;
    }

    std::uint64_t next_id_{1};
};

} // namespace

std::unique_ptr<Kernel> make_opencascade_kernel() {
    return std::make_unique<OpenCascadeKernel>();
}

} // namespace acp::brep

#endif // ACP_ENABLE_OPENCASCADE
