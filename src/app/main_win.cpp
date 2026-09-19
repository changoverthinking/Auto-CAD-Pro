#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>

#include "acp/document.hpp"
#include "acp/bounds.hpp"
#include "acp/history.hpp"
#include "acp/dxf.hpp"
#include "acp/edit2d.hpp"
#include "acp/persistence.hpp"
#include "acp/selection.hpp"
#include "acp/snap.hpp"
#include "acp/transform.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
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
    Polyline,
    Arc,
    Move,
    Copy,
    Rotate,
    Trim,
    Extend,
    Offset,
    Scale,
    Mirror,
    Dimension,
    Hatch,
    Text
};

struct AppState {
    Document document;
    acp::History history;
    acp::BlockLibrary blocks;
    std::optional<acp::EntityId> selected;
    std::optional<acp::EntityId> auxiliary_entity;
    acp::LayerId active_layer{acp::kDefaultLayerId};
    std::vector<Vec2> polyline_points;
    bool has_second_point{false};
    Vec2 second_point{};
    Tool tool{Tool::Select};
    double zoom{1.0};
    Vec2 view_center{0.0, 0.0};
    bool has_first_point{false};
    Vec2 first_point{};
    bool panning{false};
    POINT last_mouse{};
    POINT cursor{};
    bool snap_enabled{true};
    std::optional<acp::snap::Candidate> snap_candidate;
    std::wstring text_buffer;
};

AppState g_app;

constexpr int kToolbarHeight = 78;
constexpr int kStatusHeight = 24;
constexpr int kLayerPanelWidth = 230;
constexpr int kMenuNew = 1001;
constexpr int kMenuExit = 1002;
constexpr int kMenuOpen = 1003;
constexpr int kMenuSave = 1004;
constexpr int kMenuImportDxf = 1005;
constexpr int kMenuExportDxf = 1006;
constexpr int kMenuZoomExtents = 1007;
constexpr int kMenuNewLayer = 1008;
constexpr int kMenuAssignLayer = 1009;
constexpr int kMenuToggleLayerVisible = 1010;
constexpr int kMenuToggleLayerLock = 1011;
constexpr int kMenuToggleEntityVisible = 1012;
constexpr int kMenuCycleEntityWeight = 1013;
constexpr int kMenuToggleSnap = 1014;
constexpr int kToolSelect = 2001;
constexpr int kToolLine = 2002;
constexpr int kToolCircle = 2003;
constexpr int kToolMove = 2004;
constexpr int kToolCopy = 2005;
constexpr int kToolRotate = 2006;
constexpr int kToolPolyline = 2007;
constexpr int kToolArc = 2008;
constexpr int kToolTrim = 2009;
constexpr int kToolExtend = 2010;
constexpr int kToolOffset = 2011;
constexpr int kToolScale = 2012;
constexpr int kToolMirror = 2013;
constexpr int kToolDimension = 2014;
constexpr int kToolHatch = 2015;
constexpr int kToolText = 2016;

const wchar_t* tool_name(Tool tool) {
    switch (tool) {
        case Tool::Select: return L"Select";
        case Tool::Line: return L"Line";
        case Tool::Circle: return L"Circle";
        case Tool::Polyline: return L"Polyline";
        case Tool::Arc: return L"Arc";
        case Tool::Move: return L"Move";
        case Tool::Copy: return L"Copy";
        case Tool::Rotate: return L"Rotate";
        case Tool::Trim: return L"Trim";
        case Tool::Extend: return L"Extend";
        case Tool::Offset: return L"Offset";
        case Tool::Scale: return L"Scale";
        case Tool::Mirror: return L"Mirror";
        case Tool::Dimension: return L"Dimension";
        case Tool::Hatch: return L"Hatch";
        case Tool::Text: return L"Text";
    }
    return L"Select";
}

RECT canvas_rect(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    rc.top += kToolbarHeight;
    rc.right = std::max(rc.left, rc.right - kLayerPanelWidth);
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
    g_app.has_second_point = false;
    g_app.auxiliary_entity.reset();
    g_app.text_buffer.clear();
    if (tool != Tool::Polyline) {
        g_app.polyline_points.clear();
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

bool active_layer_writable() {
    const acp::Layer* layer = g_app.document.layer(g_app.active_layer);
    return layer != nullptr && layer->visible && !layer->locked;
}

bool selected_editable() {
    return g_app.selected.has_value() &&
           g_app.document.find(*g_app.selected) != nullptr &&
           !g_app.document.entity_locked(*g_app.selected);
}

void reset_interaction_state() {
    g_app.tool = Tool::Select;
    g_app.selected.reset();
    g_app.auxiliary_entity.reset();
    g_app.polyline_points.clear();
    g_app.has_first_point = false;
    g_app.has_second_point = false;
    g_app.snap_candidate.reset();
    g_app.text_buffer.clear();
}



std::string utf8_from_wide(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()),
        result.data(), length, nullptr, nullptr);
    return written == length ? result : std::string{};
}

void erase_last_utf16_codepoint(std::wstring& text) {
    if (text.empty()) {
        return;
    }
    const wchar_t last = text.back();
    text.pop_back();
    if (last >= 0xDC00 && last <= 0xDFFF && !text.empty()) {
        const wchar_t lead = text.back();
        if (lead >= 0xD800 && lead <= 0xDBFF) {
            text.pop_back();
        }
    }
}

int snap_priority(acp::snap::Kind kind) {
    switch (kind) {
        case acp::snap::Kind::Endpoint:
        case acp::snap::Kind::Intersection: return 0;
        case acp::snap::Kind::Midpoint:
        case acp::snap::Kind::Center: return 1;
        case acp::snap::Kind::Nearest: return 10;
    }
    return 100;
}

void consider_snap(
    std::optional<acp::snap::Candidate>& best,
    const std::optional<acp::snap::Candidate>& candidate) {

    if (!candidate.has_value()) {
        return;
    }
    if (!best.has_value() ||
        snap_priority(candidate->kind) < snap_priority(best->kind) ||
        (snap_priority(candidate->kind) == snap_priority(best->kind) &&
         candidate->distance_to_cursor < best->distance_to_cursor)) {
        best = candidate;
    }
}

