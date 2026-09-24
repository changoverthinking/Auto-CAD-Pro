#include "acp/command_catalog.hpp"

#include <array>

namespace acp::commands {
namespace {

using C = CommandDescriptor;
using W = Workspace;
using S = FeatureState;

constexpr std::array<C, 79> kCommands{{
    {"file.new", "pack-a.new-drawing", "New Drawing", W::Global, S::Verified, true},
    {"file.open", "pack-a.open", "Open", W::Global, S::Verified, true},
    {"file.save", "pack-a.save", "Save", W::Global, S::Production, true},
    {"file.save_as", "pack-a.save-as", "Save As", W::Global, S::Production, true},
    {"file.recovery", "pack-a.recovery-manager", "Recovery Manager", W::Global, S::Verified, true},
    {"file.autosave", "pack-a.autosave", "Autosave", W::Global, S::Verified, true},
    {"file.import_dxf", "pack-p.dxf-import", "Import DXF", W::Drafting2D, S::Verified, true},
    {"file.export_dxf", "pack-p.dxf-export", "Export DXF", W::Drafting2D, S::Verified, true},
    {"file.export_svg", "pack-p.svg-export", "Export SVG", W::Drafting2D, S::Verified, true},
    {"file.export_pdf", "pack-p.pdf-export", "Export PDF", W::Documentation, S::Production, true},
    {"file.export_obj", "pack-p.obj-export", "Export OBJ", W::Modeling3D, S::Verified, true},
    {"file.send_blender", "pack-p.blender-send", "Send to Blender", W::Modeling3D, S::Verified, true},

    {"edit.undo", "pack-d.reverse", "Undo", W::Global, S::Verified, true},
    {"edit.redo", "pack-d.reverse", "Redo", W::Global, S::Verified, true},
    {"edit.delete", "pack-d.delete", "Delete", W::Drafting2D, S::Verified, true},
    {"edit.move", "pack-d.move", "Move", W::Drafting2D, S::Verified, true},
    {"edit.copy", "pack-d.copy", "Copy", W::Drafting2D, S::Verified, true},
    {"edit.rotate", "pack-d.rotate", "Rotate", W::Drafting2D, S::Verified, true},
    {"edit.scale", "pack-d.scale", "Scale", W::Drafting2D, S::Verified, true},
    {"edit.mirror", "pack-d.mirror", "Mirror", W::Drafting2D, S::Verified, true},
    {"edit.trim", "pack-d.trim", "Trim", W::Drafting2D, S::Verified, true},
    {"edit.extend", "pack-d.extend", "Extend", W::Drafting2D, S::Verified, true},
    {"edit.offset", "pack-d.offset", "Offset", W::Drafting2D, S::Verified, true},

    {"draw.select", "pack-v.select", "Select", W::Drafting2D, S::Verified, true},
    {"draw.line", "pack-c.line", "Line", W::Drafting2D, S::Verified, true},
    {"draw.polyline", "pack-c.polyline", "Polyline", W::Drafting2D, S::Verified, true},
    {"draw.circle", "pack-c.circle", "Circle", W::Drafting2D, S::Verified, true},
    {"draw.arc", "pack-c.arc", "Arc", W::Drafting2D, S::Verified, true},
    {"draw.rectangle", "pack-c.rectangle", "Rectangle", W::Drafting2D, S::Verified, true},
    {"draw.text", "pack-g.single-line-text", "Text", W::Drafting2D, S::Verified, true},
    {"draw.hatch", "pack-h.hatch", "Hatch", W::Drafting2D, S::Production, true},
    {"draw.dimension_linear", "pack-g.linear-dimension", "Linear Dimension", W::Documentation, S::Verified, true},

    {"snap.object", "pack-e.object-snap", "Object Snap", W::Drafting2D, S::Verified, true},
    {"snap.endpoint", "pack-e.endpoint-snap", "Endpoint Snap", W::Drafting2D, S::Verified, true},
    {"snap.midpoint", "pack-e.midpoint-snap", "Midpoint Snap", W::Drafting2D, S::Verified, true},
    {"snap.center", "pack-e.center-snap", "Center Snap", W::Drafting2D, S::Verified, true},
    {"snap.intersection", "pack-e.intersection-snap", "Intersection Snap", W::Drafting2D, S::Verified, true},

    {"layer.manager", "pack-b.layer-manager", "Layer Manager", W::Drafting2D, S::Verified, true},
    {"layer.new", "pack-f.new-layer", "New Layer", W::Drafting2D, S::Verified, true},
    {"layer.color", "pack-f.layer-color", "Layer Color", W::Drafting2D, S::Verified, true},
    {"layer.linetype", "pack-f.layer-linetype", "Layer Linetype", W::Drafting2D, S::Verified, true},
    {"layer.lineweight", "pack-f.layer-lineweight", "Layer Lineweight", W::Drafting2D, S::Verified, true},
    {"layer.lock", "pack-f.layer-lock", "Lock Layer", W::Drafting2D, S::Verified, true},
    {"layer.unlock", "pack-f.layer-unlock", "Unlock Layer", W::Drafting2D, S::Verified, true},
    {"layer.visible", "pack-f.layer-on", "Layer On", W::Drafting2D, S::Verified, true},

    {"properties.object", "pack-f.object-properties", "Object Properties", W::Global, S::Verified, true},
    {"properties.color", "pack-f.color", "Color", W::Drafting2D, S::Verified, true},
    {"properties.linetype", "pack-f.linetype", "Linetype", W::Drafting2D, S::Verified, true},
    {"properties.lineweight", "pack-f.lineweight", "Lineweight", W::Drafting2D, S::Verified, true},

    {"block.create", "pack-i.create-block", "Create Block", W::Drafting2D, S::Verified, true},
    {"block.insert", "pack-i.insert-block", "Insert Block", W::Drafting2D, S::Verified, true},
    {"block.library", "pack-i.block-library", "Block Library", W::Drafting2D, S::Prototype, false},

    {"view.fit", "pack-l.zoom-extents", "Zoom Extents", W::Global, S::Verified, true},
    {"view.pan", "pack-l.pan", "Pan", W::Global, S::Verified, true},
    {"view.orbit", "pack-l.orbit", "Orbit", W::Modeling3D, S::Verified, true},
    {"view.wireframe", "pack-l.wireframe", "Wireframe", W::Modeling3D, S::Prototype, true},
    {"view.3d_toggle", "pack-b.3d-modeling-workspace", "3D View", W::Modeling3D, S::Verified, true},

    {"layout.page_setup", "pack-j.page-setup", "Page Setup", W::Documentation, S::Production, true},
    {"layout.paper_a4", "pack-j.paper-a4", "A4", W::Documentation, S::Production, true},
    {"layout.paper_a3", "pack-j.paper-a3", "A3", W::Documentation, S::Production, true},
    {"layout.paper_a2", "pack-j.paper-a2", "A2", W::Documentation, S::Production, true},
    {"layout.paper_a1", "pack-j.paper-a1", "A1", W::Documentation, S::Production, true},
    {"layout.paper_a0", "pack-j.paper-a0", "A0", W::Documentation, S::Production, true},
    {"layout.plot", "pack-j.plot", "Plot", W::Documentation, S::Production, true},

    {"bim.project", "pack-o.project", "Project Model", W::Architecture, S::Verified, false},
    {"bim.level", "pack-m.level", "Level", W::Architecture, S::Verified, false},
    {"bim.wall", "pack-m.wall", "Wall", W::Architecture, S::Verified, false},
    {"bim.slab", "pack-m.slab", "Slab", W::Architecture, S::Verified, false},
    {"bim.door", "pack-m.door", "Door", W::Architecture, S::Prototype, false},
    {"bim.window", "pack-m.window", "Window", W::Architecture, S::Prototype, false},
    {"bim.beam", "pack-m.beam", "Beam", W::Structure, S::Verified, false},
    {"bim.column", "pack-m.column", "Column", W::Structure, S::Verified, false},
    {"bim.roof", "pack-m.roof", "Roof", W::Architecture, S::Verified, false},
    {"bim.gable_roof", "pack-m.gable-roof", "Gable Roof", W::Architecture, S::Verified, false},
    {"bim.room", "pack-m.room", "Room", W::Architecture, S::Planned, false},

    {"brep.extrude", "pack-k.extrude", "Extrude", W::Modeling3D, S::Prototype, false},
    {"brep.union", "pack-k.union", "Union", W::Modeling3D, S::Prototype, false},
    {"brep.subtract", "pack-k.subtract", "Subtract", W::Modeling3D, S::Prototype, false},
    {"brep.intersect", "pack-k.intersect", "Intersect", W::Modeling3D, S::Prototype, false},
}};

} // namespace

std::span<const CommandDescriptor> catalog() noexcept {
    return kCommands;
}

const CommandDescriptor* find(std::string_view id) noexcept {
    for (const auto& command : kCommands) {
        if (command.id == id) {
            return &command;
        }
    }
    return nullptr;
}

const CommandDescriptor* find_by_icon(std::string_view icon_id) noexcept {
    for (const auto& command : kCommands) {
        if (command.icon_id == icon_id) {
            return &command;
        }
    }
    return nullptr;
}

bool can_expose_as_active_tool(const CommandDescriptor& command) noexcept {
    return command.ui_reachable && command.state != FeatureState::Planned;
}

} // namespace acp::commands
