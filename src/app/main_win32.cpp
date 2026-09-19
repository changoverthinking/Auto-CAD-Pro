#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <windowsx.h>

#include "acp/annotation.hpp"
#include "acp/block.hpp"
#include "acp/bounds.hpp"
#include "acp/document.hpp"
#include "acp/hatch.hpp"
#include "acp/persistence.hpp"
#include "acp/selection.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

using namespace acp;

constexpr int kToolbarHeight = 52;
constexpr int kToolRailWidth = 62;
constexpr int kPropertiesWidth = 230;
constexpr int kStatusHeight = 26;
constexpr int kButtonGap = 6;

enum class Tool {
    Select,
    Line,
    Circle
};

struct AppState {
    Document document;
    BlockLibrary blocks;
    Tool tool{Tool::Select};
    std::optional<EntityId> selected;
    std::optional<geo::Vec2> first_point;
    geo::Vec2 cursor_world{};
    geo::Vec2 view_center{0.0, 0.0};
    double pixels_per_unit{5.0};
    bool panning{false};
    POINT last_pan{};
    std::wstring current_file;
    bool dirty{false};
};

AppState g_state;

struct UiRect {
    RECT rect{};
    const wchar_t* label{};
    int id{};
};

enum CommandId {
    CmdNew = 1,
    CmdOpen,
    CmdSave,
    CmdZoomExtents,
    CmdSelect,
    CmdLine,
    CmdCircle
};

RECT client_canvas(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    rc.left += kToolRailWidth;
    rc.top += kToolbarHeight;
    rc.right -= kPropertiesWidth;
    rc.bottom -= kStatusHeight;
    return rc;
}

std::vector<UiRect> toolbar_buttons(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);

    std::vector<UiRect> result;
    int x = kToolRailWidth + 10;
    constexpr int y = 8;
    constexpr int h = 36;

    auto add = [&](const wchar_t* label, int id, int width) {
        result.push_back({RECT{x, y, x + width, y + h}, label, id});
        x += width + kButtonGap;
    };

    add(L"New", CmdNew, 62);
    add(L"Open", CmdOpen, 66);
    add(L"Save", CmdSave, 66);
    add(L"Zoom Extents", CmdZoomExtents, 116);
    return result;
}

std::vector<UiRect> tool_buttons() {
    std::vector<UiRect> result;
    int y = kToolbarHeight + 10;
    constexpr int x = 7;
    constexpr int w = 48;
    constexpr int h = 42;

    auto add = [&](const wchar_t* label, int id) {
        result.push_back({RECT{x, y, x + w, y + h}, label, id});
        y += h + 8;
    };

    add(L"SEL", CmdSelect);
    add(L"LINE", CmdLine);
    add(L"CIRC", CmdCircle);
    return result;
}

bool contains(const RECT& r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

geo::Vec2 screen_to_world(HWND hwnd, int sx, int sy) {
    const RECT canvas = client_canvas(hwnd);
    const double cx = (canvas.left + canvas.right) * 0.5;
    const double cy = (canvas.top + canvas.bottom) * 0.5;
    return {
        g_state.view_center.x + (static_cast<double>(sx) - cx) / g_state.pixels_per_unit,
        g_state.view_center.y - (static_cast<double>(sy) - cy) / g_state.pixels_per_unit
    };
}

POINT world_to_screen(HWND hwnd, geo::Vec2 p) {
    const RECT canvas = client_canvas(hwnd);
    const double cx = (canvas.left + canvas.right) * 0.5;
    const double cy = (canvas.top + canvas.bottom) * 0.5;
    return {
        static_cast<LONG>(std::lround(cx + (p.x - g_state.view_center.x) * g_state.pixels_per_unit)),
        static_cast<LONG>(std::lround(cy - (p.y - g_state.view_center.y) * g_state.pixels_per_unit))
    };
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(value.begin(), value.end());
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size);
    return result;
}