std::vector<acp::geo::Segment> snap_segments_for_entity(const acp::Entity& entity) {
    std::vector<acp::geo::Segment> segments;
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            segments.push_back(value.segment);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            if (value.points.size() < 2) return;
            for (std::size_t i = 1; i < value.points.size(); ++i) {
                segments.push_back({value.points[i - 1], value.points[i]});
            }
            if (value.closed && value.points.size() > 2) {
                segments.push_back({value.points.back(), value.points.front()});
            }
        }
    }, entity);
    return segments;
}

std::optional<acp::snap::Candidate> best_document_snap(
    Vec2 cursor,
    double aperture) {

    if (!g_app.snap_enabled) {
        return std::nullopt;
    }

    std::optional<acp::snap::Candidate> best;
    std::vector<acp::geo::Segment> all_segments;

    for (const acp::EntityId id : g_app.document.ids()) {
        if (!g_app.document.entity_visible(id)) {
            continue;
        }
        const acp::Entity* entity = g_app.document.find(id);
        if (entity == nullptr) {
            continue;
        }

        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, LineEntity>) {
                consider_snap(best, acp::snap::best_for_segment(
                    value.segment, cursor, aperture, true));
            } else if constexpr (std::is_same_v<T, CircleEntity>) {
                consider_snap(best, acp::snap::best_for_circle(
                    value.circle, cursor, aperture));
            } else if constexpr (std::is_same_v<T, ArcEntity>) {
                consider_snap(best, acp::snap::best_for_arc(
                    value.arc, cursor, aperture, true));
            } else if constexpr (std::is_same_v<T, PolylineEntity>) {
                const auto segments = snap_segments_for_entity(*entity);
                for (const auto& segment : segments) {
                    consider_snap(best, acp::snap::best_for_segment(
                        segment, cursor, aperture, true));
                }
            }
        }, *entity);

        const auto segments = snap_segments_for_entity(*entity);
        all_segments.insert(all_segments.end(), segments.begin(), segments.end());
    }

    for (std::size_t i = 0; i < all_segments.size(); ++i) {
        for (std::size_t j = i + 1; j < all_segments.size(); ++j) {
            consider_snap(best, acp::snap::intersection_for_segments(
                all_segments[i], all_segments[j], cursor, aperture));
        }
    }

    return best;
}

Vec2 resolved_input_point(HWND hwnd, POINT point) {
    const Vec2 raw = screen_to_world(hwnd, point);
    g_app.snap_candidate = best_document_snap(
        raw, 10.0 / std::max(g_app.zoom, 0.02));
    return g_app.snap_candidate.has_value()
        ? g_app.snap_candidate->point
        : raw;
}

const wchar_t* snap_kind_name(acp::snap::Kind kind) {
    switch (kind) {
        case acp::snap::Kind::Endpoint: return L"END";
        case acp::snap::Kind::Intersection: return L"INT";
        case acp::snap::Kind::Midpoint: return L"MID";
        case acp::snap::Kind::Center: return L"CEN";
        case acp::snap::Kind::Nearest: return L"NEA";
    }
    return L"";
}

void draw_snap_marker(HWND hwnd, HDC dc) {
    if (!g_app.snap_candidate.has_value()) {
        return;
    }

    const POINT p = world_to_screen(hwnd, g_app.snap_candidate->point);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 214, 70));
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    Rectangle(dc, p.x - 5, p.y - 5, p.x + 6, p.y + 6);

    const wchar_t* label = snap_kind_name(g_app.snap_candidate->kind);
    const int old_mode = SetBkMode(dc, TRANSPARENT);
    const COLORREF old_color = SetTextColor(dc, RGB(255, 214, 70));
    TextOutW(dc, p.x + 8, p.y - 16, label, static_cast<int>(wcslen(label)));
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_mode);

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
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

void draw_segment(HWND hwnd, HDC dc, const acp::geo::Segment& segment) {
    const POINT a = world_to_screen(hwnd, segment.a);
    const POINT b = world_to_screen(hwnd, segment.b);
    MoveToEx(dc, a.x, a.y, nullptr);
    LineTo(dc, b.x, b.y);
}

void draw_circle(HWND hwnd, HDC dc, const CircleEntity& entity) {
    const POINT center = world_to_screen(hwnd, entity.circle.center);
    const LONG r = std::max<LONG>(
        1, static_cast<LONG>(std::lround(entity.circle.radius * g_app.zoom)));
    Ellipse(dc, center.x - r, center.y - r, center.x + r, center.y + r);
}

void draw_polyline(HWND hwnd, HDC dc, const PolylineEntity& entity) {
    if (entity.points.size() < 2) {
        return;
    }
    const POINT first = world_to_screen(hwnd, entity.points.front());
    MoveToEx(dc, first.x, first.y, nullptr);
    for (std::size_t i = 1; i < entity.points.size(); ++i) {
        const POINT p = world_to_screen(hwnd, entity.points[i]);
        LineTo(dc, p.x, p.y);
    }
    if (entity.closed) {
        LineTo(dc, first.x, first.y);
    }
}

void draw_text(HWND hwnd, HDC dc, const acp::TextEntity& entity) {
    if (entity.text.empty()) {
        return;
    }

    const int wide_length = MultiByteToWideChar(
        CP_UTF8, 0, entity.text.data(), static_cast<int>(entity.text.size()),
        nullptr, 0);
    if (wide_length <= 0) {
        return;
    }

    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    (void)MultiByteToWideChar(
        CP_UTF8, 0, entity.text.data(), static_cast<int>(entity.text.size()),
        wide.data(), wide_length);

    const POINT position = world_to_screen(hwnd, entity.position);
    const int pixel_height = std::max(
        9, static_cast<int>(std::lround(entity.height * g_app.zoom)));
    const int escapement = static_cast<int>(std::lround(
        -entity.rotation * 1800.0 / std::numbers::pi));

    HFONT font = CreateFontW(
        -pixel_height, 0, escapement, escapement, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (font == nullptr) {
        return;
    }

    HGDIOBJ old_font = SelectObject(dc, font);
    const int old_mode = SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, position.x, position.y - pixel_height,
             wide.c_str(), static_cast<int>(wide.size()));
    SetBkMode(dc, old_mode);
    SelectObject(dc, old_font);
    DeleteObject(font);
}

