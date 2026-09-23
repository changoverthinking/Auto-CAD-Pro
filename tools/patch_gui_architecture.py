from pathlib import Path

path = Path("src/app/main_win.cpp")
text = path.read_text(encoding="utf-8")
original = text


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    '#include "acp/viewport3d.hpp"\n#include "resource.h"',
    '#include "acp/viewport3d.hpp"\n#include "acp/architecture3d_builder.hpp"\n#include "acp/scene_revision_cache.hpp"\n#include "resource.h"',
    "architectural includes")

replace_once(
    '    acp::model3d::Scene scene3d;\n'
    '    acp::viewport3d::Camera camera3d{};\n'
    '    bool scene3d_dirty{true};\n'
    '    bool view_3d{false};\n'
    '    bool orbiting_3d{false};\n'
    '    double extrusion_height{3000.0};',
    '    acp::model3d::Scene scene3d;\n'
    '    acp::viewport3d::Camera camera3d{};\n'
    '    acp::model3d::SceneRevisionCache scene3d_cache{};\n'
    '    acp::architecture3d::ArchitecturalSceneOptions architecture3d_options{};\n'
    '    bool view_3d{false};\n'
    '    bool orbiting_3d{false};',
    "AppState 3D cache/options")

# All explicit dirty assignments become cache invalidations. Document::revision()
# also invalidates automatically, so this is intentionally redundant only at
# project replacement/settings boundaries.
text = text.replace('g_app.scene3d_dirty = true;', 'g_app.scene3d_cache.invalidate();')

replace_once(
    'void rebuild_scene3d() {\n'
    '    g_app.scene3d =\n'
    '        acp::model3d::extrude_closed_polylines(\n'
    '            g_app.document,\n'
    '            g_app.extrusion_height,\n'
    '            0.0);\n'
    '    g_app.scene3d_dirty = false;\n'
    '}\n\n'
    'void ensure_scene3d() {\n'
    '    if (g_app.scene3d_dirty) {\n'
    '        rebuild_scene3d();\n'
    '    }\n'
    '}',
    'std::uint64_t scene3d_settings_signature() {\n'
    '    return acp::architecture3d::settings_signature(\n'
    '        g_app.architecture3d_options);\n'
    '}\n\n'
    'void rebuild_scene3d() {\n'
    '    const std::uint64_t signature = scene3d_settings_signature();\n'
    '    g_app.scene3d = acp::architecture3d::build_architectural_scene(\n'
    '        g_app.document,\n'
    '        g_app.architecture3d_options);\n'
    '    g_app.scene3d_cache.mark_built(g_app.document, signature);\n'
    '}\n\n'
    'void ensure_scene3d() {\n'
    '    const std::uint64_t signature = scene3d_settings_signature();\n'
    '    if (g_app.scene3d_cache.stale(g_app.document, signature)) {\n'
    '        rebuild_scene3d();\n'
    '    }\n'
    '}',
    "revision/signature scene cache")

if 'scene3d_dirty' in text:
    raise SystemExit("scene3d_dirty remains after migration")

replace_once(
    '            L"3D Workspace - draw a closed polyline/rectangle in 2D, then Build 3D.",',
    '            L"Architectural 3D - draw Lines for walls and closed Polylines for slabs, then Build 3D.",',
    "3D empty-state text")

replace_once(
    '        L"3D  |  RMB orbit  |  MMB pan  |  Wheel zoom",',
    '        L"Architectural 3D  |  RMB orbit  |  MMB pan  |  Wheel zoom",',
    "3D viewport hint")

