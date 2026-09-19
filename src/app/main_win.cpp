#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>

#include "acp/document.hpp"
#include "acp/bounds.hpp"
#include "acp/history.hpp"
#include "acp/dxf.hpp"
#include "acp/persistence.hpp"
#include "acp/selection.hpp"
#include "acp/transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <type_traits>

namespace {

using acp::CircleEntity;
using acp::Document;
using acp::LineEntity;
using acp::PolylineEntity;
using acp::ArcEntity;
using acp::geo::Vec2;

enum class Tool {
    Select,
    Line,
    Circle,
    Move,
    Copy,
    Rotate
};

struct AppState {
    Document document;
    acp::History history;
    acp::BlockLibrary blocks;
    std::optional<acp::EntityId> selected;
    Tool tool{Tool::Select};
    double zoom{1.0};
    Vec2 view_center{0.0, 0.0};
    bool has_first_point{false};
    Vec2 first_point{};
    bool panning{false};
    POINT last_mouse{};
    POINT cursor{};
};

AppState g_app;

constexpr int kToolbarHeight = 44;
constexpr int kStatusHeight = 24;
constexpr int kMenuNew = 1001;
constexpr int kMenuExit = 1002;
constexpr int kMenuOpen = 1003;
constexpr int kMenuSave = 1004;
constexpr int kMenuImportDxf = 1005;
constexpr int kMenuExportDxf = 1006;
constexpr int kMenuZoomExtents = 1007;
constexpr int kToolSelect = 2001;
constexpr int kToolLine = 2002;
constexpr int kToolCircle = 2003;
constexpr int kToolMove = 2004;
constexpr int kToolCopy = 2005;
constexpr int kToolRotate = 2006;

const wchar_t* tool_name(Tool tool) {
    switch (tool) {
        case Tool::Select: return L"Select";
        case Tool::Line: return L"Line";
        case Tool::Circle: return L"Circle";
        case Tool::Move: return L"Move";
        case Tool::Copy: return L"Copy";
        case Tool::Rotate: return L"Rotate";
    }
    return L"Select";
}

RECT canvas_rect(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    rc.top += kToolbarHeight;
    rc.bottom = std::max(rc.top, rc.bottom - kStatusHeight);
    return rc;
}

POINT world_to_screen(HWND hwnd, Vec2 p) {
    const RECT rc = canvas_rect(hwnd);
    const double cx = (static_cast<double>(rc.left) + static_cast<double>(rc.right)) * 0.5;
    const double cy = (static_cast<double>(rc.top) + static_cast<double>(rc.bottom)) * 0.5;
    return POINT{
        static_cast<LONG>(std::lround(cx + (p.x - g_app.view_center.x) * g_app.zoom)),
        static_cast<LONG>(std::lround(cy - (p.y - g_app.view_center.y) * g_app.zoom))
    };
}

Vec2 screen_to_world(HWND hwnd, POINT p) {
    const RECT rc = canvas_rect(hwnd);
    const double cx = (static_cast<double>(rc.left) + static_cast<double>(rc.right)) * 0.5;
    const double cy = (static_cast<double>(rc.top) + static_cast<double>(rc.bottom)) * 0.5;
    return {
        g_app.view_center.x + (static_cast<double>(p.x) - cx) / g_app.zoom,
        g_app.view_center.y - (static_cast<double>(p.y) - cy) / g_app.zoom
    };
}

void set_tool(HWND hwnd, Tool tool) {
    g_app.tool = tool;
    g_app.has_first_point = false;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void draw_grid(HWND hwnd, HDC dc, const RECT& rc) {
    const double desired_pixels = 60.0;
    const double raw_step = desired_pixels / g_app.zoom;
    const double power = std::pow(10.0, std::floor(std::log10(std::max(raw_step, 1e-9))));
    const double normalized = raw_step / power;
    const double step = (normalized < 2.0 ? 1.0 : normalized < 5.0 ? 2.0 : 5.0) * power;

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(48, 52, 60));
    HPEN axis_pen = CreatePen(PS_SOLID, 1, RGB(75, 80, 92));
    HGDIOBJ old_pen = SelectObject(dc, grid_pen);

    const Vec2 top_left = screen_to_world(hwnd, POINT{rc.left, rc.top});
    const Vec2 bottom_right = screen_to_world(hwnd, POINT{rc.right, rc.bottom});

    double start_x = std::floor(top_left.x / step) * step;
    double end_x = std::ceil(bottom_right.x / step) * step;
    if (start_x > end_x) {
        std::swap(start_x, end_x);
    }

    double min_y = std::min(top_left.y, bottom_right.y);
    double max_y = std::max(top_left.y, bottom_right.y);
    double start_y = std::floor(min_y / step) * step;
    double end_y = std::ceil(max_y / step) * step;

    for (double x = start_x; x <= end_x + step * 0.25; x += step) {
        SelectObject(dc, std::abs(x) < step * 0.1 ? axis_pen : grid_pen);
        const POINT a = world_to_screen(hwnd, {x, min_y});
        const POINT b = world_to_screen(hwnd, {x, max_y});
        MoveToEx(dc, a.x, a.y, nullptr);
        LineTo(dc, b.x, b.y);
    }

    for (double y = start_y; y <= end_y + step * 0.25; y += step) {
        SelectObject(dc, std::abs(y) < step * 0.1 ? axis_pen : grid_pen);
        const POINT a = world_to_screen(hwnd, {start_x, y});
        const POINT b = world_to_screen(hwnd, {end_x, y});
        MoveToEx(dc, a.x, a.y, nullptr);
        LineTo(dc, b.x, b.y);
    }

    SelectObject(dc, old_pen);
    DeleteObject(axis_pen);
    DeleteObject(grid_pen);
}

void draw_arc(HWND hwnd, HDC dc, const ArcEntity& entity) {
    const auto& arc = entity.arc;
    const POINT center = world_to_screen(hwnd, arc.center);
    const LONG r = std::max<LONG>(1, static_cast<LONG>(std::lround(arc.radius * g_app.zoom)));
    const Vec2 start_world{
        arc.center.x + std::cos(arc.start_angle) * arc.radius,
        arc.center.y + std::sin(arc.start_angle) * arc.radius
    };
    const Vec2 end_world{
        arc.center.x + std::cos(arc.end_angle) * arc.radius,
        arc.center.y + std::sin(arc.end_angle) * arc.radius
    };
    const POINT start = world_to_screen(hwnd, start_world);
    const POINT end = world_to_screen(hwnd, end_world);

    if (arc.counter_clockwise) {
        Arc(dc, center.x - r, center.y - r, center.x + r, center.y + r,
            start.x, start.y, end.x, end.y);
    } else {
        Arc(dc, center.x - r, center.y - r, center.x + r, center.y + r,
            end.x, end.y, start.x, start.y);
    }
}

void draw_document(HWND hwnd, HDC dc) {
    HPEN entity_pen = CreatePen(PS_SOLID, 2, RGB(229, 232, 239));
    HGDIOBJ old_pen = SelectObject(dc, entity_pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    for (const auto id : g_app.document.ids()) {
        if (!g_app.document.entity_visible(id)) {
            continue;
        }
        const auto* entity = g_app.document.find(id);
        if (entity == nullptr) {
            continue;
        }

        std::visit([&](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, LineEntity>) {
                const POINT a = world_to_screen(hwnd, item.segment.a);
                const POINT b = world_to_screen(hwnd, item.segment.b);
                MoveToEx(dc, a.x, a.y, nullptr);
                LineTo(dc, b.x, b.y);
            } else if constexpr (std::is_same_v<T, CircleEntity>) {
                const POINT center = world_to_screen(hwnd, item.circle.center);
                const LONG r = std::max<LONG>(1, static_cast<LONG>(std::lround(item.circle.radius * g_app.zoom)));
                Ellipse(dc, center.x - r, center.y - r, center.x + r, center.y + r);
            } else if constexpr (std::is_same_v<T, ArcEntity>) {
                draw_arc(hwnd, dc, item);
            } else if constexpr (std::is_same_v<T, PolylineEntity>) {
                if (item.points.size() < 2) {
                    return;
                }
                const POINT first = world_to_screen(hwnd, item.points.front());
                MoveToEx(dc, first.x, first.y, nullptr);
                for (std::size_t i = 1; i < item.points.size(); ++i) {
                    const POINT p = world_to_screen(hwnd, item.points[i]);
                    LineTo(dc, p.x, p.y);
                }
                if (item.closed) {
                    LineTo(dc, first.x, first.y);
                }
            }
        }, *entity);
    }

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(entity_pen);
}


void draw_selection_overlay(HWND hwnd, HDC dc) {
    if (!g_app.selected.has_value()) {
        return;
    }
    const acp::Entity* entity = g_app.document.find(*g_app.selected);
    if (entity == nullptr) {
        return;
    }
    const auto box = acp::bounds::entity_bounds(*entity);
    if (!box.has_value()) {
        return;
    }

    const POINT min_point = world_to_screen(hwnd, box->min);
    const POINT max_point = world_to_screen(hwnd, box->max);
    RECT selection_rect{
        std::min(min_point.x, max_point.x),
        std::min(min_point.y, max_point.y),
        std::max(min_point.x, max_point.x),
        std::max(min_point.y, max_point.y)
    };

    HPEN pen = CreatePen(PS_DOT, 1, RGB(255, 190, 70));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, selection_rect.left, selection_rect.top, selection_rect.right, selection_rect.bottom);