void draw_dimension(HWND hwnd, HDC dc, const acp::LinearDimensionEntity& entity) {
    if (!acp::annotation::valid_linear_dimension(entity)) {
        return;
    }

    draw_segment(hwnd, dc, acp::annotation::first_extension_line(entity));
    draw_segment(hwnd, dc, acp::annotation::second_extension_line(entity));
    const acp::geo::Segment line = acp::annotation::dimension_line(entity);
    draw_segment(hwnd, dc, line);

    wchar_t label[128]{};
    if (entity.text_override.has_value()) {
        const std::string& text = *entity.text_override;
        const int wide_length = MultiByteToWideChar(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (wide_length > 0) {
            std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
            (void)MultiByteToWideChar(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                wide.data(), wide_length);
            wcsncpy_s(label, wide.c_str(), _TRUNCATE);
        }
    } else {
        swprintf_s(label, L"%.2f", acp::annotation::measurement(entity));
    }

    const POINT p = world_to_screen(hwnd, acp::geo::midpoint(line));
    const int old_mode = SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, p.x + 4, p.y - 16, label, static_cast<int>(wcslen(label)));
    SetBkMode(dc, old_mode);
}

void draw_hatch(HWND hwnd, HDC dc, const acp::HatchEntity& entity) {
    if (entity.boundary.size() < 3) {
        return;
    }

    std::vector<POINT> points;
    points.reserve(entity.boundary.size());
    for (const Vec2 point : entity.boundary) {
        points.push_back(world_to_screen(hwnd, point));
    }

    HBRUSH brush = entity.solid
        ? CreateSolidBrush(RGB(72, 78, 88))
        : CreateHatchBrush(HS_BDIAGONAL, RGB(120, 126, 138));
    if (brush == nullptr) {
        return;
    }
    HGDIOBJ old_brush = SelectObject(dc, brush);
    Polygon(dc, points.data(), static_cast<int>(points.size()));
    SelectObject(dc, old_brush);
    DeleteObject(brush);
}

void draw_block_primitive(HWND hwnd, HDC dc, const acp::BlockPrimitive& primitive) {
    std::visit([&](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            draw_segment(hwnd, dc, item.segment);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            draw_circle(hwnd, dc, item);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            draw_arc(hwnd, dc, item);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            draw_polyline(hwnd, dc, item);
        }
    }, primitive);
}

void draw_document(HWND hwnd, HDC dc) {
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    const COLORREF old_text_color = SetTextColor(dc, RGB(229, 232, 239));

    for (const auto id : g_app.document.ids()) {
        if (!g_app.document.entity_visible(id)) {
            continue;
        }
        const auto* entity = g_app.document.find(id);
        if (entity == nullptr) {
            continue;
        }

        const double weight = std::clamp(
            g_app.document.effective_line_weight(id), 0.05, 2.0);
        const int pen_width = std::clamp(
            static_cast<int>(std::lround(weight * 4.0)), 1, 8);
        const COLORREF entity_color = g_app.document.entity_locked(id)
            ? RGB(145, 151, 162)
            : RGB(229, 232, 239);
        HPEN entity_pen = CreatePen(PS_SOLID, pen_width, entity_color);
        HGDIOBJ previous_pen = SelectObject(dc, entity_pen);
        SetTextColor(dc, entity_color);

        std::visit([&](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, LineEntity>) {
                draw_segment(hwnd, dc, item.segment);
            } else if constexpr (std::is_same_v<T, CircleEntity>) {
                draw_circle(hwnd, dc, item);
            } else if constexpr (std::is_same_v<T, ArcEntity>) {
                draw_arc(hwnd, dc, item);
            } else if constexpr (std::is_same_v<T, PolylineEntity>) {
                draw_polyline(hwnd, dc, item);
            } else if constexpr (std::is_same_v<T, acp::BlockReferenceEntity>) {
                for (const auto& primitive : g_app.blocks.instantiate(item)) {
                    draw_block_primitive(hwnd, dc, primitive);
                }
            } else if constexpr (std::is_same_v<T, acp::TextEntity>) {
                draw_text(hwnd, dc, item);
            } else if constexpr (std::is_same_v<T, acp::LinearDimensionEntity>) {
                draw_dimension(hwnd, dc, item);
            } else if constexpr (std::is_same_v<T, acp::HatchEntity>) {
                draw_hatch(hwnd, dc, item);
            }
        }, *entity);

        SelectObject(dc, previous_pen);
        DeleteObject(entity_pen);
    }

    SetTextColor(dc, old_text_color);
    SelectObject(dc, old_brush);
}