replace_once(
    '            L"3D Workspace    Objects: %zu    Extrude: %.2f    Zoom: %.2f px/unit    RMB Orbit    MMB Pan",\n'
    '            g_app.scene3d.size(),\n'
    '            g_app.extrusion_height,\n'
    '            g_app.camera3d.zoom);',
    '            L"Architectural 3D    Objects: %zu    Wall H: %.0f    Wall T: %.0f    Slab T: %.0f    Roof: %s    Zoom: %.2f",\n'
    '            g_app.scene3d.size(),\n'
    '            g_app.architecture3d_options.wall_height,\n'
    '            g_app.architecture3d_options.wall_thickness,\n'
    '            g_app.architecture3d_options.slab_thickness,\n'
    '            g_app.architecture3d_options.include_roofs ? L"On" : L"Off",\n'
    '            g_app.camera3d.zoom);',
    "3D status text")

text = text.replace(
    'L"No closed 2D polyline is available to build a 3D solid."',
    'L"No architectural 3D geometry could be built. Add visible Lines for walls or a closed Polyline for a slab."')

replace_once(
    'void set_extrusion_height(HWND hwnd) {\n'
    '    const auto entered = prompt_property_value(\n'
    '        hwnd,\n'
    '        L"3D Extrusion Height",\n'
    '        L"Height in drawing units:",\n'
    '        format_property_number(g_app.extrusion_height, 3));\n'
    '    if (!entered.has_value()) {\n'
    '        return;\n'
    '    }\n'
    '    const auto value = parse_finite_double(*entered);\n'
    '    if (!value.has_value() || std::abs(*value) <= acp::geo::kEpsilon) {\n'
    '        show_invalid_property(\n'
    '            hwnd,\n'
    '            L"Extrusion height must be a finite non-zero number.");\n'
    '        return;\n'
    '    }\n'
    '    g_app.extrusion_height = *value;\n'
    '    g_app.scene3d_cache.invalidate();\n'
    '    rebuild_scene3d();\n'
    '    fit_scene3d(hwnd);\n'
    '}',
    'void set_extrusion_height(HWND hwnd) {\n'
    '    const auto entered = prompt_property_value(\n'
    '        hwnd,\n'
    '        L"Architectural Wall Height",\n'
    '        L"Wall height in drawing units:",\n'
    '        format_property_number(g_app.architecture3d_options.wall_height, 3));\n'
    '    if (!entered.has_value()) {\n'
    '        return;\n'
    '    }\n'
    '    const auto value = parse_finite_double(*entered);\n'
    '    if (!value.has_value() || *value <= acp::geo::kEpsilon) {\n'
    '        show_invalid_property(\n'
    '            hwnd,\n'
    '            L"Wall height must be a finite number greater than zero.");\n'
    '        return;\n'
    '    }\n'
    '    g_app.architecture3d_options.wall_height = *value;\n'
    '    g_app.architecture3d_options.roof_eave_z =\n'
    '        g_app.architecture3d_options.base_z + *value;\n'
    '    g_app.scene3d_cache.invalidate();\n'
    '    rebuild_scene3d();\n'
    '    fit_scene3d(hwnd);\n'
    '}',
    "wall height editor")

replace_once(
    'constexpr UINT kGuiTestRightPanelStateMessage = WM_APP + 44;',
    'constexpr UINT kGuiTestRightPanelStateMessage = WM_APP + 44;\n'
    'constexpr UINT kGuiTestArchitectural3DMessage = WM_APP + 45;',
    "architectural GUI test hook constant")

