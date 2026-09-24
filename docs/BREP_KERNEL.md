# BRep kernel architecture

Auto CAD Pro treats BIM semantics and exact geometry as separate layers.

## Source of truth

`acp::bim::ProjectModel` is the canonical semantic model. Wall and Slab elements retain stable `ElementId` values, level relationships, type/material data and authoring parameters. Render meshes are derived data and must not become the source of truth.

## Geometry pipeline

```text
ProjectModel semantic element
        |
        v
BRep kernel shape
        |
        +--> exact topology operations (production backend)
        |
        v
tessellation cache
        |
        v
model3d / viewport / exchange preview
```

## Backends

### Compatibility backend

Always available. It supports profile extrusion and tessellation through the pre-existing mesh implementation. It deliberately reports `production_brep() == false` and refuses Boolean Union/Cut/Intersection rather than pretending those operations are exact.

### OpenCASCADE backend

Enabled with `ACP_ENABLE_OPENCASCADE=ON`. The dependency is supplied by the pinned vcpkg manifest and is validated by `.github/workflows/opencascade-ci.yml`.

The first production-kernel acceptance surface is intentionally small:

- closed planar profile extrusion;
- tessellation with linear/angular deflection controls;
- Boolean Union;
- Boolean Cut;
- Boolean Intersection;
- Wall and Slab derivation through the same `brep::Kernel` interface.

OpenCASCADE is not used as the BIM database. Native `TopoDS_Shape` objects are implementation details held behind `brep::Shape::native`.

## Dependency pin

The repository manifest pins:

- OpenCASCADE `>= 8.0.1`;
- vcpkg builtin baseline `f907dc21e0e8699955b002d0fe7673de5db55fab`.

The OpenCASCADE dependency has default optional features disabled because the current kernel milestone needs modeling/topology only, not OCCT visualization.

## Promotion gate

The OpenCASCADE backend is not considered verified until the dedicated Windows job successfully configures with the pinned vcpkg toolchain, compiles `acp_core`, runs the complete CTest suite, and repeatedly passes the BRep/BIM-BRep regression tests.

Packaging the Windows desktop application with OpenCASCADE enabled is a separate gate because runtime DLL deployment and third-party license notices must be validated before it can replace the compatibility backend in release artifacts.