void draw_line_world(HWND hwnd, HDC dc, geo::Vec2 a, geo::Vec2 b) {
    const POINT pa = world_to_screen(hwnd, a);
    const POINT pb = world_to_screen(hwnd, b);
    MoveToEx(dc, pa.x, pa.y, nullptr);
    LineTo(dc, pb.x, pb.y);
}

void draw_circle_world(HWND hwnd, HDC dc, const geo::Circle& circle) {
    const POINT center = world_to_screen(hwnd, circle.center);
    const int radius = static_cast<int>(std::lround(circle.radius * g_state.pixels_per_unit));
    Ellipse(dc, center.x - radius, center.y - radius, center.x + radius, center.y + radius);
}

void draw_arc_world(HWND hwnd, HDC dc, const geo::Arc& arc) {
    if (!geo::valid_arc(arc)) return;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(geo::arc_sweep(arc) * arc.radius * g_state.pixels_per_unit / 8.0)),
        12, 256);
    const double sweep = geo::arc_sweep(arc);
    geo::Vec2 previous = geo::arc_start_point(arc);
    for (int i = 1; i <= segments; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(segments);
        const double angle = arc.counter_clockwise
            ? arc.start_angle + sweep * t
            : arc.start_angle - sweep * t;
        const geo::Vec2 current{
            arc.center.x + arc.radius * std::cos(angle),
            arc.center.y + arc.radius * std::sin(angle)
        };
        draw_line_world(hwnd, dc, previous, current);
        previous = current;
    }
}

void draw_primitive(HWND hwnd, HDC dc, const BlockPrimitive& primitive) {
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            draw_line_world(hwnd, dc, value.segment.a, value.segment.b);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            draw_circle_world(hwnd, dc, value.circle);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            draw_arc_world(hwnd, dc, value.arc);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            if (value.points.size() < 2) return;
            for (std::size_t i = 1; i < value.points.size(); ++i) {
                draw_line_world(hwnd, dc, value.points[i - 1], value.points[i]);
            }
            if (value.closed && value.points.size() > 2) {
                draw_line_world(hwnd, dc, value.points.back(), value.points.front());
            }
        }
    }, primitive);
}

void draw_entity(HWND hwnd, HDC dc, const Entity& entity) {
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, LineEntity>) {
            draw_line_world(hwnd, dc, value.segment.a, value.segment.b);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            draw_circle_world(hwnd, dc, value.circle);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            draw_arc_world(hwnd, dc, value.arc);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            if (value.points.size() < 2) return;
            for (std::size_t i = 1; i < value.points.size(); ++i) {
                draw_line_world(hwnd, dc, value.points[i - 1], value.points[i]);
            }
            if (value.closed && value.points.size() > 2) {
                draw_line_world(hwnd, dc, value.points.back(), value.points.front());
            }
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            for (const auto& primitive : g_state.blocks.instantiate(value)) {
                draw_primitive(hwnd, dc, primitive);
            }
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            const POINT p = world_to_screen(hwnd, value.position);
            const std::wstring text = utf8_to_wide(value.text);
            SetBkMode(dc, TRANSPARENT);
            TextOutW(dc, p.x, p.y, text.c_str(), static_cast<int>(text.size()));
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            const auto dim = annotation::dimension_line(value);
            const auto ext1 = annotation::first_extension_line(value);
            const auto ext2 = annotation::second_extension_line(value);
            draw_line_world(hwnd, dc, dim.a, dim.b);
            draw_line_world(hwnd, dc, ext1.a, ext1.b);
            draw_line_world(hwnd, dc, ext2.a, ext2.b);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            if (value.boundary.size() < 2) return;
            for (std::size_t i = 1; i < value.boundary.size(); ++i) {
                draw_line_world(hwnd, dc, value.boundary[i - 1], value.boundary[i]);
            }
            if (value.boundary.size() > 2) {
                draw_line_world(hwnd, dc, value.boundary.back(), value.boundary.front());
            }
        }
    }, entity);
}