replace_once(
    '#endif\n\nvoid show_file_error(HWND hwnd, const wchar_t* message) {',
    'bool seed_architectural_gui_test(HWND hwnd) {\n'
    '    g_app.document = Document{};\n'
    '    g_app.history = acp::History{};\n'
    '    g_app.blocks = acp::BlockLibrary{};\n'
    '    g_app.active_layer = acp::kDefaultLayerId;\n'
    '    g_app.architecture3d_options =\n'
    '        acp::architecture3d::ArchitecturalSceneOptions{};\n'
    '    g_app.architecture3d_options.include_roofs = true;\n'
    '    g_app.architecture3d_options.roof_eave_z =\n'
    '        g_app.architecture3d_options.wall_height;\n'
    '\n'
    '    const std::array<LineEntity, 4> walls{{\n'
    '        {{{0.0, 0.0}, {6000.0, 0.0}}},\n'
    '        {{{6000.0, 0.0}, {6000.0, 4000.0}}},\n'
    '        {{{6000.0, 4000.0}, {0.0, 4000.0}}},\n'
    '        {{{0.0, 4000.0}, {0.0, 0.0}}}\n'
    '    }};\n'
    '    for (const LineEntity& wall : walls) {\n'
    '        if (!apply_history(std::make_unique<acp::AddEntityCommand>(wall))) {\n'
    '            return false;\n'
    '        }\n'
    '    }\n'
    '    if (!apply_history(std::make_unique<acp::AddEntityCommand>(\n'
    '            PolylineEntity{{\n'
    '                {0.0, 0.0}, {6000.0, 0.0},\n'
    '                {6000.0, 4000.0}, {0.0, 4000.0}}, true}))) {\n'
    '        return false;\n'
    '    }\n'
    '\n'
    '    g_app.scene3d_cache.invalidate();\n'
    '    rebuild_scene3d();\n'
    '    g_app.view_3d = true;\n'
    '    fit_scene3d(hwnd);\n'
    '    SetWindowTextW(hwnd, L"Auto CAD Pro - Architectural 3D Workspace");\n'
    '    InvalidateRect(hwnd, nullptr, FALSE);\n'
    '    UpdateWindow(hwnd);\n'
    '    return g_app.scene3d.size() >= 6;\n'
    '}\n'
    '#endif\n\nvoid show_file_error(HWND hwnd, const wchar_t* message) {',
    "architectural GUI seed helper")

replace_once(
    '        case kGuiTestRightPanelStateMessage: {\n'
    '            unsigned state = 0;\n'
    '            if (g_app.right_panel_visible) state |= 1u;\n'
    '            if (g_app.right_panel_pinned) state |= 2u;\n'
    '            if (g_app.right_panel_collapsed) state |= 4u;\n'
    '            if (g_app.right_panel_auto_hide_expanded) state |= 8u;\n'
    '            if (g_app.right_panel_resizing) state |= 16u;\n'
    '            return static_cast<LRESULT>(state);\n'
    '        }',
    '        case kGuiTestRightPanelStateMessage: {\n'
    '            unsigned state = 0;\n'
    '            if (g_app.right_panel_visible) state |= 1u;\n'
    '            if (g_app.right_panel_pinned) state |= 2u;\n'
    '            if (g_app.right_panel_collapsed) state |= 4u;\n'
    '            if (g_app.right_panel_auto_hide_expanded) state |= 8u;\n'
    '            if (g_app.right_panel_resizing) state |= 16u;\n'
    '            return static_cast<LRESULT>(state);\n'
    '        }\n'
    '        case kGuiTestArchitectural3DMessage:\n'
    '            return seed_architectural_gui_test(hwnd) ? 1 : 0;',
    "architectural GUI message handler")

text = text.replace(
    'SetWindowTextW(hwnd, L"Auto CAD Pro - 3D Workspace");',
    'SetWindowTextW(hwnd, L"Auto CAD Pro - Architectural 3D Workspace");')

replace_once(
    'AppendMenuW(model3d_menu, MF_STRING, kMenuBuild3D, L"&Build 3D from Closed Polylines");',
    'AppendMenuW(model3d_menu, MF_STRING, kMenuBuild3D, L"&Build Architectural 3D");',
    "3D build menu label")
replace_once(
    'AppendMenuW(model3d_menu, MF_STRING, kMenuSetExtrusionHeight, L"Set &Extrusion Height...");',
    'AppendMenuW(model3d_menu, MF_STRING, kMenuSetExtrusionHeight, L"Set &Wall Height...");',
    "wall height menu label")

if text == original:
    raise SystemExit("patch produced no changes")
path.write_text(text, encoding="utf-8", newline="\n")
print("Architectural GUI integration patch applied")