    const POINT grips[] = {
        {selection_rect.left, selection_rect.top},
        {selection_rect.right, selection_rect.top},
        {selection_rect.right, selection_rect.bottom},
        {selection_rect.left, selection_rect.bottom},
        {(selection_rect.left + selection_rect.right) / 2,
         (selection_rect.top + selection_rect.bottom) / 2}
    };

    HBRUSH grip_brush = CreateSolidBrush(RGB(255, 190, 70));
    SelectObject(dc, grip_brush);
    for (const POINT grip : grips) {
        Rectangle(dc, grip.x - 3, grip.y - 3, grip.x + 4, grip.y + 4);
    }

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(grip_brush);
    DeleteObject(pen);
}

void draw_preview(HWND hwnd, HDC dc) {
    if (!g_app.has_first_point || g_app.tool == Tool::Select) {
        return;
    }

    const Vec2 current = screen_to_world(hwnd, g_app.cursor);
    HPEN preview_pen = CreatePen(PS_DOT, 1, RGB(93, 190, 255));
    HGDIOBJ old_pen = SelectObject(dc, preview_pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    const POINT first = world_to_screen(hwnd, g_app.first_point);
    const POINT second = world_to_screen(hwnd, current);

    if (g_app.tool == Tool::Line ||
        g_app.tool == Tool::Move ||
        g_app.tool == Tool::Copy ||
        g_app.tool == Tool::Rotate) {
        MoveToEx(dc, first.x, first.y, nullptr);
        LineTo(dc, second.x, second.y);
    } else if (g_app.tool == Tool::Circle) {
        const double radius = acp::geo::distance(g_app.first_point, current);
        const LONG r = std::max<LONG>(1, static_cast<LONG>(std::lround(radius * g_app.zoom)));
        Ellipse(dc, first.x - r, first.y - r, first.x + r, first.y + r);
    }

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(preview_pen);
}

void draw_toolbar(HDC dc, const RECT& client) {
    RECT bar{client.left, client.top, client.right, client.top + kToolbarHeight};
    HBRUSH brush = CreateSolidBrush(RGB(39, 43, 50));
    FillRect(dc, &bar, brush);
    DeleteObject(brush);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 237, 242));

    RECT title{12, 0, 180, kToolbarHeight};
    DrawTextW(dc, L"AUTO CAD PRO", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    struct Button { RECT rect; const wchar_t* text; Tool tool; };
    const Button buttons[] = {
        {{190, 7, 265, 37}, L"Select", Tool::Select},
        {{272, 7, 337, 37}, L"Line", Tool::Line},
        {{344, 7, 419, 37}, L"Circle", Tool::Circle},
        {{426, 7, 491, 37}, L"Move", Tool::Move},
        {{498, 7, 563, 37}, L"Copy", Tool::Copy},
        {{570, 7, 645, 37}, L"Rotate", Tool::Rotate}
    };

    for (const auto& button : buttons) {
        HBRUSH button_brush = CreateSolidBrush(
            g_app.tool == button.tool ? RGB(58, 118, 181) : RGB(56, 61, 70));
        FillRect(dc, &button.rect, button_brush);
        DeleteObject(button_brush);
        DrawTextW(dc, button.text, -1, const_cast<RECT*>(&button.rect),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void draw_status(HWND hwnd, HDC dc, const RECT& client) {
    RECT bar{client.left, client.bottom - kStatusHeight, client.right, client.bottom};
    HBRUSH brush = CreateSolidBrush(RGB(32, 35, 41));
    FillRect(dc, &bar, brush);
    DeleteObject(brush);

    const Vec2 cursor_world = screen_to_world(hwnd, g_app.cursor);
    wchar_t buffer[256]{};
    swprintf_s(buffer, L"Tool: %s    X: %.2f    Y: %.2f    Zoom: %.0f%%    Entities: %zu    Selected: %llu    Undo: %zu",
               tool_name(g_app.tool), cursor_world.x, cursor_world.y,
               g_app.zoom * 100.0, g_app.document.size(),
               static_cast<unsigned long long>(g_app.selected.value_or(0)),
               g_app.history.undo_size());

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(190, 195, 205));
    RECT text_rect{10, bar.top, bar.right - 10, bar.bottom};
    DrawTextW(dc, buffer, -1, &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}


std::optional<std::filesystem::path> choose_file(
    HWND hwnd,
    bool save,
    const wchar_t* filter,
    const wchar_t* default_extension) {

    wchar_t buffer[4096]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
    dialog.lpstrFilter = filter;
    dialog.lpstrDefExt = default_extension;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST |
                   (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    const BOOL ok = save
        ? GetSaveFileNameW(&dialog)
        : GetOpenFileNameW(&dialog);
    if (!ok) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer);
}

std::optional<std::string> read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    std::string data(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (!input.good() && !input.eof()) {
        return std::nullopt;
    }
    return data;
}

bool write_text_file(const std::filesystem::path& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return output.good();
}

void show_file_error(HWND hwnd, const wchar_t* message) {
    MessageBoxW(hwnd, message, L"Auto CAD Pro", MB_OK | MB_ICONERROR);
}

void fit_drawing(HWND hwnd) {
    const auto drawing = acp::bounds::drawing_bounds(g_app.document, &g_app.blocks);
    if (!drawing.has_value()) {
        return;
    }

    const RECT rc = canvas_rect(hwnd);
    const double width_px = std::max(1L, rc.right - rc.left);
    const double height_px = std::max(1L, rc.bottom - rc.top);
    const auto fit = acp::bounds::fit_to_aspect(
        *drawing, width_px / height_px, 0.08);
    if (!fit.has_value()) {
        return;
    }

    g_app.view_center = fit->center;
    const double world_width = std::max(fit->world_width, acp::geo::kEpsilon);
    const double world_height = std::max(fit->world_height, acp::geo::kEpsilon);
    g_app.zoom = std::clamp(
        std::min(width_px / world_width, height_px / world_height),
        0.02, 200.0);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void open_project(HWND hwnd) {
    const auto path = choose_file(
        hwnd, false,
        L"Auto CAD Pro Project (*.acp)\0*.acp\0All Files (*.*)\0*.*\0\0",
        L"acp");
    if (!path.has_value()) {
        return;
    }

    const auto data = read_text_file(*path);
    if (!data.has_value()) {
        show_file_error(hwnd, L"Could not read the selected project.");
        return;
    }

    auto project = acp::persistence::deserialize_project(*data);
    if (!project.has_value()) {
        show_file_error(hwnd, L"The project file is invalid or unsupported.");
        return;
    }

    g_app.document = std::move(project->document);
    g_app.blocks = std::move(project->blocks);
    g_app.history = acp::History{};
    g_app.selected.reset();
    g_app.has_first_point = false;
    fit_drawing(hwnd);
}

void save_project(HWND hwnd) {
    const auto path = choose_file(
        hwnd, true,
        L"Auto CAD Pro Project (*.acp)\0*.acp\0All Files (*.*)\0*.*\0\0",
        L"acp");
    if (!path.has_value()) {
        return;
    }

    const std::string data =
        acp::persistence::serialize_project(g_app.document, g_app.blocks);
    if (!write_text_file(*path, data)) {
        show_file_error(hwnd, L"Could not save the project.");
    }
}

void import_dxf(HWND hwnd) {
    const auto path = choose_file(
        hwnd, false,
        L"DXF Drawing (*.dxf)\0*.dxf\0All Files (*.*)\0*.*\0\0",
        L"dxf");
    if (!path.has_value()) {
        return;
    }

    const auto data = read_text_file(*path);
    if (!data.has_value()) {
        show_file_error(hwnd, L"Could not read the selected DXF.");
        return;
    }

    auto result = acp::dxf::import_ascii(*data);
    if (!result.has_value()) {
        show_file_error(hwnd, L"The DXF file is invalid or unsupported.");
        return;
    }

    g_app.document = std::move(result->document);
    g_app.blocks = acp::BlockLibrary{};
    g_app.history = acp::History{};
    g_app.selected.reset();
    g_app.has_first_point = false;
    fit_drawing(hwnd);
}

void export_dxf(HWND hwnd) {
    const auto path = choose_file(
        hwnd, true,
        L"DXF Drawing (*.dxf)\0*.dxf\0All Files (*.*)\0*.*\0\0",
        L"dxf");
    if (!path.has_value()) {
        return;
    }

    if (!write_text_file(*path, acp::dxf::export_ascii(g_app.document))) {
        show_file_error(hwnd, L"Could not export the DXF drawing.");
    }
}

void handle_left_click(HWND hwnd, POINT point) {
    if (point.y < kToolbarHeight) {
        if (point.x >= 190 && point.x <= 265) set_tool(hwnd, Tool::Select);
        else if (point.x >= 272 && point.x <= 337) set_tool(hwnd, Tool::Line);
        else if (point.x >= 344 && point.x <= 419) set_tool(hwnd, Tool::Circle);
        else if (point.x >= 426 && point.x <= 491) set_tool(hwnd, Tool::Move);
        else if (point.x >= 498 && point.x <= 563) set_tool(hwnd, Tool::Copy);
        else if (point.x >= 570 && point.x <= 645) set_tool(hwnd, Tool::Rotate);
        return;
    }

    const RECT canvas = canvas_rect(hwnd);
    if (!PtInRect(&canvas, point)) {
        return;
    }

    const Vec2 world = screen_to_world(hwnd, point);

    if (g_app.tool == Tool::Select) {
        const auto hit = acp::selection::hit_test(
            g_app.document, world, 8.0 / std::max(g_app.zoom, 0.02));
        g_app.selected = hit.has_value()
            ? std::optional<acp::EntityId>{hit->id}
            : std::nullopt;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if ((g_app.tool == Tool::Move ||
         g_app.tool == Tool::Copy ||
         g_app.tool == Tool::Rotate) &&
        !g_app.selected.has_value()) {
        const auto hit = acp::selection::hit_test(
            g_app.document, world, 8.0 / std::max(g_app.zoom, 0.02));
        if (hit.has_value()) {
            g_app.selected = hit->id;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (!g_app.has_first_point) {
        g_app.first_point = world;
        g_app.has_first_point = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Line) {
        if (acp::geo::distance(g_app.first_point, world) > acp::geo::kEpsilon) {
            (void)g_app.history.apply(
                g_app.document,
                std::make_unique<acp::AddEntityCommand>(
                    LineEntity{{g_app.first_point, world}}));
        }
    } else if (g_app.tool == Tool::Circle) {
        const double radius = acp::geo::distance(g_app.first_point, world);
        if (radius > acp::geo::kEpsilon) {
            (void)g_app.history.apply(
                g_app.document,
                std::make_unique<acp::AddEntityCommand>(
                    CircleEntity{{g_app.first_point, radius}}));
        }
    } else if (g_app.selected.has_value()) {
        const acp::Entity* source = g_app.document.find(*g_app.selected);
        if (source != nullptr) {
            if (g_app.tool == Tool::Move) {
                acp::Entity replacement = *source;
                acp::transform::translate(replacement, world - g_app.first_point);
                (void)g_app.history.apply(
                    g_app.document,
                    std::make_unique<acp::UpdateEntityCommand>(
                        *g_app.selected, replacement));
            } else if (g_app.tool == Tool::Copy) {
                const acp::Entity copy =
                    acp::transform::translated_copy(*source, world - g_app.first_point);
                auto command = std::make_unique<acp::AddEntityCommand>(copy);
                auto* command_ptr = command.get();
                if (g_app.history.apply(g_app.document, std::move(command))) {
                    g_app.selected = command_ptr->id();
                }
            } else if (g_app.tool == Tool::Rotate) {
                const Vec2 direction = world - g_app.first_point;
                if (acp::geo::length(direction) > acp::geo::kEpsilon) {
                    acp::Entity replacement = *source;
                    const double radians = std::atan2(direction.y, direction.x);
                    acp::transform::rotate(replacement, g_app.first_point, radians);
                    (void)g_app.history.apply(
                        g_app.document,
                        std::make_unique<acp::UpdateEntityCommand>(
                            *g_app.selected, replacement));
                }
            }
        }
    }

    g_app.has_first_point = false;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void zoom_at(HWND hwnd, POINT cursor, int wheel_delta) {
    const Vec2 before = screen_to_world(hwnd, cursor);
    const double factor = wheel_delta > 0 ? 1.15 : (1.0 / 1.15);
    g_app.zoom = std::clamp(g_app.zoom * factor, 0.02, 200.0);
    const Vec2 after = screen_to_world(hwnd, cursor);
    g_app.view_center = g_app.view_center + (before - after);
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_COMMAND:
            switch (LOWORD(w_param)) {
                case kMenuNew:
                    g_app.document = Document{};
                    g_app.history = acp::History{};
                    g_app.blocks = acp::BlockLibrary{};
                    g_app.selected.reset();
                    g_app.has_first_point = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case kMenuOpen:
                    open_project(hwnd);
                    return 0;
                case kMenuSave:
                    save_project(hwnd);
                    return 0;
                case kMenuImportDxf:
                    import_dxf(hwnd);
                    return 0;
                case kMenuExportDxf:
                    export_dxf(hwnd);
                    return 0;
                case kMenuZoomExtents:
                    fit_drawing(hwnd);
                    return 0;
                case kMenuExit:
                    DestroyWindow(hwnd);
                    return 0;
                case kToolSelect: set_tool(hwnd, Tool::Select); return 0;
                case kToolLine: set_tool(hwnd, Tool::Line); return 0;
                case kToolCircle: set_tool(hwnd, Tool::Circle); return 0;
                case kToolMove: set_tool(hwnd, Tool::Move); return 0;
                case kToolCopy: set_tool(hwnd, Tool::Copy); return 0;
                case kToolRotate: set_tool(hwnd, Tool::Rotate); return 0;
                default: break;
            }
            break;

        case WM_KEYDOWN:
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && w_param == 'Z') {
                if (g_app.history.undo(g_app.document)) {
                    if (g_app.selected.has_value() &&
                        g_app.document.find(*g_app.selected) == nullptr) {
                        g_app.selected.reset();
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && w_param == 'Y') {
                if (g_app.history.redo(g_app.document)) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (w_param == VK_DELETE && g_app.selected.has_value()) {
                const acp::EntityId id = *g_app.selected;
                if (g_app.history.apply(
                        g_app.document,
                        std::make_unique<acp::RemoveEntityCommand>(id))) {
                    g_app.selected.reset();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (w_param == VK_ESCAPE) {
                g_app.has_first_point = false;
                set_tool(hwnd, Tool::Select);
                return 0;
            }
            if (w_param == 'L') {
                set_tool(hwnd, Tool::Line);
                return 0;
            }
            if (w_param == 'C') {
                set_tool(hwnd, Tool::Circle);
                return 0;
            }
            if (w_param == 'M') {
                set_tool(hwnd, Tool::Move);
                return 0;
            }
            if (w_param == 'P') {
                set_tool(hwnd, Tool::Copy);
                return 0;
            }
            if (w_param == 'R') {
                set_tool(hwnd, Tool::Rotate);
                return 0;
            }
            break;

        case WM_LBUTTONDOWN: {
            const POINT p{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            handle_left_click(hwnd, p);
            return 0;
        }

        case WM_MBUTTONDOWN:
            g_app.panning = true;
            g_app.last_mouse = POINT{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            SetCapture(hwnd);
            return 0;

        case WM_MBUTTONUP:
            g_app.panning = false;
            ReleaseCapture();
            return 0;

        case WM_MOUSEMOVE: {
            const POINT p{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            g_app.cursor = p;
            if (g_app.panning) {
                const LONG dx = p.x - g_app.last_mouse.x;
                const LONG dy = p.y - g_app.last_mouse.y;
                g_app.view_center.x -= static_cast<double>(dx) / g_app.zoom;
                g_app.view_center.y += static_cast<double>(dy) / g_app.zoom;
                g_app.last_mouse = p;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT p{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            ScreenToClient(hwnd, &p);
            zoom_at(hwnd, p, GET_WHEEL_DELTA_WPARAM(w_param));
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT client{};
            GetClientRect(hwnd, &client);

            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
            HGDIOBJ old_bitmap = SelectObject(memory, bitmap);

            HBRUSH background = CreateSolidBrush(RGB(27, 30, 36));
            FillRect(memory, &client, background);
            DeleteObject(background);

            const RECT canvas = canvas_rect(hwnd);
            draw_grid(hwnd, memory, canvas);
            draw_document(hwnd, memory);
            draw_selection_overlay(hwnd, memory);
            draw_preview(hwnd, memory);
            draw_toolbar(memory, client);
            draw_status(hwnd, memory, client);

            BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, old_bitmap);
            DeleteObject(bitmap);
            DeleteDC(memory);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

HMENU create_app_menu() {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU draw = CreatePopupMenu();
    HMENU view = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, kMenuNew, L"&New");
    AppendMenuW(file, MF_STRING, kMenuOpen, L"&Open Project...");
    AppendMenuW(file, MF_STRING, kMenuSave, L"&Save Project...");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kMenuImportDxf, L"&Import DXF...");
    AppendMenuW(file, MF_STRING, kMenuExportDxf, L"&Export DXF...");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kMenuExit, L"E&xit");

    AppendMenuW(draw, MF_STRING, kToolSelect, L"&Select\tEsc");
    AppendMenuW(draw, MF_STRING, kToolLine, L"&Line\tL");
    AppendMenuW(draw, MF_STRING, kToolCircle, L"&Circle\tC");
    AppendMenuW(draw, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(draw, MF_STRING, kToolMove, L"&Move\tM");
    AppendMenuW(draw, MF_STRING, kToolCopy, L"Co&py\tP");
    AppendMenuW(draw, MF_STRING, kToolRotate, L"&Rotate\tR");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(view, MF_STRING, kMenuZoomExtents, L"Zoom &Extents");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(draw), L"&Draw");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"&View");
    return menu;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    constexpr wchar_t kClassName[] = L"AutoCADProMainWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;

    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"Auto CAD Pro - 2D Workspace",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 800,
        nullptr,
        create_app_menu(),
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
#endif