void draw_selection_overlay(HWND hwnd, HDC dc) {
    if (!g_app.selected.has_value()) {
        return;
    }
    const acp::Entity* entity = g_app.document.find(*g_app.selected);
    if (entity == nullptr || !g_app.document.entity_visible(*g_app.selected)) {
        return;
    }
    const auto box = acp::bounds::entity_bounds(*entity, &g_app.blocks);
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

    if (g_app.tool == Tool::Text) {
        const POINT p = world_to_screen(hwnd, g_app.first_point);
        std::wstring preview = g_app.text_buffer;
        preview.push_back(L'_');
        const int old_mode = SetBkMode(dc, TRANSPARENT);
        TextOutW(dc, p.x, p.y - 16, preview.c_str(),
                 static_cast<int>(preview.size()));
        SetBkMode(dc, old_mode);
    } else     if (g_app.tool == Tool::Line ||
        g_app.tool == Tool::Move ||
        g_app.tool == Tool::Copy ||
        g_app.tool == Tool::Rotate ||
        g_app.tool == Tool::Trim ||
        g_app.tool == Tool::Extend ||
        g_app.tool == Tool::Offset ||
        g_app.tool == Tool::Scale ||
        g_app.tool == Tool::Mirror ||
        g_app.tool == Tool::Dimension ||
        g_app.tool == Tool::Polyline ||
        g_app.tool == Tool::Arc) {
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
        {{426, 7, 501, 37}, L"Polyline", Tool::Polyline},
        {{508, 7, 563, 37}, L"Arc", Tool::Arc},
        {{570, 7, 625, 37}, L"Move", Tool::Move},
        {{632, 7, 687, 37}, L"Copy", Tool::Copy},
        {{694, 7, 759, 37}, L"Rotate", Tool::Rotate},
        {{766, 7, 821, 37}, L"Trim", Tool::Trim},
        {{828, 7, 893, 37}, L"Extend", Tool::Extend},
        {{900, 7, 965, 37}, L"Offset", Tool::Offset},
        {{190, 43, 255, 73}, L"Scale", Tool::Scale},
        {{262, 43, 337, 73}, L"Mirror", Tool::Mirror},
        {{344, 43, 439, 73}, L"Dimension", Tool::Dimension},
        {{446, 43, 511, 73}, L"Hatch", Tool::Hatch},
        {{518, 43, 583, 73}, L"Text", Tool::Text}
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


void draw_layer_panel(HWND hwnd, HDC dc, const RECT& client) {
    RECT panel{
        std::max(client.left, client.right - kLayerPanelWidth),
        kToolbarHeight,
        client.right,
        client.bottom - kStatusHeight
    };

    HBRUSH background = CreateSolidBrush(RGB(35, 39, 46));
    FillRect(dc, &panel, background);
    DeleteObject(background);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 237, 242));

    RECT heading{panel.left + 12, panel.top + 8, panel.right - 8, panel.top + 34};
    DrawTextW(dc, L"LAYERS / PROPERTIES", -1, &heading,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int y = panel.top + 40;
    for (const acp::LayerId id : g_app.document.layer_ids()) {
        const acp::Layer* layer = g_app.document.layer(id);
        if (layer == nullptr) {
            continue;
        }

        RECT row{panel.left + 8, y, panel.right - 8, y + 28};
        HBRUSH row_brush = CreateSolidBrush(
            id == g_app.active_layer ? RGB(58, 76, 98) : RGB(46, 50, 58));
        FillRect(dc, &row, row_brush);
        DeleteObject(row_brush);

        wchar_t name[160]{};
        MultiByteToWideChar(CP_UTF8, 0, layer->name.c_str(), -1,
                            name, static_cast<int>(std::size(name)));
        RECT name_rect{row.left + 8, row.top, row.right - 72, row.bottom};
        DrawTextW(dc, name, -1, &name_rect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT vis_rect{row.right - 68, row.top, row.right - 38, row.bottom};
        RECT lock_rect{row.right - 34, row.top, row.right - 4, row.bottom};
        DrawTextW(dc, layer->visible ? L"V" : L"-", -1, &vis_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(dc, layer->locked ? L"L" : L"-", -1, &lock_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        y += 32;
    }

    y += 8;
    SetTextColor(dc, RGB(180, 186, 198));
    RECT hint{panel.left + 10, y, panel.right - 10, panel.bottom};
    DrawTextW(dc,
              L"Click layer: activate\nV: visibility   L: lock\n"
              L"Layer menu: create/assign\n\n"
              L"Selected entity properties appear\nin the status bar.\n"
              L"Entity menu: visibility/weight.\n"
              L"F3 toggles Object Snap.",
              -1, &hint, DT_LEFT | DT_TOP | DT_WORDBREAK);

    (void)hwnd;
}

bool handle_layer_panel_click(HWND hwnd, POINT point) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int panel_left = client.right - kLayerPanelWidth;
    if (point.x < panel_left || point.y < kToolbarHeight ||
        point.y >= client.bottom - kStatusHeight) {
        return false;
    }

    int y = kToolbarHeight + 40;
    for (const acp::LayerId id : g_app.document.layer_ids()) {
        const acp::Layer* layer = g_app.document.layer(id);
        if (layer == nullptr) {
            continue;
        }
        RECT row{panel_left + 8, y, client.right - 8, y + 28};
        if (PtInRect(&row, point)) {
            if (point.x >= row.right - 68 && point.x < row.right - 38) {
                acp::Layer replacement = *layer;
                replacement.visible = !replacement.visible;
                (void)g_app.history.apply(
                    g_app.document,
                    std::make_unique<acp::UpdateLayerCommand>(
                        id, replacement));
            } else if (point.x >= row.right - 34) {
                acp::Layer replacement = *layer;
                replacement.locked = !replacement.locked;
                (void)g_app.history.apply(
                    g_app.document,
                    std::make_unique<acp::UpdateLayerCommand>(
                        id, replacement));
            } else {
                g_app.active_layer = id;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        }
        y += 32;
    }
    return true;
}

void create_layer(HWND hwnd) {
    int suffix = 1;
    while (true) {
        const std::string candidate = "Layer " + std::to_string(suffix);
        const acp::LayerId id = g_app.document.create_layer(candidate);
        if (id != 0) {
            g_app.active_layer = id;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        ++suffix;
        if (suffix > 9999) {
            return;
        }
    }
}

void assign_selected_to_active_layer(HWND hwnd) {
    if (!selected_editable() || !active_layer_writable()) {
        return;
    }
    const acp::EntityProperties* current =
        g_app.document.properties(*g_app.selected);
    if (current == nullptr) {
        return;
    }
    acp::EntityProperties replacement = *current;
    replacement.layer_id = g_app.active_layer;
    if (g_app.history.apply(
            g_app.document,
            std::make_unique<acp::UpdateEntityPropertiesCommand>(
                *g_app.selected, replacement))) {
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void draw_status(HWND hwnd, HDC dc, const RECT& client) {
    RECT bar{client.left, client.bottom - kStatusHeight, client.right, client.bottom};
    HBRUSH brush = CreateSolidBrush(RGB(32, 35, 41));
    FillRect(dc, &bar, brush);
    DeleteObject(brush);

    const Vec2 cursor_world = screen_to_world(hwnd, g_app.cursor);
    wchar_t buffer[256]{};
    const acp::Layer* active_layer = g_app.document.layer(g_app.active_layer);
    const wchar_t* snap_state = g_app.snap_enabled ? L"ON" : L"OFF";
    const wchar_t* layer_state =
        active_layer == nullptr ? L"MISSING" :
        active_layer->locked ? L"LOCKED" :
        !active_layer->visible ? L"HIDDEN" :
        L"ACTIVE";
    swprintf_s(buffer, L"Tool: %s    X: %.2f    Y: %.2f    Zoom: %.0f%%    Entities: %zu    Selected: %llu    Layer: %u (%s)    SNAP: %s    Undo: %zu",
               tool_name(g_app.tool), cursor_world.x, cursor_world.y,
               g_app.zoom * 100.0, g_app.document.size(),
               static_cast<unsigned long long>(g_app.selected.value_or(0)),
               static_cast<unsigned>(g_app.active_layer), layer_state,
               snap_state, g_app.history.undo_size());

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
    g_app.active_layer = acp::kDefaultLayerId;
    reset_interaction_state();
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
    g_app.active_layer = acp::kDefaultLayerId;
    reset_interaction_state();
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
    if (handle_layer_panel_click(hwnd, point)) {
        return;
    }

    if (point.y < kToolbarHeight) {
        if (point.y >= 43) {
            if (point.x >= 190 && point.x <= 255) set_tool(hwnd, Tool::Scale);
            else if (point.x >= 262 && point.x <= 337) set_tool(hwnd, Tool::Mirror);
            else if (point.x >= 344 && point.x <= 439) set_tool(hwnd, Tool::Dimension);
            else if (point.x >= 446 && point.x <= 511) set_tool(hwnd, Tool::Hatch);
            else if (point.x >= 518 && point.x <= 583) set_tool(hwnd, Tool::Text);
            return;
        }
        if (point.x >= 190 && point.x <= 265) set_tool(hwnd, Tool::Select);
        else if (point.x >= 272 && point.x <= 337) set_tool(hwnd, Tool::Line);
        else if (point.x >= 344 && point.x <= 419) set_tool(hwnd, Tool::Circle);
        else if (point.x >= 426 && point.x <= 501) set_tool(hwnd, Tool::Polyline);
        else if (point.x >= 508 && point.x <= 563) set_tool(hwnd, Tool::Arc);
        else if (point.x >= 570 && point.x <= 625) set_tool(hwnd, Tool::Move);
        else if (point.x >= 632 && point.x <= 687) set_tool(hwnd, Tool::Copy);
        else if (point.x >= 694 && point.x <= 759) set_tool(hwnd, Tool::Rotate);
        else if (point.x >= 766 && point.x <= 821) set_tool(hwnd, Tool::Trim);
        else if (point.x >= 828 && point.x <= 893) set_tool(hwnd, Tool::Extend);
        else if (point.x >= 900 && point.x <= 965) set_tool(hwnd, Tool::Offset);
        return;
    }

    const RECT canvas = canvas_rect(hwnd);
    if (!PtInRect(&canvas, point)) {
        return;
    }

    const Vec2 raw_world = screen_to_world(hwnd, point);
    const Vec2 world = resolved_input_point(hwnd, point);

    if ((g_app.tool == Tool::Line ||
         g_app.tool == Tool::Circle ||
         g_app.tool == Tool::Polyline ||
         g_app.tool == Tool::Arc ||
         g_app.tool == Tool::Dimension ||
         g_app.tool == Tool::Text) &&
        !active_layer_writable()) {
        return;
    }

    if (g_app.tool == Tool::Select) {
        const auto hit = acp::selection::hit_test(
            g_app.document, g_app.blocks, raw_world,
            8.0 / std::max(g_app.zoom, 0.02));
        g_app.selected = hit.has_value()
            ? std::optional<acp::EntityId>{hit->id}
            : std::nullopt;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }


    if (g_app.tool == Tool::Text) {
        if (!g_app.has_first_point) {
            g_app.first_point = world;
            g_app.has_first_point = true;
            g_app.text_buffer.clear();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Polyline) {
        g_app.polyline_points.push_back(world);
        g_app.has_first_point = true;
        g_app.first_point = world;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Arc) {
        if (!g_app.has_first_point) {
            g_app.first_point = world;
            g_app.has_first_point = true;
        } else if (!g_app.has_second_point) {
            g_app.second_point = world;
            g_app.has_second_point = true;
        } else {
            const double radius = acp::geo::distance(g_app.first_point, g_app.second_point);
            if (radius > acp::geo::kEpsilon) {
                const double start_angle = std::atan2(
                    g_app.second_point.y - g_app.first_point.y,
                    g_app.second_point.x - g_app.first_point.x);
                const double end_angle = std::atan2(
                    world.y - g_app.first_point.y,
                    world.x - g_app.first_point.x);
                auto command = std::make_unique<acp::AddEntityCommand>(
                    ArcEntity{{g_app.first_point, radius, start_angle, end_angle, true}});
                auto* command_ptr = command.get();
                if (g_app.history.apply(g_app.document, std::move(command))) {
                    g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                    g_app.selected = command_ptr->id();
                }
            }
            g_app.has_first_point = false;
            g_app.has_second_point = false;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if ((g_app.tool == Tool::Move ||
         g_app.tool == Tool::Copy ||
         g_app.tool == Tool::Rotate ||
         g_app.tool == Tool::Trim ||
         g_app.tool == Tool::Extend ||
         g_app.tool == Tool::Offset ||
         g_app.tool == Tool::Scale ||
         g_app.tool == Tool::Mirror ||
         g_app.tool == Tool::Hatch) &&
        !g_app.selected.has_value()) {
        const auto hit = acp::selection::hit_test(
            g_app.document, g_app.blocks, raw_world,
            8.0 / std::max(g_app.zoom, 0.02));
        if (hit.has_value()) {
            g_app.selected = hit->id;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Trim && g_app.selected.has_value()) {
        if (!selected_editable()) {
            return;
        }
        const acp::Entity* target = g_app.document.find(*g_app.selected);
        if (target == nullptr || !std::holds_alternative<LineEntity>(*target)) {
            return;
        }

        if (!g_app.auxiliary_entity.has_value()) {
            const auto hit = acp::selection::hit_test(
                g_app.document, g_app.blocks, raw_world,
                8.0 / std::max(g_app.zoom, 0.02));
            if (hit.has_value() && hit->id != *g_app.selected) {
                const acp::Entity* cutter = g_app.document.find(hit->id);
                if (cutter != nullptr && std::holds_alternative<LineEntity>(*cutter)) {
                    g_app.auxiliary_entity = hit->id;
                }
            }
        } else {
            const acp::Entity* cutter = g_app.document.find(*g_app.auxiliary_entity);
            if (cutter != nullptr && std::holds_alternative<LineEntity>(*cutter)) {
                acp::Entity replacement = *target;
                auto& segment = std::get<LineEntity>(replacement).segment;
                if (acp::edit2d::trim_segment(
                        segment,
                        std::get<LineEntity>(*cutter).segment,
                        world)) {
                    (void)g_app.history.apply(
                        g_app.document,
                        std::make_unique<acp::UpdateEntityCommand>(
                            *g_app.selected, replacement));
                }
            }
            g_app.auxiliary_entity.reset();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Extend && g_app.selected.has_value()) {
        if (!selected_editable()) {
            return;
        }
        const acp::Entity* target = g_app.document.find(*g_app.selected);
        const auto hit = acp::selection::hit_test(
            g_app.document, g_app.blocks, raw_world,
            8.0 / std::max(g_app.zoom, 0.02));
        if (target != nullptr && hit.has_value() &&
            hit->id != *g_app.selected &&
            std::holds_alternative<LineEntity>(*target)) {
            const acp::Entity* boundary = g_app.document.find(hit->id);
            if (boundary != nullptr && std::holds_alternative<LineEntity>(*boundary)) {
                acp::Entity replacement = *target;
                auto& segment = std::get<LineEntity>(replacement).segment;
                if (acp::edit2d::extend_segment(
                        segment,
                        std::get<LineEntity>(*boundary).segment)) {
                    (void)g_app.history.apply(
                        g_app.document,
                        std::make_unique<acp::UpdateEntityCommand>(
                            *g_app.selected, replacement));
                }
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Offset && g_app.selected.has_value()) {
        if (!selected_editable() || !active_layer_writable()) {
            return;
        }
        const acp::Entity* source = g_app.document.find(*g_app.selected);
        if (source != nullptr && std::holds_alternative<LineEntity>(*source)) {
            const auto& segment = std::get<LineEntity>(*source).segment;
            const Vec2 direction = segment.b - segment.a;
            const double length = acp::geo::length(direction);
            if (length > acp::geo::kEpsilon) {
                const double signed_distance =
                    acp::geo::cross(direction, world - segment.a) / length;
                const auto offset = acp::edit2d::offset_segment(
                    segment, signed_distance);
                if (offset.has_value()) {
                    auto command = std::make_unique<acp::AddEntityCommand>(
                        LineEntity{*offset});
                    auto* command_ptr = command.get();
                    if (g_app.history.apply(g_app.document, std::move(command))) {
                        g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                        g_app.selected = command_ptr->id();
                    }
                }
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Hatch && g_app.selected.has_value()) {
        if (!selected_editable() || !active_layer_writable()) {
            return;
        }
        const acp::Entity* source = g_app.document.find(*g_app.selected);
        if (source != nullptr && std::holds_alternative<PolylineEntity>(*source)) {
            const auto& polyline = std::get<PolylineEntity>(*source);
            if (polyline.closed && polyline.points.size() >= 3) {
                auto command = std::make_unique<acp::AddEntityCommand>(
                    acp::HatchEntity{polyline.points, "ANSI31", 0.0, 1.0, false});
                auto* command_ptr = command.get();
                if (g_app.history.apply(g_app.document, std::move(command))) {
                    g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                    g_app.selected = command_ptr->id();
                }
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Mirror && g_app.selected.has_value()) {
        if (!selected_editable()) {
            return;
        }
        if (!g_app.has_first_point) {
            g_app.first_point = world;
            g_app.has_first_point = true;
        } else {
            const acp::Entity* source = g_app.document.find(*g_app.selected);
            if (source != nullptr) {
                acp::Entity replacement = *source;
                if (acp::transform::mirror(
                        replacement, {g_app.first_point, world})) {
                    (void)g_app.history.apply(
                        g_app.document,
                        std::make_unique<acp::UpdateEntityCommand>(
                            *g_app.selected, replacement));
                }
            }
            g_app.has_first_point = false;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Scale && g_app.selected.has_value()) {
        if (!selected_editable()) {
            return;
        }
        if (!g_app.has_first_point) {
            g_app.first_point = world;
            g_app.has_first_point = true;
        } else if (!g_app.has_second_point) {
            g_app.second_point = world;
            g_app.has_second_point = true;
        } else {
            const double reference =
                acp::geo::distance(g_app.first_point, g_app.second_point);
            const double target =
                acp::geo::distance(g_app.first_point, world);
            const acp::Entity* source = g_app.document.find(*g_app.selected);
            if (source != nullptr &&
                reference > acp::geo::kEpsilon &&
                target > acp::geo::kEpsilon) {
                acp::Entity replacement = *source;
                if (acp::transform::scale_uniform(
                        replacement, g_app.first_point, target / reference)) {
                    (void)g_app.history.apply(
                        g_app.document,
                        std::make_unique<acp::UpdateEntityCommand>(
                            *g_app.selected, replacement));
                }
            }
            g_app.has_first_point = false;
            g_app.has_second_point = false;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Dimension) {
        if (!g_app.has_first_point) {
            g_app.first_point = world;
            g_app.has_first_point = true;
        } else if (!g_app.has_second_point) {
            g_app.second_point = world;
            g_app.has_second_point = true;
        } else {
            auto command = std::make_unique<acp::AddEntityCommand>(
                acp::LinearDimensionEntity{
                    g_app.first_point, g_app.second_point, world, std::nullopt});
            auto* command_ptr = command.get();
            if (g_app.history.apply(g_app.document, std::move(command))) {
                g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                g_app.selected = command_ptr->id();
            }
            g_app.has_first_point = false;
            g_app.has_second_point = false;
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
            auto command = std::make_unique<acp::AddEntityCommand>(
                LineEntity{{g_app.first_point, world}});
            auto* command_ptr = command.get();
            if (g_app.history.apply(g_app.document, std::move(command))) {
                g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                g_app.selected = command_ptr->id();
            }
        }
    } else if (g_app.tool == Tool::Circle) {
        const double radius = acp::geo::distance(g_app.first_point, world);
        if (radius > acp::geo::kEpsilon) {
            auto command = std::make_unique<acp::AddEntityCommand>(
                CircleEntity{{g_app.first_point, radius}});
            auto* command_ptr = command.get();
            if (g_app.history.apply(g_app.document, std::move(command))) {
                g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                g_app.selected = command_ptr->id();
            }
        }
    } else if (g_app.selected.has_value()) {
        if (!selected_editable()) {
            g_app.has_first_point = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
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
                const acp::EntityProperties* source_properties_ptr =
                    g_app.document.properties(*g_app.selected);
                const std::optional<acp::EntityProperties> source_properties =
                    source_properties_ptr != nullptr
                        ? std::optional<acp::EntityProperties>{*source_properties_ptr}
                        : std::nullopt;
                const acp::Entity copy =
                    acp::transform::translated_copy(*source, world - g_app.first_point);
                auto command = std::make_unique<acp::AddEntityCommand>(copy);
                auto* command_ptr = command.get();
                if (g_app.history.apply(g_app.document, std::move(command))) {
                    if (source_properties.has_value()) {
                        if (acp::EntityProperties* copied_properties =
                                g_app.document.properties(command_ptr->id())) {
                            *copied_properties = *source_properties;
                        }
                    }
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
                    g_app.active_layer = acp::kDefaultLayerId;
                    reset_interaction_state();
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
                case kMenuNewLayer:
                    create_layer(hwnd);
                    return 0;
                case kMenuAssignLayer:
                    assign_selected_to_active_layer(hwnd);
                    return 0;
                case kMenuToggleLayerVisible:
                    if (const acp::Layer* layer = g_app.document.layer(g_app.active_layer)) {
                        acp::Layer replacement = *layer;
                        replacement.visible = !replacement.visible;
                        (void)g_app.history.apply(
                            g_app.document,
                            std::make_unique<acp::UpdateLayerCommand>(
                                g_app.active_layer, replacement));
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                case kMenuToggleEntityVisible:
                    if (selected_editable()) {
                        if (const acp::EntityProperties* props =
                                g_app.document.properties(*g_app.selected)) {
                            acp::EntityProperties replacement = *props;
                            replacement.visible = !replacement.visible;
                            (void)g_app.history.apply(
                                g_app.document,
                                std::make_unique<acp::UpdateEntityPropertiesCommand>(
                                    *g_app.selected, replacement));
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    return 0;
                case kMenuToggleSnap:
                    g_app.snap_enabled = !g_app.snap_enabled;
                    g_app.snap_candidate.reset();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case kMenuCycleEntityWeight:
                    if (selected_editable()) {
                        if (const acp::EntityProperties* props =
                                g_app.document.properties(*g_app.selected)) {
                            const double current =
                                props->line_weight_override.value_or(
                                    g_app.document.effective_line_weight(*g_app.selected));
                            double next = 0.13;
                            if (current < 0.18) next = 0.25;
                            else if (current < 0.35) next = 0.50;
                            else if (current < 0.75) next = 1.00;
                            acp::EntityProperties replacement = *props;
                            replacement.line_weight_override = next;
                            (void)g_app.history.apply(
                                g_app.document,
                                std::make_unique<acp::UpdateEntityPropertiesCommand>(
                                    *g_app.selected, replacement));
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    return 0;
                case kMenuToggleLayerLock:
                    if (const acp::Layer* layer = g_app.document.layer(g_app.active_layer)) {
                        acp::Layer replacement = *layer;
                        replacement.locked = !replacement.locked;
                        (void)g_app.history.apply(
                            g_app.document,
                            std::make_unique<acp::UpdateLayerCommand>(
                                g_app.active_layer, replacement));
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                case kMenuExit:
                    DestroyWindow(hwnd);
                    return 0;
                case kToolSelect: set_tool(hwnd, Tool::Select); return 0;
                case kToolLine: set_tool(hwnd, Tool::Line); return 0;
                case kToolCircle: set_tool(hwnd, Tool::Circle); return 0;
                case kToolPolyline: set_tool(hwnd, Tool::Polyline); return 0;
                case kToolArc: set_tool(hwnd, Tool::Arc); return 0;
                case kToolMove: set_tool(hwnd, Tool::Move); return 0;
                case kToolCopy: set_tool(hwnd, Tool::Copy); return 0;
                case kToolRotate: set_tool(hwnd, Tool::Rotate); return 0;
                case kToolTrim: set_tool(hwnd, Tool::Trim); return 0;
                case kToolExtend: set_tool(hwnd, Tool::Extend); return 0;
                case kToolOffset: set_tool(hwnd, Tool::Offset); return 0;
                case kToolScale: set_tool(hwnd, Tool::Scale); return 0;
                case kToolMirror: set_tool(hwnd, Tool::Mirror); return 0;
                case kToolDimension: set_tool(hwnd, Tool::Dimension); return 0;
                case kToolHatch: set_tool(hwnd, Tool::Hatch); return 0;
                case kToolText: set_tool(hwnd, Tool::Text); return 0;
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
                if (!selected_editable()) {
                    return 0;
                }
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
                g_app.has_second_point = false;
                g_app.polyline_points.clear();
                g_app.auxiliary_entity.reset();
                set_tool(hwnd, Tool::Select);
                return 0;
            }
            if (w_param == VK_RETURN && g_app.tool == Tool::Text) {
                if (g_app.has_first_point &&
                    !g_app.text_buffer.empty() &&
                    active_layer_writable()) {
                    const std::string utf8 = utf8_from_wide(g_app.text_buffer);
                    if (utf8.empty()) {
                        MessageBeep(MB_ICONWARNING);
                        return 0;
                    }
                    auto command = std::make_unique<acp::AddEntityCommand>(
                        acp::TextEntity{
                            g_app.first_point, utf8, 2.5, 0.0});
                    auto* command_ptr = command.get();
                    if (g_app.history.apply(
                            g_app.document, std::move(command))) {
                        g_app.document.set_entity_layer(
                            command_ptr->id(), g_app.active_layer);
                        g_app.selected = command_ptr->id();
                    }
                }
                g_app.text_buffer.clear();
                g_app.has_first_point = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (w_param == VK_RETURN && g_app.tool == Tool::Polyline) {
                const bool close_polyline =
                    (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const std::size_t minimum_points = close_polyline ? 3u : 2u;
                if (g_app.polyline_points.size() >= minimum_points &&
                    active_layer_writable()) {
                    auto command = std::make_unique<acp::AddEntityCommand>(
                        PolylineEntity{g_app.polyline_points, close_polyline});
                    auto* command_ptr = command.get();
                    if (g_app.history.apply(g_app.document, std::move(command))) {
                        g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                        g_app.selected = command_ptr->id();
                    }
                }
                g_app.polyline_points.clear();
                g_app.has_first_point = false;
                InvalidateRect(hwnd, nullptr, FALSE);
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
            if (w_param == 'P' && (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
                set_tool(hwnd, Tool::Polyline);
                return 0;
            }
            if (w_param == 'A') {
                set_tool(hwnd, Tool::Arc);
                return 0;
            }
            if (w_param == 'M') {
                set_tool(hwnd, Tool::Move);
                return 0;
            }
            if (w_param == 'O') {
                set_tool(hwnd, Tool::Offset);
                return 0;
            }
            if (w_param == 'T') {
                set_tool(hwnd, Tool::Trim);
                return 0;
            }
            if (w_param == 'E') {
                set_tool(hwnd, Tool::Extend);
                return 0;
            }
            if (w_param == 'Y') {
                set_tool(hwnd, Tool::Copy);
                return 0;
            }
            if (w_param == 'R') {
                set_tool(hwnd, Tool::Rotate);
                return 0;
            }
            if (w_param == 'S') {
                set_tool(hwnd, Tool::Scale);
                return 0;
            }
            if (w_param == 'I') {
                set_tool(hwnd, Tool::Mirror);
                return 0;
            }
            if (w_param == 'D') {
                set_tool(hwnd, Tool::Dimension);
                return 0;
            }
            if (w_param == 'H') {
                set_tool(hwnd, Tool::Hatch);
                return 0;
            }
            if (w_param == 'X') {
                set_tool(hwnd, Tool::Text);
                return 0;
            }
            if (w_param == VK_F3) {
                g_app.snap_enabled = !g_app.snap_enabled;
                g_app.snap_candidate.reset();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_CHAR:
            if (g_app.tool == Tool::Text && g_app.has_first_point) {
                if (w_param == VK_BACK) {
                    erase_last_utf16_codepoint(g_app.text_buffer);
                } else if (w_param >= 32 && w_param <= 0xFFFF) {
                    g_app.text_buffer.push_back(
                        static_cast<wchar_t>(w_param));
                }
                InvalidateRect(hwnd, nullptr, FALSE);
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
            const RECT canvas = canvas_rect(hwnd);
            if (!g_app.panning && PtInRect(&canvas, p)) {
                const Vec2 raw = screen_to_world(hwnd, p);
                g_app.snap_candidate = best_document_snap(
                    raw, 10.0 / std::max(g_app.zoom, 0.02));
            } else if (!g_app.panning) {
                g_app.snap_candidate.reset();
            }
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
            draw_snap_marker(hwnd, memory);
            draw_toolbar(memory, client);
            draw_layer_panel(hwnd, memory, client);
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
    HMENU layer = CreatePopupMenu();
    HMENU entity = CreatePopupMenu();

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
    AppendMenuW(draw, MF_STRING, kToolPolyline, L"&Polyline\tP");
    AppendMenuW(draw, MF_STRING, kToolArc, L"&Arc\tA");
    AppendMenuW(draw, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(draw, MF_STRING, kToolMove, L"&Move\tM");
    AppendMenuW(draw, MF_STRING, kToolCopy, L"Cop&y\tY");
    AppendMenuW(draw, MF_STRING, kToolRotate, L"&Rotate\tR");
    AppendMenuW(draw, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(draw, MF_STRING, kToolTrim, L"&Trim\tT");
    AppendMenuW(draw, MF_STRING, kToolExtend, L"&Extend\tE");
    AppendMenuW(draw, MF_STRING, kToolOffset, L"&Offset\tO");
    AppendMenuW(draw, MF_STRING, kToolScale, L"&Scale\tS");
    AppendMenuW(draw, MF_STRING, kToolMirror, L"M&irror\tI");
    AppendMenuW(draw, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(draw, MF_STRING, kToolDimension, L"&Dimension\tD");
    AppendMenuW(draw, MF_STRING, kToolHatch, L"&Hatch\tH");
    AppendMenuW(draw, MF_STRING, kToolText, L"Te&xt\tX");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(view, MF_STRING, kMenuZoomExtents, L"Zoom &Extents");
    AppendMenuW(view, MF_STRING, kMenuToggleSnap, L"Toggle Object &Snap\tF3");
    AppendMenuW(layer, MF_STRING, kMenuNewLayer, L"&New Layer");
    AppendMenuW(layer, MF_STRING, kMenuAssignLayer, L"&Assign Selected to Active");
    AppendMenuW(layer, MF_STRING, kMenuToggleLayerVisible, L"Toggle &Visibility");
    AppendMenuW(layer, MF_STRING, kMenuToggleLayerLock, L"Toggle &Lock");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(draw), L"&Draw");
    AppendMenuW(entity, MF_STRING, kMenuToggleEntityVisible, L"Toggle &Visibility");
    AppendMenuW(entity, MF_STRING, kMenuCycleEntityWeight, L"Cycle Line &Weight");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(layer), L"&Layer");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(entity), L"&Entity");
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
