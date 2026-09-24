#pragma once

#include "acp/brep.hpp"
#include "acp/project_model.hpp"

#include <optional>

namespace acp::bim {

struct DerivedBrepGeometry {
    ElementId element_id{kInvalidElementId};
    ElementKind kind{ElementKind::GenericModel};
    brep::Shape shape;
    geo3d::Mesh render_mesh;
};

[[nodiscard]] std::optional<DerivedBrepGeometry> build_brep_geometry(
    const ProjectModel& project,
    ElementId element_id,
    brep::Kernel& kernel,
    brep::TessellationOptions tessellation = {});

} // namespace acp::bim