void draw_grid(HWND hwnd, HDC dc, const RECT& canvas) {
    const double target_pixels = 48.0;
    const double raw_step = target_pixels / g_state.pixels_per_unit;
    const double exponent = std::floor(std::log10(std::max(raw_step, 1e-12)));
    const double base = std::pow(10.0, exponent);
    const double normalized = raw_step / base;
    const double multiplier = normalized <= 1.0 ? 1.0 : (normalized <= 2.0 ? 2.0 : (normalized <= 5.0 ? 5.0 : 10.0));
    const double step = base * multiplier;

    const geo::Vec2 top_left = screen_to_world(hwnd, canvas.left, canvas.top);
    const geo::Vec2 bottom_right = screen_to_world(hwnd, canvas.right, canvas.bottom);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(48, 52, 59));
    HPEN old_pen = static_cast<HPEN>(SelectObject(dc, grid_pen));

    const double x0 = std::floor(top_left.x / step) * step;
    for (double x = x0; x <= bottom_right.x + step; x += step) {
        const POINT a = world_to_screen(hwnd, {x, top_left.y});
        const POINT b = world_to_screen(hwnd, {x, bottom_right.y});
        MoveToEx(dc, a.x, canvas.top, nullptr);
        LineTo(dc, b.x, canvas.bottom);
    }

    const double y_min = bottom_right.y;
    const double y_max = top_left.y;
    const double y0 = std::floor(y_min / step) * step;
    for (double y = y0; y <= y_max + step; y += step) {
        const POINT a = world_to_screen(hwnd, {top_left.x, y});
        const POINT b = world_to_screen(hwnd, {bottom_right.x, y});
        MoveToEx(dc, canvas.left, a.y, nullptr);
        LineTo(dc, canvas.right, b.y);
    }

    SelectObject(dc, old_pen);
    DeleteObject(grid_pen);

    HPEN axis_pen = CreatePen(PS_SOLID, 1, RGB(78, 89, 103));
    old_pen = static_cast<HPEN>(SelectObject(dc, axis_pen));
    const POINT origin = world_to_screen(hwnd, {0, 0});
    if (origin.x >= canvas.left && origin.x <= canvas.right) {
        MoveToEx(dc, origin.x, canvas.top, nullptr);
        LineTo(dc, origin.x, canvas.bottom);
    }
    if (origin.y >= canvas.top && origin.y <= canvas.bottom) {
        MoveToEx(dc, canvas.left, origin.y, nullptr);
        LineTo(dc, canvas.right, origin.y);
    }
    SelectObject(dc, old_pen);
    DeleteObject(axis_pen);
}

void zoom_extents(HWND hwnd) {
    const auto ext = bounds::drawing_bounds(g_state.document, &g_state.blocks);
    if (!ext.has_value()) return;

    const RECT canvas = client_canvas(hwnd);
    const int width = std::max(1L, canvas.right - canvas.left);
    const int height = std::max(1L, canvas.bottom - canvas.top);
    const auto fit = bounds::fit_to_aspect(
        *ext, static_cast<double>(width) / static_cast<double>(height), 0.08);
    if (!fit.has_value()) return;

    g_state.view_center = fit->center;
    g_state.pixels_per_unit = std::max(
        0.0001,
        std::min(
            static_cast<double>(width) / fit->world_width,
            static_cast<double>(height) / fit->world_height));
    InvalidateRect(hwnd, nullptr, FALSE);
}

std::optional<std::wstring> choose_file(HWND hwnd, bool save) {
    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = L"Auto CAD Pro Project (*.acp2d)\0*.acp2d\0All Files (*.*)\0*.*\0";
    dialog.lpstrDefExt = L"acp2d";
    dialog.Flags = OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    const BOOL ok = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    if (!ok) return std::nullopt;
    return std::wstring(path);
}

bool save_project(HWND hwnd, bool save_as) {
    std::wstring path = g_state.current_file;
    if (save_as || path.empty()) {
        const auto selected = choose_file(hwnd, true);
        if (!selected.has_value()) return false;
        path = *selected;
    }

    const std::string data = persistence::serialize_project(g_state.document, g_state.blocks);
    std::ofstream out(std::filesystem::path(path), std::ios::binary);
    if (!out) {
        MessageBoxW(hwnd, L"Could not save project.", L"Auto CAD Pro", MB_OK | MB_ICONERROR);
        return false;
    }
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) {
        MessageBoxW(hwnd, L"Failed while writing project.", L"Auto CAD Pro", MB_OK | MB_ICONERROR);
        return false;
    }

    g_state.current_file = path;
    g_state.dirty = false;
    return true;
}

void open_project(HWND hwnd) {
    const auto selected = choose_file(hwnd, false);
    if (!selected.has_value()) return;

    std::ifstream in(std::filesystem::path(*selected), std::ios::binary);
    if (!in) {
        MessageBoxW(hwnd, L"Could not open project.", L"Auto CAD Pro", MB_OK | MB_ICONERROR);
        return;
    }

    const std::string data(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    auto loaded = persistence::deserialize_project(data);
    if (!loaded.has_value()) {
        MessageBoxW(hwnd, L"Project format is invalid or unsupported.", L"Auto CAD Pro", MB_OK | MB_ICONERROR);
        return;
    }

    g_state.document = std::move(loaded->document);
    g_state.blocks = std::move(loaded->blocks);
    g_state.selected.reset();
    g_state.first_point.reset();
    g_state.current_file = *selected;
    g_state.dirty = false;
    zoom_extents(hwnd);
}

void new_project(HWND hwnd) {
    g_state.document = Document{};
    g_state.blocks = BlockLibrary{};
    g_state.selected.reset();
    g_state.first_point.reset();
    g_state.current_file.clear();
    g_state.dirty = false;
    g_state.view_center = {0, 0};
    g_state.pixels_per_unit = 5.0;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void set_tool(HWND hwnd, Tool tool) {
    g_state.tool = tool;
    g_state.first_point.reset();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void execute_command(HWND hwnd, int id) {
    switch (id) {
        case CmdNew: new_project(hwnd); break;
        case CmdOpen: open_project(hwnd); break;
        case CmdSave: save_project(hwnd, false); break;
        case CmdZoomExtents: zoom_extents(hwnd); break;
        case CmdSelect: set_tool(hwnd, Tool::Select); break;
        case CmdLine: set_tool(hwnd, Tool::Line); break;
        case CmdCircle: set_tool(hwnd, Tool::Circle); break;
        default: break;
    }
}

void paint_button(HDC dc, const UiRect& button, bool active) {
    HBRUSH brush = CreateSolidBrush(active ? RGB(45, 119, 190) : RGB(53, 57, 64));
    FillRect(dc, &button.rect, brush);
    DeleteObject(brush);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 238, 242));
    DrawTextW(dc, button.label, -1, const_cast<RECT*>(&button.rect),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paint_ui(HWND hwnd, HDC dc) {
    RECT client{};
    GetClientRect(hwnd, &client);

    HBRUSH bg = CreateSolidBrush(RGB(32, 35, 40));
    FillRect(dc, &client, bg);
    DeleteObject(bg);

    RECT toolbar{0, 0, client.right, kToolbarHeight};
    HBRUSH toolbar_brush = CreateSolidBrush(RGB(40, 43, 49));
    FillRect(dc, &toolbar, toolbar_brush);

    RECT rail{0, kToolbarHeight, kToolRailWidth, client.bottom - kStatusHeight};
    FillRect(dc, &rail, toolbar_brush);

    RECT props{client.right - kPropertiesWidth, kToolbarHeight, client.right, client.bottom - kStatusHeight};
    FillRect(dc, &props, toolbar_brush);
    DeleteObject(toolbar_brush);

    const RECT canvas = client_canvas(hwnd);
    HBRUSH canvas_brush = CreateSolidBrush(RGB(25, 28, 33));
    FillRect(dc, &canvas, canvas_brush);
    DeleteObject(canvas_brush);

    SaveDC(dc);
    IntersectClipRect(dc, canvas.left, canvas.top, canvas.right, canvas.bottom);
    draw_grid(hwnd, dc, canvas);

    for (const EntityId id : g_state.document.ids()) {
        if (!g_state.document.entity_visible(id)) continue;
        const Entity* entity = g_state.document.find(id);
        if (entity == nullptr) continue;

        const bool selected = g_state.selected.has_value() && *g_state.selected == id;
        HPEN pen = CreatePen(PS_SOLID, selected ? 2 : 1,
                             selected ? RGB(0, 174, 255) : RGB(218, 223, 230));
        HPEN old_pen = static_cast<HPEN>(SelectObject(dc, pen));
        HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(dc, GetStockObject(HOLLOW_BRUSH)));
        SetTextColor(dc, selected ? RGB(0, 174, 255) : RGB(218, 223, 230));

        draw_entity(hwnd, dc, *entity);

        SelectObject(dc, old_brush);
        SelectObject(dc, old_pen);
        DeleteObject(pen);
    }

    if (g_state.first_point.has_value()) {
        HPEN preview_pen = CreatePen(PS_DOT, 1, RGB(0, 174, 255));
        HPEN old = static_cast<HPEN>(SelectObject(dc, preview_pen));
        if (g_state.tool == Tool::Line) {
            draw_line_world(hwnd, dc, *g_state.first_point, g_state.cursor_world);
        } else if (g_state.tool == Tool::Circle) {
            draw_circle_world(hwnd, dc, {
                *g_state.first_point,
                geo::distance(*g_state.first_point, g_state.cursor_world)
            });
        }
        SelectObject(dc, old);
        DeleteObject(preview_pen);
    }
    RestoreDC(dc, -1);

    for (const auto& button : toolbar_buttons(hwnd)) {
        paint_button(dc, button, false);
    }

    for (const auto& button : tool_buttons()) {
        const bool active =
            (button.id == CmdSelect && g_state.tool == Tool::Select) ||
            (button.id == CmdLine && g_state.tool == Tool::Line) ||
            (button.id == CmdCircle && g_state.tool == Tool::Circle);
        paint_button(dc, button, active);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(220, 224, 230));
    RECT title_rect{client.right - kPropertiesWidth + 14, kToolbarHeight + 14,
                    client.right - 10, kToolbarHeight + 40};
    DrawTextW(dc, L"Properties", -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    std::wstringstream info;
    info << L"Entities: " << g_state.document.size() << L"\n";
    if (g_state.selected.has_value()) {
        info << L"Selected ID: " << *g_state.selected << L"\n";
        const EntityProperties* props = g_state.document.properties(*g_state.selected);
        if (props != nullptr) {
            const Layer* layer = g_state.document.layer(props->layer_id);
            info << L"Layer: " << (layer ? utf8_to_wide(layer->name) : L"?") << L"\n";
            info << L"Locked: " << (g_state.document.entity_locked(*g_state.selected) ? L"Yes" : L"No") << L"\n";
        }
    } else {
        info << L"Selected: none\n";
    }

    const std::wstring info_text = info.str();
    RECT info_rect{client.right - kPropertiesWidth + 14, kToolbarHeight + 48,
                   client.right - 12, client.bottom - kStatusHeight - 12};
    DrawTextW(dc, info_text.c_str(), -1, &info_rect, DT_LEFT | DT_TOP | DT_WORDBREAK);

    RECT status{0, client.bottom - kStatusHeight, client.right, client.bottom};
    HBRUSH status_brush = CreateSolidBrush(RGB(38, 41, 46));
    FillRect(dc, &status, status_brush);
    DeleteObject(status_brush);

    const wchar_t* tool_name =
        g_state.tool == Tool::Select ? L"SELECT" :
        g_state.tool == Tool::Line ? L"LINE" : L"CIRCLE";
    std::wstringstream status_text;
    status_text.setf(std::ios::fixed);
    status_text.precision(3);
    status_text << L"  " << tool_name
                << L"     X: " << g_state.cursor_world.x
                << L"   Y: " << g_state.cursor_world.y
                << L"     Zoom: " << g_state.pixels_per_unit << L" px/unit";
    const std::wstring s = status_text.str();
    SetTextColor(dc, RGB(205, 210, 218));
    TextOutW(dc, 8, client.bottom - kStatusHeight + 5, s.c_str(), static_cast<int>(s.size()));
}

void handle_left_click(HWND hwnd, int x, int y) {
    for (const auto& button : toolbar_buttons(hwnd)) {
        if (contains(button.rect, x, y)) {
            execute_command(hwnd, button.id);
            return;
        }
    }
    for (const auto& button : tool_buttons()) {
        if (contains(button.rect, x, y)) {
            execute_command(hwnd, button.id);
            return;
        }
    }

    const RECT canvas = client_canvas(hwnd);
    if (!contains(canvas, x, y)) return;

    const geo::Vec2 world = screen_to_world(hwnd, x, y);

    if (g_state.tool == Tool::Select) {
        const double aperture = 7.0 / g_state.pixels_per_unit;
        const auto hit = selection::hit_test(g_state.document, g_state.blocks, world, aperture);
        g_state.selected = hit.has_value() ? std::optional<EntityId>{hit->id} : std::nullopt;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (!g_state.first_point.has_value()) {
        g_state.first_point = world;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_state.tool == Tool::Line) {
        if (geo::distance(*g_state.first_point, world) > geo::kEpsilon) {
            (void)g_state.document.insert(LineEntity{{*g_state.first_point, world}});
            g_state.dirty = true;
        }
    } else if (g_state.tool == Tool::Circle) {
        const double radius = geo::distance(*g_state.first_point, world);
        if (radius > geo::kEpsilon) {
            (void)g_state.document.insert(CircleEntity{{*g_state.first_point, radius}});
            g_state.dirty = true;
        }
    }

    g_state.first_point.reset();
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            paint_ui(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;

        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lparam);
            const int y = GET_Y_LPARAM(lparam);
            g_state.cursor_world = screen_to_world(hwnd, x, y);

            if (g_state.panning) {
                const int dx = x - g_state.last_pan.x;
                const int dy = y - g_state.last_pan.y;
                g_state.view_center.x -= static_cast<double>(dx) / g_state.pixels_per_unit;
                g_state.view_center.y += static_cast<double>(dy) / g_state.pixels_per_unit;
                g_state.last_pan = {x, y};
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            handle_left_click(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;

        case WM_MBUTTONDOWN:
            g_state.panning = true;
            g_state.last_pan = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            SetCapture(hwnd);
            return 0;

        case WM_MBUTTONUP:
            g_state.panning = false;
            ReleaseCapture();
            return 0;

        case WM_MOUSEWHEEL: {
            POINT mouse{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &mouse);
            const RECT canvas = client_canvas(hwnd);
            if (!contains(canvas, mouse.x, mouse.y)) return 0;

            const geo::Vec2 before = screen_to_world(hwnd, mouse.x, mouse.y);
            const double factor = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1.2 : (1.0 / 1.2);
            g_state.pixels_per_unit = std::clamp(g_state.pixels_per_unit * factor, 0.0001, 100000.0);
            const geo::Vec2 after = screen_to_world(hwnd, mouse.x, mouse.y);
            g_state.view_center = g_state.view_center + (before - after);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                g_state.first_point.reset();
                set_tool(hwnd, Tool::Select);
                return 0;
            }
            if (wparam == VK_DELETE && g_state.selected.has_value()) {
                const EntityId id = *g_state.selected;
                if (!g_state.document.entity_locked(id) && g_state.document.erase(id)) {
                    g_state.selected.reset();
                    g_state.dirty = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (wparam == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                (void)save_project(hwnd, false);
                return 0;
            }
            if (wparam == 'O' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                open_project(hwnd);
                return 0;
            }
            if (wparam == 'N' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                new_project(hwnd);
                return 0;
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDPIAware();

    const wchar_t* class_name = L"AutoCADProMainWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = class_name;

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        class_name,
        L"Auto CAD Pro — 2D",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1440,
        900,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        return 2;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
