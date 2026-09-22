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
#include "acp/property_edit.hpp"
#include "acp/pdf.hpp"
#include "acp/recovery.hpp"
#include "acp/selection.hpp"
#include "acp/snap.hpp"
#include "acp/svg.hpp"
#include "acp/transform.hpp"
#include "acp/ui_layout.hpp"
#include "resource.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
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
    Rectangle,
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
    Text,
    BlockInsert
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
    bool dirty{false};
    std::optional<std::filesystem::path> project_path;
    acp::layout::PageSetup page_setup{};
    std::optional<double> print_scale_denominator;
    std::optional<acp::BlockId> active_block;
    std::size_t layer_scroll_index{0};
};

AppState g_app;

constexpr int kStatusHeight = 26;
constexpr UINT_PTR kAutosaveTimerId = 1;
constexpr UINT kAutosaveIntervalMs = 30000;

acp::ui_layout::Metrics client_layout_metrics(const RECT& client) {
    return acp::ui_layout::metrics_for_client(
        static_cast<int>(std::max<LONG>(1, client.right - client.left)),
        static_cast<int>(std::max<LONG>(1, client.bottom - client.top)));
}

int toolbar_height(const RECT& client) {
    return client_layout_metrics(client).toolbar_height;
}

int layer_panel_width(const RECT& client) {
    return client_layout_metrics(client).right_panel_width;
}

int left_tool_rail_width(const RECT& client) {
    return client_layout_metrics(client).left_rail_width;
}
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
constexpr int kMenuExportSvg = 1015;
constexpr int kMenuSaveAs = 1016;
constexpr int kMenuExportPdf = 1017;
constexpr int kMenuTextCycleHeight = 1018;
constexpr int kMenuTextRotate15 = 1019;
constexpr int kMenuDimensionShiftLine = 1020;
constexpr int kMenuHatchToggleSolid = 1021;
constexpr int kMenuHatchRotate45 = 1022;
constexpr int kMenuHatchCycleSpacing = 1023;
constexpr int kMenuCyclePaperSize = 1024;
constexpr int kMenuToggleOrientation = 1025;
constexpr int kMenuCyclePrintScale = 1026;
constexpr int kMenuCreateBlock = 1027;
constexpr int kMenuInsertBlock = 1028;
constexpr int kMenuEditLineWeight = 1029;
constexpr int kMenuEditTextContent = 1030;
constexpr int kMenuEditTextHeight = 1031;
constexpr int kMenuEditTextRotation = 1032;
constexpr int kMenuEditDimensionOverride = 1033;
constexpr int kMenuEditHatchAngle = 1034;
constexpr int kMenuEditHatchSpacing = 1035;
constexpr int kMenuEditBlockScale = 1036;
constexpr int kMenuEditBlockRotation = 1037;
constexpr int kMenuEditLayerName = 1038;
constexpr int kMenuEditLayerWeight = 1039;
constexpr int kMenuEditEntityColor = 1040;
constexpr int kMenuEditEntityLineType = 1041;
constexpr int kMenuEditLayerColor = 1042;
constexpr int kMenuEditLayerLineType = 1043;
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
constexpr int kToolRectangle = 2017;
constexpr int kToolBlockInsert = 2018;
#ifdef ACP_ENABLE_GUI_TEST_HOOKS
constexpr UINT kGuiTestSnapshotMessage = WM_APP + 42;
#endif

const wchar_t* paper_size_name(acp::layout::PaperSize paper) {
    switch (paper) {
        case acp::layout::PaperSize::A4: return L"A4";
        case acp::layout::PaperSize::A3: return L"A3";
        case acp::layout::PaperSize::A2: return L"A2";
        case acp::layout::PaperSize::A1: return L"A1";
        case acp::layout::PaperSize::A0: return L"A0";
    }
    return L"A4";
}

void cycle_paper_size() {
    using acp::layout::PaperSize;
    switch (g_app.page_setup.paper) {
        case PaperSize::A4: g_app.page_setup.paper = PaperSize::A3; break;
        case PaperSize::A3: g_app.page_setup.paper = PaperSize::A2; break;
        case PaperSize::A2: g_app.page_setup.paper = PaperSize::A1; break;
        case PaperSize::A1: g_app.page_setup.paper = PaperSize::A0; break;
        case PaperSize::A0: g_app.page_setup.paper = PaperSize::A4; break;
    }
}

void cycle_print_scale() {
    if (!g_app.print_scale_denominator.has_value()) {
        g_app.print_scale_denominator = 50.0;
    } else if (*g_app.print_scale_denominator < 75.0) {
        g_app.print_scale_denominator = 100.0;
    } else if (*g_app.print_scale_denominator < 150.0) {
        g_app.print_scale_denominator = 200.0;
    } else {
        g_app.print_scale_denominator.reset();
    }
}

const wchar_t* tool_name(Tool tool) {
    switch (tool) {
        case Tool::Select: return L"Select";
        case Tool::Line: return L"Line";
        case Tool::Circle: return L"Circle";
        case Tool::Polyline: return L"Polyline";
        case Tool::Arc: return L"Arc";
        case Tool::Rectangle: return L"Rectangle";
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
        case Tool::BlockInsert: return L"Insert Block";
    }
    return L"Select";
}
struct ToolbarButton {
    const wchar_t* text;
    Tool tool;
};

constexpr std::array<ToolbarButton, 18> kToolbarButtons{{
    {L"Select", Tool::Select},
    {L"Line", Tool::Line},
    {L"Polyline", Tool::Polyline},
    {L"Circle", Tool::Circle},
    {L"Arc", Tool::Arc},
    {L"Rectangle", Tool::Rectangle},
    {L"Move", Tool::Move},
    {L"Copy", Tool::Copy},
    {L"Rotate", Tool::Rotate},
    {L"Scale", Tool::Scale},
    {L"Mirror", Tool::Mirror},
    {L"Trim", Tool::Trim},
    {L"Extend", Tool::Extend},
    {L"Offset", Tool::Offset},
    {L"Dimension", Tool::Dimension},
    {L"Hatch", Tool::Hatch},
    {L"Text", Tool::Text},
    {L"Block", Tool::BlockInsert}
}};

RECT toolbar_button_rect(std::size_t index, const RECT& client) {
    const int height = toolbar_height(client);
    const int size = std::clamp(height - 4, 28, 34);
    const int gap = 2;
    const int left = 4 + static_cast<int>(index) * (size + gap);
    return RECT{left, 2, left + size, std::max(3, height - 2)};
}


RECT canvas_rect(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    rc.top += toolbar_height(rc);
    rc.left += left_tool_rail_width(rc);
    rc.right = std::max<LONG>(rc.left, rc.right - layer_panel_width(rc));
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


bool apply_history(std::unique_ptr<acp::Command> command) {
    if (!g_app.history.apply(
            g_app.document, g_app.blocks, std::move(command))) {
        return false;
    }
    g_app.dirty = true;
    return true;
}

bool confirm_discard_unsaved(HWND hwnd) {
    if (!g_app.dirty) {
        return true;
    }

    const int result = MessageBoxW(
        hwnd,
        L"The current drawing has unsaved changes.\n\nDiscard them?",
        L"Auto CAD Pro",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    return result == IDYES;
}

void ensure_active_layer_exists() {
    if (g_app.document.layer(g_app.active_layer) == nullptr) {
        g_app.active_layer = acp::kDefaultLayerId;
    }
}

void ensure_active_block_exists() {
    if (g_app.active_block.has_value() &&
        g_app.blocks.find(*g_app.active_block) != nullptr) {
        return;
    }

    const auto ids = g_app.blocks.ids();
    g_app.active_block =
        ids.empty()
            ? std::nullopt
            : std::optional<acp::BlockId>{ids.back()};
}

bool active_layer_writable() {
    const acp::Layer* layer = g_app.document.layer(g_app.active_layer);
    return layer != nullptr && layer->visible && !layer->locked;
}

bool selected_editable() {
    return g_app.selected.has_value() &&
           g_app.document.entity_editable(*g_app.selected);
}

int property_row_count() {
    int rows = 0;

    if (g_app.selected.has_value()) {
        ++rows; // Selected.
        const acp::EntityProperties* props =
            g_app.document.properties(*g_app.selected);
        if (props != nullptr) {
            rows += 4; // Layer, lineweight, visible, locked.
        }

        const acp::Entity* entity =
            g_app.document.find(*g_app.selected);
        if (entity != nullptr) {
            if (std::holds_alternative<acp::TextEntity>(*entity)) {
                rows += 4;
            } else if (std::holds_alternative<acp::LinearDimensionEntity>(*entity)) {
                rows += 3;
            } else if (std::holds_alternative<acp::HatchEntity>(*entity) ||
                       std::holds_alternative<acp::BlockReferenceEntity>(*entity)) {
                rows += 4;
            }
        }

        if (props != nullptr) {
            rows += 2; // Color and linetype.
        }
    } else {
        rows += 2; // Selected and active layer ID.
        if (g_app.document.layer(g_app.active_layer) != nullptr) {
            rows += 4; // Layer name, weight, color and type.
        }
    }

    rows += 5; // Zoom, snap, paper, orientation and print scale.
    return std::max(rows, 1);
}

acp::ui_layout::RightPanelMetrics right_panel_metrics_for_client(
    const RECT& client) {

    const int panel_height = std::max(
        1,
        static_cast<int>(
            client.bottom - kStatusHeight - toolbar_height(client)));
    return acp::ui_layout::right_panel_metrics(
        panel_height,
        property_row_count());
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
    g_app.layer_scroll_index = 0;
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

std::wstring wide_from_utf8(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()),
        nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()),
        result.data(), length);
    return written == length ? result : std::wstring{};
}

std::optional<double> parse_finite_double(const std::wstring& text) {
    const wchar_t* begin = text.c_str();
    wchar_t* end = nullptr;
    errno = 0;
    const double value = std::wcstod(begin, &end);
    if (begin == end || errno == ERANGE || !std::isfinite(value)) {
        return std::nullopt;
    }
    while (end != nullptr && *end != L'\0' &&
           std::iswspace(static_cast<wint_t>(*end))) {
        ++end;
    }
    if (end == nullptr || *end != L'\0') {
        return std::nullopt;
    }
    return value;
}

struct PropertyInputDialogData {
    std::wstring title;
    std::wstring label;
    std::wstring value;
    std::optional<std::wstring> result;
};

INT_PTR CALLBACK property_input_dialog_proc(
    HWND dialog,
    UINT message,
    WPARAM w_param,
    LPARAM l_param) {

    auto* data = reinterpret_cast<PropertyInputDialogData*>(
        GetWindowLongPtrW(dialog, GWLP_USERDATA));

    if (message == WM_INITDIALOG) {
        data = reinterpret_cast<PropertyInputDialogData*>(l_param);
        SetWindowLongPtrW(
            dialog, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(data));
        if (data == nullptr) {
            return FALSE;
        }
        SetWindowTextW(dialog, data->title.c_str());
        SetDlgItemTextW(
            dialog, IDC_PROPERTY_LABEL, data->label.c_str());
        SetDlgItemTextW(
            dialog, IDC_PROPERTY_EDIT, data->value.c_str());
        SendDlgItemMessageW(
            dialog, IDC_PROPERTY_EDIT, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(dialog, IDC_PROPERTY_EDIT));
        return FALSE;
    }

    if (message == WM_COMMAND && data != nullptr) {
        if (LOWORD(w_param) == IDOK) {
            const int length =
                GetWindowTextLengthW(
                    GetDlgItem(dialog, IDC_PROPERTY_EDIT));
            std::wstring value(
                static_cast<std::size_t>(std::max(0, length)) + 1u,
                L'\0');
            if (length > 0) {
                GetDlgItemTextW(
                    dialog,
                    IDC_PROPERTY_EDIT,
                    value.data(),
                    length + 1);
            }
            value.resize(static_cast<std::size_t>(std::max(0, length)));
            data->result = std::move(value);
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        if (LOWORD(w_param) == IDCANCEL) {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
    }

    return FALSE;
}

std::optional<std::wstring> prompt_property_value(
    HWND owner,
    std::wstring title,
    std::wstring label,
    std::wstring initial) {

    PropertyInputDialogData data{
        std::move(title),
        std::move(label),
        std::move(initial),
        std::nullopt
    };

    const INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_PROPERTY_INPUT),
        owner,
        property_input_dialog_proc,
        reinterpret_cast<LPARAM>(&data));
    if (result != IDOK) {
        return std::nullopt;
    }
    return data.result;
}

void show_invalid_property(HWND hwnd, const wchar_t* message) {
    MessageBoxW(
        hwnd,
        message,
        L"Auto CAD Pro Properties",
        MB_OK | MB_ICONWARNING);
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

std::optional<acp::snap::Candidate> best_document_snap(
    Vec2 cursor,
    double aperture) {

    if (!g_app.snap_enabled) {
        return std::nullopt;
    }
    return acp::snap::best_for_document(
        g_app.document,
        &g_app.blocks,
        cursor,
        aperture,
        true);
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

COLORREF display_color(acp::RgbColor color, bool locked) {
    if (locked) {
        const auto blend = [](std::uint8_t component) -> std::uint8_t {
            return static_cast<std::uint8_t>(
                (static_cast<unsigned>(component) + 145u * 2u) / 3u);
        };
        return RGB(blend(color.r), blend(color.g), blend(color.b));
    }
    return RGB(color.r, color.g, color.b);
}

HPEN create_entity_pen(
    int width,
    COLORREF color,
    acp::LineType line_type) {

    width = std::clamp(width, 1, 8);
    if (line_type == acp::LineType::Continuous) {
        return CreatePen(PS_SOLID, width, color);
    }

    LOGBRUSH brush{};
    brush.lbStyle = BS_SOLID;
    brush.lbColor = color;

    const DWORD dashed[] = {12, 8};
    const DWORD center[] = {18, 6, 3, 6};
    const DWORD* pattern =
        line_type == acp::LineType::Center ? center : dashed;
    const DWORD count =
        line_type == acp::LineType::Center
            ? static_cast<DWORD>(std::size(center))
            : static_cast<DWORD>(std::size(dashed));

    HPEN pen = ExtCreatePen(
        PS_GEOMETRIC | PS_USERSTYLE | PS_ENDCAP_FLAT | PS_JOIN_ROUND,
        static_cast<DWORD>(width),
        &brush,
        count,
        pattern);
    return pen != nullptr
        ? pen
        : CreatePen(PS_SOLID, width, color);
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

void draw_hatch(
    HWND hwnd,
    HDC dc,
    const acp::HatchEntity& entity,
    COLORREF fill_color) {
    if (!acp::hatch::valid(entity)) {
        return;
    }

    std::vector<POINT> points;
    points.reserve(entity.boundary.size());
    for (const Vec2 point : entity.boundary) {
        points.push_back(world_to_screen(hwnd, point));
    }

    if (entity.solid) {
        HBRUSH brush = CreateSolidBrush(fill_color);
        if (brush == nullptr) {
            return;
        }
        HGDIOBJ old_brush = SelectObject(dc, brush);
        Polygon(dc, points.data(), static_cast<int>(points.size()));
        SelectObject(dc, old_brush);
        DeleteObject(brush);
        return;
    }

    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Polygon(dc, points.data(), static_cast<int>(points.size()));
    for (const auto& segment : acp::hatch::pattern_segments(entity)) {
        draw_segment(hwnd, dc, segment);
    }
    SelectObject(dc, old_brush);
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
        const COLORREF entity_color = display_color(
            g_app.document.effective_color(id),
            g_app.document.entity_locked(id));
        const acp::LineType line_type =
            g_app.document.effective_line_type(id);
        HPEN entity_pen =
            create_entity_pen(pen_width, entity_color, line_type);
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
                draw_hatch(hwnd, dc, item, entity_color);
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
    } else if (g_app.tool == Tool::Rectangle) {
        const POINT a = first;
        const POINT b = second;
        Rectangle(dc, std::min(a.x, b.x), std::min(a.y, b.y),
                  std::max(a.x, b.x), std::max(a.y, b.y));
    } else if (g_app.tool == Tool::Line ||
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

void draw_tool_icon(HDC dc, Tool tool, RECT area, COLORREF color) {
    const int cx = (area.left + area.right) / 2;
    const int cy = (area.top + area.bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    auto line = [&](int x1, int y1, int x2, int y2) {
        MoveToEx(dc, x1, y1, nullptr);
        LineTo(dc, x2, y2);
    };

    switch (tool) {
        case Tool::Select:
            line(cx - 5, cy - 6, cx - 5, cy + 6);
            line(cx - 5, cy - 6, cx + 5, cy);
            line(cx + 5, cy, cx, cy + 1);
            line(cx, cy + 1, cx + 3, cy + 7);
            break;
        case Tool::Line:
            line(cx - 7, cy + 6, cx + 7, cy - 6);
            break;
        case Tool::Polyline:
            line(cx - 8, cy + 5, cx - 2, cy - 2);
            line(cx - 2, cy - 2, cx + 4, cy + 1);
            line(cx + 4, cy + 1, cx + 8, cy - 5);
            break;
        case Tool::Circle:
            Ellipse(dc, cx - 7, cy - 7, cx + 8, cy + 8);
            break;
        case Tool::Arc:
            Arc(dc, cx - 8, cy - 7, cx + 8, cy + 7,
                cx - 7, cy + 4, cx + 7, cy - 4);
            break;
        case Tool::Rectangle:
            Rectangle(dc, cx - 8, cy - 6, cx + 8, cy + 6);
            break;
        case Tool::Move:
            line(cx - 8, cy, cx + 8, cy);
            line(cx, cy - 8, cx, cy + 8);
            line(cx - 8, cy, cx - 4, cy - 3);
            line(cx - 8, cy, cx - 4, cy + 3);
            line(cx + 8, cy, cx + 4, cy - 3);
            line(cx + 8, cy, cx + 4, cy + 3);
            break;
        case Tool::Copy:
            Rectangle(dc, cx - 7, cy - 6, cx + 4, cy + 5);
            Rectangle(dc, cx - 3, cy - 2, cx + 8, cy + 9);
            break;
        case Tool::Rotate:
            Arc(dc, cx - 7, cy - 7, cx + 7, cy + 7,
                cx + 6, cy + 3, cx + 2, cy - 7);
            line(cx + 2, cy - 7, cx + 6, cy - 6);
            line(cx + 2, cy - 7, cx + 3, cy - 3);
            break;
        case Tool::Scale:
            Rectangle(dc, cx - 7, cy - 7, cx + 4, cy + 4);
            line(cx + 2, cy + 2, cx + 8, cy + 8);
            line(cx + 8, cy + 8, cx + 4, cy + 8);
            line(cx + 8, cy + 8, cx + 8, cy + 4);
            break;
        case Tool::Mirror:
            line(cx, cy - 8, cx, cy + 8);
            line(cx - 8, cy + 5, cx - 2, cy - 5);
            line(cx + 8, cy + 5, cx + 2, cy - 5);
            break;
        case Tool::Trim:
            line(cx - 8, cy + 5, cx + 8, cy - 5);
            line(cx - 8, cy - 5, cx - 1, cy);
            line(cx + 2, cy + 4, cx + 8, cy + 7);
            break;
        case Tool::Extend:
            line(cx - 8, cy + 5, cx + 3, cy - 2);
            line(cx + 5, cy - 8, cx + 5, cy + 8);
            line(cx + 3, cy - 2, cx + 5, cy - 3);
            break;
        case Tool::Offset:
            line(cx - 8, cy + 3, cx + 6, cy - 5);
            line(cx - 6, cy + 7, cx + 8, cy - 1);
            break;
        case Tool::Dimension:
            line(cx - 8, cy, cx + 8, cy);
            line(cx - 8, cy - 5, cx - 8, cy + 5);
            line(cx + 8, cy - 5, cx + 8, cy + 5);
            line(cx - 8, cy, cx - 4, cy - 3);
            line(cx - 8, cy, cx - 4, cy + 3);
            line(cx + 8, cy, cx + 4, cy - 3);
            line(cx + 8, cy, cx + 4, cy + 3);
            break;
        case Tool::Hatch:
            Rectangle(dc, cx - 7, cy - 7, cx + 7, cy + 7);
            for (int d = -8; d <= 6; d += 5) {
                line(cx - 7, cy + d + 5, cx + 2, cy + d - 4);
            }
            break;
        case Tool::Text:
            line(cx - 7, cy - 7, cx + 7, cy - 7);
            line(cx, cy - 7, cx, cy + 8);
            break;
        case Tool::BlockInsert:
            Rectangle(dc, cx - 8, cy - 8, cx + 4, cy + 4);
            Rectangle(dc, cx - 3, cy - 3, cx + 9, cy + 9);
            break;
    }

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void draw_toolbar(HDC dc, const RECT& client) {
    const int height = toolbar_height(client);
    RECT bar{client.left, client.top, client.right, client.top + height};
    HBRUSH background = CreateSolidBrush(RGB(27, 43, 57));
    FillRect(dc, &bar, background);
    DeleteObject(background);

    HPEN divider = CreatePen(PS_SOLID, 1, RGB(57, 76, 94));
    HGDIOBJ old_pen = SelectObject(dc, divider);
    MoveToEx(dc, 0, height - 1, nullptr);
    LineTo(dc, client.right, height - 1);
    SelectObject(dc, old_pen);
    DeleteObject(divider);

    SetBkMode(dc, TRANSPARENT);

    for (std::size_t i = 0; i < kToolbarButtons.size(); ++i) {
        const auto& button = kToolbarButtons[i];
        RECT rect = toolbar_button_rect(i, client);
        const bool active = g_app.tool == button.tool;

        HBRUSH button_brush = CreateSolidBrush(
            active ? RGB(28, 103, 163) : RGB(34, 52, 67));
        FillRect(dc, &rect, button_brush);
        DeleteObject(button_brush);

        if (active) {
            HPEN edge = CreatePen(PS_SOLID, 1, RGB(58, 169, 239));
            HGDIOBJ previous = SelectObject(dc, edge);
            HGDIOBJ previous_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(dc, previous_brush);
            SelectObject(dc, previous);
            DeleteObject(edge);
        }

        draw_tool_icon(dc, button.tool, rect,
                       active ? RGB(240, 250, 255) : RGB(111, 205, 255));
    }
}

bool handle_toolbar_click(HWND hwnd, POINT point) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int height = toolbar_height(client);
    if (point.y < 0 || point.y >= height) {
        return false;
    }
    for (std::size_t i = 0; i < kToolbarButtons.size(); ++i) {
        const RECT rect = toolbar_button_rect(i, client);
        if (PtInRect(&rect, point)) {
            set_tool(hwnd, kToolbarButtons[i].tool);
            return true;
        }
    }
    return true;
}


constexpr std::array<Tool, 12> kLeftRailTools{
    Tool::Select, Tool::Line, Tool::Polyline, Tool::Circle, Tool::Arc,
    Tool::Rectangle, Tool::Move, Tool::Trim, Tool::Dimension,
    Tool::Hatch, Tool::Text, Tool::BlockInsert
};

void draw_left_tool_rail(HDC dc, const RECT& client) {
    const int top = toolbar_height(client);
    const int width = left_tool_rail_width(client);
    RECT rail{
        client.left,
        top,
        client.left + width,
        client.bottom - kStatusHeight
    };
    HBRUSH background = CreateSolidBrush(RGB(25, 40, 53));
    FillRect(dc, &rail, background);
    DeleteObject(background);

    SetBkMode(dc, TRANSPARENT);
    const int cell_height = std::clamp(width - 8, 26, 34);
    const int gap = 3;
    int y = rail.top + 4;
    for (const Tool item : kLeftRailTools) {
        RECT cell{rail.left + 3, y, rail.right - 3, y + cell_height};
        HBRUSH brush = CreateSolidBrush(
            g_app.tool == item ? RGB(28, 103, 163) : RGB(33, 51, 66));
        FillRect(dc, &cell, brush);
        DeleteObject(brush);
        draw_tool_icon(dc, item, cell,
                       g_app.tool == item ? RGB(240, 250, 255) : RGB(111, 205, 255));
        y += cell_height + gap;
    }
}

bool handle_left_tool_rail_click(HWND hwnd, POINT point) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int top = toolbar_height(client);
    const int width = left_tool_rail_width(client);
    if (point.x < client.left || point.x >= client.left + width ||
        point.y < top || point.y >= client.bottom - kStatusHeight) {
        return false;
    }

    const int cell_height = std::clamp(width - 8, 26, 34);
    const int stride = cell_height + 3;
    const int relative = point.y - (top + 4);
    if (relative >= 0) {
        const int index = relative / stride;
        const int local_y = relative % stride;
        if (index >= 0 && index < static_cast<int>(kLeftRailTools.size()) &&
            local_y < cell_height) {
            set_tool(hwnd, kLeftRailTools[static_cast<std::size_t>(index)]);
            return true;
        }
    }
    return true;
}




enum class DirectProperty {
    LineWeight,
    TextContent,
    TextHeight,
    TextRotation,
    DimensionOverride,
    HatchAngle,
    HatchSpacing,
    BlockScale,
    BlockRotation,
    Color,
    LineType
};

std::wstring format_property_number(double value, int precision = 3) {
    wchar_t buffer[96]{};
    swprintf_s(buffer, L"%.*f", precision, value);
    return buffer;
}

std::wstring format_rgb(acp::RgbColor color) {
    wchar_t buffer[16]{};
    swprintf_s(
        buffer, L"#%02X%02X%02X",
        static_cast<unsigned>(color.r),
        static_cast<unsigned>(color.g),
        static_cast<unsigned>(color.b));
    return buffer;
}

std::optional<acp::RgbColor> parse_rgb(std::wstring_view text) {
    if (text.size() != 7 || text.front() != L'#') return std::nullopt;
    auto hex_value = [](wchar_t ch) -> int {
        if (ch >= L'0' && ch <= L'9') return ch - L'0';
        if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
        if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
        return -1;
    };
    std::array<int, 6> values{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = hex_value(text[i + 1]);
        if (values[i] < 0) return std::nullopt;
    }
    return acp::RgbColor{
        static_cast<std::uint8_t>(values[0] * 16 + values[1]),
        static_cast<std::uint8_t>(values[2] * 16 + values[3]),
        static_cast<std::uint8_t>(values[4] * 16 + values[5])};
}

const wchar_t* line_type_name(acp::LineType value) {
    switch (value) {
        case acp::LineType::Continuous: return L"Continuous";
        case acp::LineType::Dashed: return L"Dashed";
        case acp::LineType::Center: return L"Center";
    }
    return L"Continuous";
}

std::optional<acp::LineType> parse_line_type(std::wstring value) {
    value.erase(
        std::remove_if(
            value.begin(), value.end(),
            [](wchar_t ch) { return std::iswspace(ch) != 0; }),
        value.end());
    if (_wcsicmp(value.c_str(), L"Continuous") == 0)
        return acp::LineType::Continuous;
    if (_wcsicmp(value.c_str(), L"Dashed") == 0)
        return acp::LineType::Dashed;
    if (_wcsicmp(value.c_str(), L"Center") == 0)
        return acp::LineType::Center;
    return std::nullopt;
}

bool edit_selected_property(HWND hwnd, DirectProperty property) {
    if (!selected_editable() || !g_app.selected.has_value()) {
        MessageBeep(MB_ICONWARNING);
        return false;
    }

    const acp::EntityId id = *g_app.selected;
    const acp::Entity* entity = g_app.document.find(id);
    if (entity == nullptr) {
        return false;
    }

    if (property == DirectProperty::LineWeight) {
        const acp::EntityProperties* current =
            g_app.document.properties(id);
        if (current == nullptr) {
            return false;
        }

        const std::wstring initial =
            current->line_weight_override.has_value()
                ? format_property_number(*current->line_weight_override, 3)
                : L"";
        const auto entered = prompt_property_value(
            hwnd,
            L"Line Weight",
            L"Line weight in mm (blank = ByLayer):",
            initial);
        if (!entered.has_value()) {
            return false;
        }

        std::optional<double> value;
        if (!entered->empty()) {
            value = parse_finite_double(*entered);
            if (!value.has_value()) {
                show_invalid_property(
                    hwnd,
                    L"Line weight must be a finite number greater than or equal to 0.");
                return false;
            }
        }

        const auto replacement =
            acp::property_edit::line_weight_override(*current, value);
        if (!replacement.has_value()) {
            show_invalid_property(
                hwnd,
                L"Line weight must be a finite number greater than or equal to 0.");
            return false;
        }

        if (apply_history(
                std::make_unique<acp::UpdateEntityPropertiesCommand>(
                    id, *replacement))) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        }
        return false;
    }

    if (property == DirectProperty::Color ||
        property == DirectProperty::LineType) {
        const acp::EntityProperties* current =
            g_app.document.properties(id);
        if (current == nullptr) return false;

        std::optional<acp::EntityProperties> replacement;
        if (property == DirectProperty::Color) {
            const auto entered = prompt_property_value(
                hwnd, L"Entity Color",
                L"#RRGGBB (blank = ByLayer):",
                current->color_override.has_value()
                    ? format_rgb(*current->color_override) : L"");
            if (!entered.has_value()) return false;
            if (entered->empty()) {
                replacement =
                    acp::property_edit::color_override(*current, std::nullopt);
            } else {
                const auto parsed = parse_rgb(*entered);
                if (!parsed.has_value()) {
                    show_invalid_property(
                        hwnd,
                        L"Color must use #RRGGBB format, for example #FF0000.");
                    return false;
                }
                replacement =
                    acp::property_edit::color_override(*current, *parsed);
            }
        } else {
            const auto entered = prompt_property_value(
                hwnd, L"Entity Linetype",
                L"Continuous, Dashed or Center (blank = ByLayer):",
                current->line_type_override.has_value()
                    ? std::wstring{line_type_name(*current->line_type_override)}
                    : L"");
            if (!entered.has_value()) return false;
            if (entered->empty()) {
                replacement =
                    acp::property_edit::line_type_override(*current, std::nullopt);
            } else {
                const auto parsed = parse_line_type(*entered);
                if (!parsed.has_value()) {
                    show_invalid_property(
                        hwnd,
                        L"Linetype must be Continuous, Dashed or Center.");
                    return false;
                }
                replacement =
                    acp::property_edit::line_type_override(*current, *parsed);
            }
        }

        if (!replacement.has_value()) return false;
        if (apply_history(
                std::make_unique<acp::UpdateEntityPropertiesCommand>(
                    id, *replacement))) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        }
        return false;
    }

    std::optional<acp::Entity> replacement;

    if (property == DirectProperty::TextContent) {
        if (!std::holds_alternative<acp::TextEntity>(*entity)) {
            return false;
        }
        const auto& text = std::get<acp::TextEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd, L"Text Content", L"Text:", wide_from_utf8(text.text));
        if (!entered.has_value()) return false;

        replacement = acp::property_edit::text_content(
            *entity, utf8_from_wide(*entered));
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"Text content cannot be empty or invalid.");
            return false;
        }
    } else if (property == DirectProperty::TextHeight) {
        if (!std::holds_alternative<acp::TextEntity>(*entity)) {
            return false;
        }
        const auto& text = std::get<acp::TextEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd, L"Text Height", L"Height:",
            format_property_number(text.height));
        if (!entered.has_value()) return false;
        const auto parsed = parse_finite_double(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(hwnd, L"Text height must be greater than 0.");
            return false;
        }

        replacement = acp::property_edit::text_height(*entity, *parsed);
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"Text height must be greater than 0.");
            return false;
        }
    } else if (property == DirectProperty::TextRotation) {
        if (!std::holds_alternative<acp::TextEntity>(*entity)) {
            return false;
        }
        const auto& text = std::get<acp::TextEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd, L"Text Rotation", L"Rotation in degrees:",
            format_property_number(
                text.rotation * 180.0 / std::numbers::pi));
        if (!entered.has_value()) return false;
        const auto parsed = parse_finite_double(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(hwnd, L"Rotation must be a finite number.");
            return false;
        }

        replacement = acp::property_edit::text_rotation(
            *entity, *parsed * std::numbers::pi / 180.0);
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"Rotation must be a finite number.");
            return false;
        }
    } else if (property == DirectProperty::DimensionOverride) {
        if (!std::holds_alternative<acp::LinearDimensionEntity>(*entity)) {
            return false;
        }
        const auto& dimension =
            std::get<acp::LinearDimensionEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd,
            L"Dimension Text Override",
            L"Override text (blank = measured value):",
            dimension.text_override.has_value()
                ? wide_from_utf8(*dimension.text_override)
                : L"");
        if (!entered.has_value()) return false;

        std::optional<std::string> value;
        if (!entered->empty()) {
            value = utf8_from_wide(*entered);
            if (value->empty()) {
                show_invalid_property(
                    hwnd, L"Dimension override is not valid UTF-8 text.");
                return false;
            }
        }

        replacement =
            acp::property_edit::dimension_override(*entity, std::move(value));
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"The edited dimension would be invalid.");
            return false;
        }
    } else if (property == DirectProperty::HatchAngle) {
        if (!std::holds_alternative<acp::HatchEntity>(*entity)) {
            return false;
        }
        const auto& hatch = std::get<acp::HatchEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd, L"Hatch Angle", L"Angle in degrees:",
            format_property_number(
                hatch.angle * 180.0 / std::numbers::pi));
        if (!entered.has_value()) return false;
        const auto parsed = parse_finite_double(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(hwnd, L"Hatch angle must be a finite number.");
            return false;
        }

        replacement = acp::property_edit::hatch_angle(
            *entity, *parsed * std::numbers::pi / 180.0);
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"The edited hatch would be invalid.");
            return false;
        }
    } else if (property == DirectProperty::HatchSpacing) {
        if (!std::holds_alternative<acp::HatchEntity>(*entity)) {
            return false;
        }
        const auto& hatch = std::get<acp::HatchEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd, L"Hatch Spacing", L"Spacing:",
            format_property_number(hatch.spacing));
        if (!entered.has_value()) return false;
        const auto parsed = parse_finite_double(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(hwnd, L"Hatch spacing must be greater than 0.");
            return false;
        }

        replacement =
            acp::property_edit::hatch_spacing(*entity, *parsed);
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"Hatch spacing must be greater than 0.");
            return false;
        }
    } else if (property == DirectProperty::BlockScale) {
        if (!std::holds_alternative<acp::BlockReferenceEntity>(*entity)) {
            return false;
        }
        const auto& block =
            std::get<acp::BlockReferenceEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd, L"Block Scale", L"Uniform scale:",
            format_property_number(block.scale));
        if (!entered.has_value()) return false;
        const auto parsed = parse_finite_double(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(hwnd, L"Block scale must be greater than 0.");
            return false;
        }

        replacement =
            acp::property_edit::block_scale(*entity, *parsed);
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"Block scale must be greater than 0.");
            return false;
        }
    } else if (property == DirectProperty::BlockRotation) {
        if (!std::holds_alternative<acp::BlockReferenceEntity>(*entity)) {
            return false;
        }
        const auto& block =
            std::get<acp::BlockReferenceEntity>(*entity);
        const auto entered = prompt_property_value(
            hwnd, L"Block Rotation", L"Rotation in degrees:",
            format_property_number(
                block.rotation * 180.0 / std::numbers::pi));
        if (!entered.has_value()) return false;
        const auto parsed = parse_finite_double(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(hwnd, L"Block rotation must be a finite number.");
            return false;
        }

        replacement = acp::property_edit::block_rotation(
            *entity, *parsed * std::numbers::pi / 180.0);
        if (!replacement.has_value()) {
            show_invalid_property(hwnd, L"Block rotation must be a finite number.");
            return false;
        }
    } else {
        return false;
    }

    if (apply_history(
            std::make_unique<acp::UpdateEntityCommand>(
                id, *replacement))) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }
    return false;
}

enum class DirectLayerProperty {
    Name,
    LineWeight,
    Color,
    LineType
};

bool edit_active_layer_property(HWND hwnd, DirectLayerProperty property) {
    const acp::Layer* current =
        g_app.document.layer(g_app.active_layer);
    if (current == nullptr) {
        MessageBeep(MB_ICONWARNING);
        return false;
    }

    acp::Layer replacement = *current;

    if (property == DirectLayerProperty::Name) {
        const auto entered = prompt_property_value(
            hwnd,
            L"Layer Name",
            L"Layer name:",
            wide_from_utf8(current->name));
        if (!entered.has_value()) {
            return false;
        }

        const std::string utf8 = utf8_from_wide(*entered);
        if (utf8.empty()) {
            show_invalid_property(
                hwnd, L"Layer name cannot be empty.");
            return false;
        }

        for (const acp::LayerId id : g_app.document.layer_ids()) {
            if (id == g_app.active_layer) {
                continue;
            }
            const acp::Layer* other = g_app.document.layer(id);
            if (other != nullptr && other->name == utf8) {
                show_invalid_property(
                    hwnd, L"Another layer already uses that name.");
                return false;
            }
        }
        replacement.name = utf8;
    } else if (property == DirectLayerProperty::LineWeight) {
        const auto entered = prompt_property_value(
            hwnd,
            L"Layer Line Weight",
            L"Line weight in mm:",
            format_property_number(current->line_weight, 3));
        if (!entered.has_value()) {
            return false;
        }

        const auto parsed = parse_finite_double(*entered);
        if (!parsed.has_value() || *parsed < 0.0) {
            show_invalid_property(
                hwnd,
                L"Layer line weight must be a finite number greater than or equal to 0.");
            return false;
        }
        replacement.line_weight = *parsed;
    } else if (property == DirectLayerProperty::Color) {
        const auto entered = prompt_property_value(
            hwnd, L"Layer Color", L"#RRGGBB:",
            format_rgb(current->color));
        if (!entered.has_value()) return false;
        const auto parsed = parse_rgb(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(
                hwnd,
                L"Color must use #RRGGBB format, for example #00FFFF.");
            return false;
        }
        replacement.color = *parsed;
    } else {
        const auto entered = prompt_property_value(
            hwnd, L"Layer Linetype",
            L"Continuous, Dashed or Center:",
            line_type_name(current->line_type));
        if (!entered.has_value()) return false;
        const auto parsed = parse_line_type(*entered);
        if (!parsed.has_value()) {
            show_invalid_property(
                hwnd,
                L"Linetype must be Continuous, Dashed or Center.");
            return false;
        }
        replacement.line_type = *parsed;
    }

    if (apply_history(
            std::make_unique<acp::UpdateLayerCommand>(
                g_app.active_layer, replacement))) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return true;
    }
    return false;
}

void draw_layer_panel(HWND hwnd, HDC dc, const RECT& client) {
    const int top = toolbar_height(client);
    const int width = layer_panel_width(client);
    RECT panel{
        std::max<LONG>(client.left, client.right - width),
        top,
        client.right,
        client.bottom - kStatusHeight
    };

    HBRUSH background = CreateSolidBrush(RGB(28, 42, 54));
    FillRect(dc, &panel, background);
    DeleteObject(background);

    HPEN separator = CreatePen(PS_SOLID, 1, RGB(58, 76, 92));
    HGDIOBJ old_pen = SelectObject(dc, separator);
    MoveToEx(dc, panel.left, panel.top, nullptr);
    LineTo(dc, panel.left, panel.bottom);
    SelectObject(dc, old_pen);
    DeleteObject(separator);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(231, 237, 242));

    RECT heading{panel.left + 10, panel.top + 4, panel.right - 8, panel.top + 28};
    DrawTextW(dc, L"Layers", -1, &heading,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const bool compact_panel =
        acp::ui_layout::compact_right_panel(width);
    const auto panel_metrics =
        right_panel_metrics_for_client(client);
    const auto layer_ids = g_app.document.layer_ids();
    const std::size_t visible_layer_rows =
        static_cast<std::size_t>(
            std::max(panel_metrics.visible_layer_rows, 1));
    const std::size_t max_scroll =
        layer_ids.size() > visible_layer_rows
            ? layer_ids.size() - visible_layer_rows
            : 0;
    g_app.layer_scroll_index =
        std::min(g_app.layer_scroll_index, max_scroll);
    const std::size_t layer_end =
        std::min(
            layer_ids.size(),
            g_app.layer_scroll_index + visible_layer_rows);

    int y = panel.top + 32;
    for (std::size_t layer_index = g_app.layer_scroll_index;
         layer_index < layer_end;
         ++layer_index) {
        const acp::LayerId id = layer_ids[layer_index];
        const acp::Layer* layer = g_app.document.layer(id);
        if (layer == nullptr) continue;

        RECT row{panel.left + 6, y, panel.right - 6, y + 24};
        HBRUSH row_brush = CreateSolidBrush(
            id == g_app.active_layer ? RGB(34, 90, 137) : RGB(35, 51, 64));
        FillRect(dc, &row, row_brush);
        DeleteObject(row_brush);

        const std::wstring name = wide_from_utf8(layer->name);

        const int vis_width = compact_panel ? 22 : 28;
        const int lock_width = compact_panel ? 22 : 28;
        RECT vis_rect{
            row.left + 2, row.top,
            row.left + 2 + vis_width, row.bottom};
        RECT lock_rect{
            vis_rect.right + 2, row.top,
            vis_rect.right + 2 + lock_width, row.bottom};

        const LONG name_right =
            compact_panel ? row.right - 4 : row.right - 44;
        RECT name_rect{
            lock_rect.right + 4, row.top,
            std::max<LONG>(lock_rect.right + 5, name_right), row.bottom};

        DrawTextW(dc, layer->visible ? L"V" : L"-", -1, &vis_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(dc, layer->locked ? L"L" : L"-", -1, &lock_rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(dc, name.c_str(), -1, &name_rect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (!compact_panel) {
            RECT swatch{row.right - 38, row.top + 5,
                        row.right - 24, row.bottom - 5};
            HBRUSH swatch_brush = CreateSolidBrush(
                RGB(layer->color.r, layer->color.g, layer->color.b));
            FillRect(dc, &swatch, swatch_brush);
            DeleteObject(swatch_brush);
            FrameRect(dc, &swatch,
                      static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

            RECT type_rect{
                row.right - 21, row.top + 2,
                row.right - 2, row.bottom - 2};
            HPEN type_pen = create_entity_pen(
                1, RGB(220, 230, 238), layer->line_type);
            HGDIOBJ previous_pen = SelectObject(dc, type_pen);
            const int type_y = (type_rect.top + type_rect.bottom) / 2;
            MoveToEx(dc, type_rect.left + 1, type_y, nullptr);
            LineTo(dc, type_rect.right - 1, type_y);
            SelectObject(dc, previous_pen);
            DeleteObject(type_pen);
        }
        y += 26;
    }

    const int properties_top =
        panel.top + panel_metrics.properties_top_offset;
    RECT prop_header{panel.left, properties_top, panel.right, properties_top + 28};
    HBRUSH prop_brush = CreateSolidBrush(RGB(32, 49, 63));
    FillRect(dc, &prop_header, prop_brush);
    DeleteObject(prop_brush);
    RECT prop_title{panel.left + 10, properties_top, panel.right - 8, properties_top + 28};
    DrawTextW(dc, L"Properties", -1, &prop_title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int py = properties_top + 34;
    const int key_width =
        acp::ui_layout::property_key_width(width);
    SetTextColor(dc, RGB(185, 198, 209));
    auto draw_property = [&](const wchar_t* key, const wchar_t* value) {
        const int row_height = panel_metrics.property_row_height;
        RECT key_rect{
            panel.left + 10, py,
            panel.left + 10 + key_width, py + row_height};
        RECT value_rect{
            key_rect.right + 6, py,
            panel.right - 8, py + row_height};
        DrawTextW(dc, key, -1, &key_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(dc, RGB(232, 237, 241));
        DrawTextW(dc, value, -1, &value_rect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(dc, RGB(185, 198, 209));
        py += panel_metrics.property_row_height;
    };

    wchar_t value[128]{};
    if (g_app.selected.has_value()) {
        swprintf_s(value, L"%llu",
                   static_cast<unsigned long long>(*g_app.selected));
        draw_property(L"Selected", value);
        if (const acp::EntityProperties* props =
                g_app.document.properties(*g_app.selected)) {
            swprintf_s(value, L"%u", static_cast<unsigned>(props->layer_id));
            draw_property(L"Layer", value);
            swprintf_s(value, L"%.2f mm",
                       g_app.document.effective_line_weight(*g_app.selected));
            draw_property(L"Lineweight*", value);
            draw_property(L"Visible", props->visible ? L"Yes" : L"No");
            draw_property(L"Locked",
                          g_app.document.entity_locked(*g_app.selected) ? L"Yes" : L"No");
        }
        if (const acp::Entity* entity = g_app.document.find(*g_app.selected)) {
            if (std::holds_alternative<acp::TextEntity>(*entity)) {
                const auto& text = std::get<acp::TextEntity>(*entity);
                draw_property(L"Type", L"Text");
                swprintf_s(value, L"%.2f", text.height);
                draw_property(L"Height*", value);
                swprintf_s(value, L"%.1f deg",
                           text.rotation * 180.0 / std::numbers::pi);
                draw_property(L"Rotation*", value);
                const std::wstring content = wide_from_utf8(text.text);
                draw_property(L"Content*", content.c_str());
            } else if (std::holds_alternative<acp::LinearDimensionEntity>(*entity)) {
                const auto& dimension = std::get<acp::LinearDimensionEntity>(*entity);
                draw_property(L"Type", L"Dimension");
                swprintf_s(value, L"%.2f", acp::annotation::measurement(dimension));
                draw_property(L"Measurement", value);
                if (dimension.text_override.has_value()) {
                    const std::wstring override_text =
                        wide_from_utf8(*dimension.text_override);
                    draw_property(L"Override*", override_text.c_str());
                } else {
                    draw_property(L"Override*", L"(measured)");
                }
            } else if (std::holds_alternative<acp::HatchEntity>(*entity)) {
                const auto& hatch = std::get<acp::HatchEntity>(*entity);
                draw_property(L"Type", L"Hatch");
                draw_property(L"Fill", hatch.solid ? L"Solid" : L"Pattern");
                swprintf_s(value, L"%.1f deg",
                           hatch.angle * 180.0 / std::numbers::pi);
                draw_property(L"Angle*", value);
                swprintf_s(value, L"%.2f", hatch.spacing);
                draw_property(L"Spacing*", value);
            } else if (std::holds_alternative<acp::BlockReferenceEntity>(*entity)) {
                const auto& block =
                    std::get<acp::BlockReferenceEntity>(*entity);
                draw_property(L"Type", L"BlockRef");
                const acp::BlockDefinition* definition =
                    g_app.blocks.find(block.block_id);
                const std::wstring block_name =
                    definition != nullptr
                        ? wide_from_utf8(definition->name)
                        : L"(missing)";
                draw_property(L"Block", block_name.c_str());
                swprintf_s(value, L"%.3f", block.scale);
                draw_property(L"Scale*", value);
                swprintf_s(value, L"%.1f deg",
                           block.rotation * 180.0 / std::numbers::pi);
                draw_property(L"Rotation*", value);
            }
        }

        if (const acp::EntityProperties* props =
                g_app.document.properties(*g_app.selected)) {
            const std::wstring color_text =
                props->color_override.has_value()
                    ? format_rgb(*props->color_override)
                    : L"ByLayer";
            draw_property(L"Color*", color_text.c_str());

            const std::wstring type_text =
                props->line_type_override.has_value()
                    ? std::wstring{line_type_name(*props->line_type_override)}
                    : L"ByLayer";
            draw_property(L"Linetype*", type_text.c_str());
        }
    } else {
        draw_property(L"Selected", L"None");
        swprintf_s(value, L"%u", static_cast<unsigned>(g_app.active_layer));
        draw_property(L"Layer", value);
        if (const acp::Layer* layer =
                g_app.document.layer(g_app.active_layer)) {
            const std::wstring layer_name = wide_from_utf8(layer->name);
            draw_property(L"Layer name", layer_name.c_str());
            swprintf_s(value, L"%.2f mm", layer->line_weight);
            draw_property(L"Layer weight", value);
            const std::wstring layer_color = format_rgb(layer->color);
            draw_property(L"Layer color", layer_color.c_str());
            draw_property(L"Layer type", line_type_name(layer->line_type));
        }
    }

    swprintf_s(value, L"%.0f%%", g_app.zoom * 100.0);
    draw_property(L"Zoom", value);
    draw_property(L"Object snap", g_app.snap_enabled ? L"On (F3)" : L"Off (F3)");
    draw_property(L"Paper", paper_size_name(g_app.page_setup.paper));
    draw_property(
        L"Orientation",
        g_app.page_setup.orientation == acp::layout::Orientation::Landscape
            ? L"Landscape"
            : L"Portrait");
    if (g_app.print_scale_denominator.has_value()) {
        swprintf_s(value, L"1:%.0f", *g_app.print_scale_denominator);
        draw_property(L"Print scale", value);
    } else {
        draw_property(L"Print scale", L"Fit");
    }

    (void)hwnd;
}


bool handle_property_panel_double_click(HWND hwnd, POINT point) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int top = toolbar_height(client);
    const int panel_width = layer_panel_width(client);
    const int panel_left = client.right - panel_width;
    const int panel_bottom = client.bottom - kStatusHeight;
    const int key_width =
        acp::ui_layout::property_key_width(panel_width);
    const int value_left = panel_left + 10 + key_width + 6;
    if (point.x < value_left ||
        point.x >= client.right - 8 ||
        point.y < top ||
        point.y >= panel_bottom ||
        !g_app.selected.has_value()) {
        return false;
    }

    int layer_y = top + 32;
    for (const acp::LayerId id : g_app.document.layer_ids()) {
        if (g_app.document.layer(id) != nullptr) {
            layer_y += 26;
        }
    }
    const int properties_top = layer_y + 10;
    const int values_top = properties_top + 34;
    if (point.y < values_top) {
        return false;
    }

    const int row = (point.y - values_top) / 22;
    if (row == 2) {
        return edit_selected_property(
            hwnd, DirectProperty::LineWeight);
    }

    const acp::Entity* entity =
        g_app.document.find(*g_app.selected);
    if (entity == nullptr) {
        return false;
    }

    if (std::holds_alternative<acp::TextEntity>(*entity)) {
        if (row == 6) return edit_selected_property(hwnd, DirectProperty::TextHeight);
        if (row == 7) return edit_selected_property(hwnd, DirectProperty::TextRotation);
        if (row == 8) return edit_selected_property(hwnd, DirectProperty::TextContent);
    } else if (std::holds_alternative<acp::LinearDimensionEntity>(*entity)) {
        if (row == 7) return edit_selected_property(hwnd, DirectProperty::DimensionOverride);
    } else if (std::holds_alternative<acp::HatchEntity>(*entity)) {
        if (row == 7) return edit_selected_property(hwnd, DirectProperty::HatchAngle);
        if (row == 8) return edit_selected_property(hwnd, DirectProperty::HatchSpacing);
    } else if (std::holds_alternative<acp::BlockReferenceEntity>(*entity)) {
        if (row == 7) return edit_selected_property(hwnd, DirectProperty::BlockScale);
        if (row == 8) return edit_selected_property(hwnd, DirectProperty::BlockRotation);
    }

    int color_row = 5;
    if (std::holds_alternative<acp::TextEntity>(*entity)) {
        color_row = 9;
    } else if (std::holds_alternative<acp::LinearDimensionEntity>(*entity)) {
        color_row = 8;
    } else if (std::holds_alternative<acp::HatchEntity>(*entity) ||
               std::holds_alternative<acp::BlockReferenceEntity>(*entity)) {
        color_row = 9;
    }

    if (row == color_row) {
        return edit_selected_property(hwnd, DirectProperty::Color);
    }
    if (row == color_row + 1) {
        return edit_selected_property(hwnd, DirectProperty::LineType);
    }

    return true;
}

bool handle_layer_panel_double_click(HWND hwnd, POINT point) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int top = toolbar_height(client);
    const int panel_left =
        client.right - layer_panel_width(client);
    if (point.x < panel_left ||
        point.y < top ||
        point.y >= client.bottom - kStatusHeight) {
        return false;
    }

    int y = top + 32;
    for (const acp::LayerId id : g_app.document.layer_ids()) {
        const acp::Layer* layer = g_app.document.layer(id);
        if (layer == nullptr) {
            continue;
        }

        RECT row{
            panel_left + 6, y,
            client.right - 6, y + 24};
        if (PtInRect(&row, point)) {
            const bool compact_panel =
                acp::ui_layout::compact_right_panel(
                    layer_panel_width(client));
            const int control_edge =
                row.left + (compact_panel ? 48 : 58);
            if (point.x < control_edge) {
                return true;
            }
            g_app.active_layer = id;
            if (!compact_panel && point.x >= row.right - 22) {
                (void)edit_active_layer_property(
                    hwnd, DirectLayerProperty::LineType);
            } else if (!compact_panel && point.x >= row.right - 40) {
                (void)edit_active_layer_property(
                    hwnd, DirectLayerProperty::Color);
            } else {
                (void)edit_active_layer_property(
                    hwnd, DirectLayerProperty::Name);
            }
            return true;
        }
        y += 26;
    }
    return false;
}

bool handle_layer_panel_click(HWND hwnd, POINT point) {
    RECT client{};
    GetClientRect(hwnd, &client);
    const int top = toolbar_height(client);
    const int panel_left = client.right - layer_panel_width(client);
    if (point.x < panel_left || point.y < top ||
        point.y >= client.bottom - kStatusHeight) {
        return false;
    }

    int y = top + 32;
    for (const acp::LayerId id : g_app.document.layer_ids()) {
        const acp::Layer* layer = g_app.document.layer(id);
        if (layer == nullptr) {
            continue;
        }
        RECT row{panel_left + 6, y, client.right - 6, y + 24};
        if (PtInRect(&row, point)) {
            const bool compact_panel =
                acp::ui_layout::compact_right_panel(
                    layer_panel_width(client));
            const int visibility_edge =
                row.left + (compact_panel ? 24 : 30);
            const int lock_edge =
                row.left + (compact_panel ? 48 : 58);
            if (point.x < visibility_edge) {
                acp::Layer replacement = *layer;
                replacement.visible = !replacement.visible;
                if (apply_history(std::make_unique<acp::UpdateLayerCommand>(
                        id, replacement)) &&
                    !replacement.visible &&
                    g_app.selected.has_value() &&
                    !g_app.document.entity_visible(*g_app.selected)) {
                    g_app.selected.reset();
                }
            } else if (point.x < lock_edge) {
                acp::Layer replacement = *layer;
                replacement.locked = !replacement.locked;
                (void)apply_history(std::make_unique<acp::UpdateLayerCommand>(
                        id, replacement));
            } else {
                g_app.active_layer = id;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        }
        y += 26;
    }
    return true;
}

void create_layer(HWND hwnd) {
    int suffix = 1;
    while (true) {
        const std::string candidate = "Layer " + std::to_string(suffix);
        auto command = std::make_unique<acp::CreateLayerCommand>(candidate);
        auto* command_ptr = command.get();
        if (apply_history(std::move(command))) {
            g_app.active_layer = command_ptr->id();
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        ++suffix;
        if (suffix > 9999) {
            return;
        }
    }
}

void create_block_from_selected(HWND hwnd) {
    if (!selected_editable()) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    const acp::Entity* entity =
        g_app.document.find(*g_app.selected);
    if (entity == nullptr ||
        !(std::holds_alternative<LineEntity>(*entity) ||
          std::holds_alternative<CircleEntity>(*entity) ||
          std::holds_alternative<ArcEntity>(*entity) ||
          std::holds_alternative<PolylineEntity>(*entity))) {
        MessageBoxW(
            hwnd,
            L"Create Block currently accepts Line, Circle, Arc or Polyline geometry.",
            L"Auto CAD Pro Blocks",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    int suffix = 1;
    while (suffix <= 9999) {
        const std::string name =
            "Block " + std::to_string(suffix);

        bool exists = false;
        for (const acp::BlockId id : g_app.blocks.ids()) {
            const acp::BlockDefinition* definition =
                g_app.blocks.find(id);
            if (definition != nullptr &&
                definition->name == name) {
                exists = true;
                break;
            }
        }
        if (exists) {
            ++suffix;
            continue;
        }

        auto command =
            std::make_unique<acp::CreateBlockFromEntityCommand>(
                *g_app.selected, name);
        auto* command_ptr = command.get();
        if (apply_history(std::move(command))) {
            g_app.active_block = command_ptr->block_id();
            set_tool(hwnd, Tool::Select);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }
}

void begin_insert_active_block(HWND hwnd) {
    ensure_active_block_exists();
    if (!g_app.active_block.has_value()) {
        MessageBoxW(
            hwnd,
            L"No block definition is available. Create a block first.",
            L"Auto CAD Pro Blocks",
            MB_OK | MB_ICONINFORMATION);
        return;
    }
    set_tool(hwnd, Tool::BlockInsert);
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
    if (apply_history(std::make_unique<acp::UpdateEntityPropertiesCommand>(
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
    swprintf_s(buffer, L"%s  Tool: %s    X: %.2f    Y: %.2f    Zoom: %.0f%%    Entities: %zu    Selected: %llu    Layer: %u (%s)    SNAP: %s    Undo: %zu",
               g_app.dirty ? L"*" : L" ",
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

std::optional<std::filesystem::path> application_directory() {
    std::array<wchar_t, 4096> buffer{};
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return std::nullopt;
    }
    return std::filesystem::path(buffer.data()).parent_path();
}

std::optional<std::string> load_bundled_pdf_font() {
    const auto directory = application_directory();
    if (!directory.has_value()) {
        return std::nullopt;
    }
    return read_text_file(*directory / L"assets" / L"NotoSansJP.ttf");
}

void fit_drawing(HWND hwnd);

bool write_text_file(const std::filesystem::path& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return output.good();
}

std::filesystem::path recovery_snapshot_path() {
    std::array<wchar_t, 4096> local_app_data{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        local_app_data.data(),
        static_cast<DWORD>(local_app_data.size()));

    std::filesystem::path root;
    if (length > 0 && length < local_app_data.size()) {
        root = std::filesystem::path(local_app_data.data());
    } else {
        std::error_code ec;
        root = std::filesystem::temp_directory_path(ec);
        if (ec) {
            root = std::filesystem::current_path(ec);
        }
    }

    return root / L"AutoCADPro" / L"recovery.acp";
}

acp::persistence::ProjectSettings current_project_settings() {
    return acp::persistence::ProjectSettings{
        g_app.page_setup,
        g_app.print_scale_denominator
    };
}

void apply_project_settings(const acp::persistence::ProjectSettings& settings) {
    g_app.page_setup = settings.page_setup;
    g_app.print_scale_denominator = settings.print_scale_denominator;
}

void reset_project_settings() {
    g_app.page_setup = acp::layout::PageSetup{};
    g_app.print_scale_denominator.reset();
}

bool autosave_recovery_snapshot() {
    if (!g_app.dirty) {
        return true;
    }
    return acp::recovery::write_snapshot(
        recovery_snapshot_path(),
        g_app.document,
        g_app.blocks,
        current_project_settings());
}

void clear_recovery_snapshot() {
    (void)acp::recovery::remove_snapshot(recovery_snapshot_path());
}

void restore_recovery_if_available(HWND hwnd) {
    const auto path = recovery_snapshot_path();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return;
    }

    auto recovered = acp::recovery::load_snapshot(path);
    if (!recovered.has_value()) {
        const int remove = MessageBoxW(
            hwnd,
            L"An invalid recovery snapshot was found.\n\nRemove it?",
            L"Auto CAD Pro Recovery",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1);
        if (remove == IDYES) {
            clear_recovery_snapshot();
        }
        return;
    }

    const int result = MessageBoxW(
        hwnd,
        L"Auto CAD Pro found an autosaved recovery drawing from a previous session.\n\nRecover it now?",
        L"Auto CAD Pro Recovery",
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);

    if (result == IDYES) {
        g_app.document = std::move(recovered->document);
        g_app.blocks = std::move(recovered->blocks);
        apply_project_settings(recovered->settings);
        g_app.history = acp::History{};
        g_app.active_layer = acp::kDefaultLayerId;
        g_app.project_path.reset();
        g_app.dirty = true;
        reset_interaction_state();
        fit_drawing(hwnd);
    } else {
        clear_recovery_snapshot();
    }
}

#ifdef ACP_ENABLE_GUI_TEST_HOOKS
bool write_gui_test_snapshot(WPARAM snapshot_id) {
    std::array<wchar_t, 4096> directory{};
    const DWORD length = GetEnvironmentVariableW(
        L"ACP_GUI_TEST_SNAPSHOT_DIR",
        directory.data(),
        static_cast<DWORD>(directory.size()));
    if (length == 0 || length >= directory.size()) {
        return false;
    }

    const auto path = std::filesystem::path(directory.data()) /
        (L"gui-interaction-" + std::to_wstring(snapshot_id) + L".acp2d");
    return write_text_file(
        path,
        acp::persistence::serialize_project(
            g_app.document, g_app.blocks, current_project_settings()));
}
#endif

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
    if (!confirm_discard_unsaved(hwnd)) {
        return;
    }

    g_app.document = std::move(project->document);
    g_app.blocks = std::move(project->blocks);
    apply_project_settings(project->settings);
    g_app.history = acp::History{};
    g_app.active_layer = acp::kDefaultLayerId;
    g_app.dirty = false;
    g_app.project_path = *path;
    clear_recovery_snapshot();
    reset_interaction_state();
    fit_drawing(hwnd);
}

void save_project(HWND hwnd, bool save_as = false) {
    std::optional<std::filesystem::path> path = g_app.project_path;
    if (save_as || !path.has_value()) {
        path = choose_file(
            hwnd, true,
            L"Auto CAD Pro Project (*.acp)\0*.acp\0All Files (*.*)\0*.*\0\0",
            L"acp");
        if (!path.has_value()) {
            return;
        }
    }

    if (!acp::persistence::save_project_atomic(
            *path,
            g_app.document,
            g_app.blocks,
            current_project_settings())) {
        show_file_error(
            hwnd,
            L"Could not save the project atomically. The previous project file was left unchanged.");
        return;
    }

    g_app.project_path = *path;
    g_app.dirty = false;
    clear_recovery_snapshot();
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
    if (!confirm_discard_unsaved(hwnd)) {
        return;
    }

    g_app.document = std::move(result->document);
    g_app.blocks = acp::BlockLibrary{};
    reset_project_settings();
    g_app.history = acp::History{};
    g_app.active_layer = acp::kDefaultLayerId;
    g_app.dirty = true;
    g_app.project_path.reset();
    reset_interaction_state();
    fit_drawing(hwnd);
}


void export_svg(HWND hwnd) {
    const auto path = choose_file(
        hwnd, true,
        L"Scalable Vector Graphics (*.svg)\0*.svg\0All Files (*.*)\0*.*\0\0",
        L"svg");
    if (!path.has_value()) {
        return;
    }

    const auto data = acp::svg::export_document(
        g_app.document, &g_app.blocks, 10.0);
    if (!data.has_value()) {
        show_file_error(hwnd, L"Nothing exportable was found in the drawing.");
        return;
    }

    if (!write_text_file(*path, *data)) {
        show_file_error(hwnd, L"Could not export the SVG drawing.");
    }
}

void export_pdf(HWND hwnd) {
    const auto path = choose_file(
        hwnd, true,
        L"PDF document (*.pdf)\0*.pdf\0All files (*.*)\0*.*\0\0",
        L"pdf");
    if (!path.has_value()) return;

    const auto font_bytes = load_bundled_pdf_font();
    const std::optional<acp::pdf::FontData> font =
        font_bytes.has_value()
            ? std::optional<acp::pdf::FontData>{
                  acp::pdf::FontData{*font_bytes, "NotoSansJP"}}
            : std::nullopt;

    const auto data = acp::pdf::export_document(
        g_app.document,
        &g_app.blocks,
        g_app.page_setup,
        g_app.print_scale_denominator,
        font.has_value() ? &*font : nullptr);
    if (!data.has_value() || !write_text_file(*path, *data)) {
        show_file_error(
            hwnd,
            font_bytes.has_value()
                ? L"Could not export PDF."
                : L"Could not export PDF because the bundled Unicode font is missing.");
    }
}


void export_dxf(HWND hwnd) {
    const auto path = choose_file(
        hwnd, true,
        L"DXF Drawing (*.dxf)\0*.dxf\0All Files (*.*)\0*.*\0\0",
        L"dxf");
    if (!path.has_value()) {
        return;
    }

    const auto result = acp::dxf::export_ascii_report(g_app.document);
    if (result.skipped != 0) {
        std::wstring message =
            L"This DXF subset supports Line, Circle, Arc, Polyline and Text.\n\n";
        message += std::to_wstring(result.skipped);
        message +=
            L" visible unsupported or invalid entity/entities will be omitted.\n"
            L"Continue exporting the supported geometry?";
        if (MessageBoxW(
                hwnd,
                message.c_str(),
                L"Auto CAD Pro DXF Export",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
            return;
        }
    }

    if (!write_text_file(*path, result.data)) {
        show_file_error(hwnd, L"Could not export the DXF drawing.");
    }
}

void handle_left_click(HWND hwnd, POINT point) {
    if (handle_toolbar_click(hwnd, point)) {
        return;
    }
    if (handle_left_tool_rail_click(hwnd, point)) {
        return;
    }
    if (handle_layer_panel_click(hwnd, point)) {
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
         g_app.tool == Tool::Rectangle ||
         g_app.tool == Tool::Dimension ||
         g_app.tool == Tool::Text ||
         g_app.tool == Tool::BlockInsert) &&
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

    if (g_app.tool == Tool::BlockInsert) {
        ensure_active_block_exists();
        if (!g_app.active_block.has_value() ||
            !active_layer_writable()) {
            MessageBeep(MB_ICONWARNING);
            return;
        }

        auto command = std::make_unique<acp::AddEntityCommand>(
            acp::BlockReferenceEntity{
                *g_app.active_block, world, 0.0, 1.0});
        auto* command_ptr = command.get();
        if (apply_history(std::move(command))) {
            g_app.document.set_entity_layer(
                command_ptr->id(), g_app.active_layer);
            g_app.selected = command_ptr->id();
        }
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
                if (apply_history(std::move(command))) {
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
                    (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
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
                    (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
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
                    if (apply_history(std::move(command))) {
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
                if (apply_history(std::move(command))) {
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
                    (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
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
                    (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
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
            if (apply_history(std::move(command))) {
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

    if (g_app.tool == Tool::Rectangle) {
        const Vec2 a = g_app.first_point;
        const Vec2 b = world;
        if (std::abs(a.x - b.x) > acp::geo::kEpsilon &&
            std::abs(a.y - b.y) > acp::geo::kEpsilon) {
            std::vector<Vec2> points{
                {a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}
            };
            auto command = std::make_unique<acp::AddEntityCommand>(
                PolylineEntity{std::move(points), true});
            auto* command_ptr = command.get();
            if (apply_history(std::move(command))) {
                g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                g_app.selected = command_ptr->id();
            }
        }
    } else if (g_app.tool == Tool::Line) {
        if (acp::geo::distance(g_app.first_point, world) > acp::geo::kEpsilon) {
            auto command = std::make_unique<acp::AddEntityCommand>(
                LineEntity{{g_app.first_point, world}});
            auto* command_ptr = command.get();
            if (apply_history(std::move(command))) {
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
            if (apply_history(std::move(command))) {
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
                (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
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
                if (apply_history(std::move(command))) {
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
                    (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
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
#ifdef ACP_ENABLE_GUI_TEST_HOOKS
        case kGuiTestSnapshotMessage:
            return write_gui_test_snapshot(w_param) ? 1 : 0;
#endif
        case WM_COMMAND:
            switch (LOWORD(w_param)) {
                case kMenuNew:
                    if (!confirm_discard_unsaved(hwnd)) {
                        return 0;
                    }
                    g_app.document = Document{};
                    g_app.history = acp::History{};
                    g_app.blocks = acp::BlockLibrary{};
                    reset_project_settings();
                    g_app.active_layer = acp::kDefaultLayerId;
                    g_app.dirty = false;
                    g_app.project_path.reset();
                    clear_recovery_snapshot();
                    reset_interaction_state();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case kMenuOpen:
                    open_project(hwnd);
                    return 0;
                case kMenuSave:
                    save_project(hwnd, false);
                    return 0;
                case kMenuSaveAs:
                    save_project(hwnd, true);
                    return 0;
                case kMenuImportDxf:
                    import_dxf(hwnd);
                    return 0;
                case kMenuExportDxf:
                    export_dxf(hwnd);
                    return 0;
                case kMenuExportSvg:
                    export_svg(hwnd);
                    return 0;
                case kMenuExportPdf:
                    export_pdf(hwnd);
                    return 0;
                case kMenuZoomExtents:
                    fit_drawing(hwnd);
                    return 0;
                case kMenuCyclePaperSize:
                    cycle_paper_size();
                    g_app.dirty = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case kMenuToggleOrientation:
                    g_app.page_setup.orientation =
                        g_app.page_setup.orientation == acp::layout::Orientation::Landscape
                            ? acp::layout::Orientation::Portrait
                            : acp::layout::Orientation::Landscape;
                    g_app.dirty = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case kMenuCyclePrintScale:
                    cycle_print_scale();
                    g_app.dirty = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case kMenuCreateBlock:
                    create_block_from_selected(hwnd);
                    return 0;
                case kMenuInsertBlock:
                    begin_insert_active_block(hwnd);
                    return 0;
                case kMenuNewLayer:
                    create_layer(hwnd);
                    return 0;
                case kMenuEditLayerName:
                    (void)edit_active_layer_property(
                        hwnd, DirectLayerProperty::Name);
                    return 0;
                case kMenuEditLayerWeight:
                    (void)edit_active_layer_property(
                        hwnd, DirectLayerProperty::LineWeight);
                    return 0;
                case kMenuEditLayerColor:
                    (void)edit_active_layer_property(
                        hwnd, DirectLayerProperty::Color);
                    return 0;
                case kMenuEditLayerLineType:
                    (void)edit_active_layer_property(
                        hwnd, DirectLayerProperty::LineType);
                    return 0;
                case kMenuAssignLayer:
                    assign_selected_to_active_layer(hwnd);
                    return 0;
                case kMenuToggleLayerVisible:
                    if (const acp::Layer* layer = g_app.document.layer(g_app.active_layer)) {
                        acp::Layer replacement = *layer;
                        replacement.visible = !replacement.visible;
                        (void)apply_history(std::make_unique<acp::UpdateLayerCommand>(
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
                            (void)apply_history(std::make_unique<acp::UpdateEntityPropertiesCommand>(
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
                case kMenuEditLineWeight:
                    (void)edit_selected_property(hwnd, DirectProperty::LineWeight);
                    return 0;
                case kMenuEditEntityColor:
                    (void)edit_selected_property(hwnd, DirectProperty::Color);
                    return 0;
                case kMenuEditEntityLineType:
                    (void)edit_selected_property(hwnd, DirectProperty::LineType);
                    return 0;
                case kMenuEditTextContent:
                    (void)edit_selected_property(hwnd, DirectProperty::TextContent);
                    return 0;
                case kMenuEditTextHeight:
                    (void)edit_selected_property(hwnd, DirectProperty::TextHeight);
                    return 0;
                case kMenuEditTextRotation:
                    (void)edit_selected_property(hwnd, DirectProperty::TextRotation);
                    return 0;
                case kMenuEditDimensionOverride:
                    (void)edit_selected_property(hwnd, DirectProperty::DimensionOverride);
                    return 0;
                case kMenuEditHatchAngle:
                    (void)edit_selected_property(hwnd, DirectProperty::HatchAngle);
                    return 0;
                case kMenuEditHatchSpacing:
                    (void)edit_selected_property(hwnd, DirectProperty::HatchSpacing);
                    return 0;
                case kMenuEditBlockScale:
                    (void)edit_selected_property(hwnd, DirectProperty::BlockScale);
                    return 0;
                case kMenuEditBlockRotation:
                    (void)edit_selected_property(hwnd, DirectProperty::BlockRotation);
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
                            (void)apply_history(std::make_unique<acp::UpdateEntityPropertiesCommand>(
                                    *g_app.selected, replacement));
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    return 0;
                case kMenuTextCycleHeight:
                    if (selected_editable()) {
                        if (const acp::Entity* entity =
                                g_app.document.find(*g_app.selected);
                            entity != nullptr &&
                            std::holds_alternative<acp::TextEntity>(*entity)) {
                            acp::Entity replacement = *entity;
                            auto& text = std::get<acp::TextEntity>(replacement);
                            if (text.height < 3.75) text.height = 5.0;
                            else if (text.height < 7.5) text.height = 10.0;
                            else text.height = 2.5;
                            (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
                                *g_app.selected, replacement));
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    return 0;
                case kMenuTextRotate15:
                    if (selected_editable()) {
                        if (const acp::Entity* entity =
                                g_app.document.find(*g_app.selected);
                            entity != nullptr &&
                            std::holds_alternative<acp::TextEntity>(*entity)) {
                            acp::Entity replacement = *entity;
                            auto& text = std::get<acp::TextEntity>(replacement);
                            text.rotation += std::numbers::pi / 12.0;
                            (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
                                *g_app.selected, replacement));
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    return 0;
                case kMenuDimensionShiftLine:
                    if (selected_editable()) {
                        if (const acp::Entity* entity =
                                g_app.document.find(*g_app.selected);
                            entity != nullptr &&
                            std::holds_alternative<acp::LinearDimensionEntity>(*entity)) {
                            acp::Entity replacement = *entity;
                            auto& dimension =
                                std::get<acp::LinearDimensionEntity>(replacement);
                            const Vec2 delta = dimension.second - dimension.first;
                            const double length = acp::geo::length(delta);
                            if (length > acp::geo::kEpsilon) {
                                const Vec2 normal{-delta.y / length, delta.x / length};
                                dimension.line_point = dimension.line_point + normal * 5.0;
                                (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
                                    *g_app.selected, replacement));
                                InvalidateRect(hwnd, nullptr, FALSE);
                            }
                        }
                    }
                    return 0;
                case kMenuHatchToggleSolid:
                case kMenuHatchRotate45:
                case kMenuHatchCycleSpacing:
                    if (selected_editable()) {
                        if (const acp::Entity* entity =
                                g_app.document.find(*g_app.selected);
                            entity != nullptr &&
                            std::holds_alternative<acp::HatchEntity>(*entity)) {
                            acp::Entity replacement = *entity;
                            auto& hatch = std::get<acp::HatchEntity>(replacement);
                            if (LOWORD(w_param) == kMenuHatchToggleSolid) {
                                hatch.solid = !hatch.solid;
                            } else if (LOWORD(w_param) == kMenuHatchRotate45) {
                                hatch.angle += std::numbers::pi / 4.0;
                            } else {
                                if (hatch.spacing < 1.5) hatch.spacing = 2.0;
                                else if (hatch.spacing < 3.5) hatch.spacing = 5.0;
                                else hatch.spacing = 1.0;
                            }
                            if (acp::hatch::valid(hatch)) {
                                (void)apply_history(std::make_unique<acp::UpdateEntityCommand>(
                                    *g_app.selected, replacement));
                                InvalidateRect(hwnd, nullptr, FALSE);
                            }
                        }
                    }
                    return 0;
                case kMenuToggleLayerLock:
                    if (const acp::Layer* layer = g_app.document.layer(g_app.active_layer)) {
                        acp::Layer replacement = *layer;
                        replacement.locked = !replacement.locked;
                        (void)apply_history(std::make_unique<acp::UpdateLayerCommand>(
                                g_app.active_layer, replacement));
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                case kMenuExit:
                    if (confirm_discard_unsaved(hwnd)) {
                        clear_recovery_snapshot();
                        DestroyWindow(hwnd);
                    }
                    return 0;
                case kToolSelect: set_tool(hwnd, Tool::Select); return 0;
                case kToolLine: set_tool(hwnd, Tool::Line); return 0;
                case kToolCircle: set_tool(hwnd, Tool::Circle); return 0;
                case kToolPolyline: set_tool(hwnd, Tool::Polyline); return 0;
                case kToolArc: set_tool(hwnd, Tool::Arc); return 0;
                case kToolRectangle: set_tool(hwnd, Tool::Rectangle); return 0;
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
                case kToolBlockInsert: begin_insert_active_block(hwnd); return 0;
                default: break;
            }
            break;

        case WM_KEYDOWN: {
            const bool control_down =
                (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift_down =
                (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (control_down && w_param == 'S') {
                save_project(hwnd, shift_down);
                return 0;
            }
            if (control_down && w_param == 'O') {
                open_project(hwnd);
                return 0;
            }
            if (control_down && w_param == 'N') {
                SendMessageW(hwnd, WM_COMMAND, kMenuNew, 0);
                return 0;
            }
            if (control_down && w_param == 'Z') {
                if (g_app.history.undo(g_app.document, g_app.blocks)) {
                    g_app.dirty = true;
                    ensure_active_layer_exists();
                    ensure_active_block_exists();
                    if (g_app.selected.has_value() &&
                        g_app.document.find(*g_app.selected) == nullptr) {
                        g_app.selected.reset();
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (control_down && w_param == 'Y') {
                if (g_app.history.redo(g_app.document, g_app.blocks)) {
                    g_app.dirty = true;
                    ensure_active_layer_exists();
                    ensure_active_block_exists();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (w_param == VK_DELETE && g_app.selected.has_value()) {
                if (!selected_editable()) {
                    return 0;
                }
                const acp::EntityId id = *g_app.selected;
                if (apply_history(std::make_unique<acp::RemoveEntityCommand>(id))) {
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
                    if (apply_history(std::move(command))) {
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
                    if (apply_history(std::move(command))) {
                        g_app.document.set_entity_layer(command_ptr->id(), g_app.active_layer);
                        g_app.selected = command_ptr->id();
                    }
                }
                g_app.polyline_points.clear();
                g_app.has_first_point = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (w_param == 'K') {
                begin_insert_active_block(hwnd);
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
            if (w_param == 'B') {
                set_tool(hwnd, Tool::Rectangle);
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
        }

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

        case WM_LBUTTONDBLCLK: {
            const POINT p{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
            if (handle_property_panel_double_click(hwnd, p) ||
                handle_layer_panel_double_click(hwnd, p)) {
                return 0;
            }
            break;
        }

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
            draw_left_tool_rail(memory, client);
            draw_layer_panel(hwnd, memory, client);
            draw_status(hwnd, memory, client);

            BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, old_bitmap);
            DeleteObject(bitmap);
            DeleteDC(memory);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER:
            if (w_param == kAutosaveTimerId && g_app.dirty) {
                if (!autosave_recovery_snapshot()) {
                    KillTimer(hwnd, kAutosaveTimerId);
                    MessageBoxW(
                        hwnd,
                        L"Autosave recovery could not write its snapshot.\n"
                        L"Automatic recovery has been disabled for this session.",
                        L"Auto CAD Pro Recovery",
                        MB_OK | MB_ICONWARNING);
                }
                return 0;
            }
            break;

        case WM_CLOSE:
            if (confirm_discard_unsaved(hwnd)) {
                clear_recovery_snapshot();
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, kAutosaveTimerId);
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
    HMENU block_menu = CreatePopupMenu();
    HMENU layout_menu = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, kMenuNew, L"&New");
    AppendMenuW(file, MF_STRING, kMenuOpen, L"&Open Project...\tCtrl+O");
    AppendMenuW(file, MF_STRING, kMenuSave, L"&Save Project\tCtrl+S");
    AppendMenuW(file, MF_STRING, kMenuSaveAs, L"Save Project &As...\tCtrl+Shift+S");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kMenuImportDxf, L"&Import DXF...");
    AppendMenuW(file, MF_STRING, kMenuExportDxf, L"&Export DXF...");
    AppendMenuW(file, MF_STRING, kMenuExportSvg, L"Export &SVG...");
    AppendMenuW(file, MF_STRING, kMenuExportPdf, L"Export &PDF...");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kMenuExit, L"E&xit");

    AppendMenuW(draw, MF_STRING, kToolSelect, L"&Select\tEsc");
    AppendMenuW(draw, MF_STRING, kToolLine, L"&Line\tL");
    AppendMenuW(draw, MF_STRING, kToolCircle, L"&Circle\tC");
    AppendMenuW(draw, MF_STRING, kToolPolyline, L"&Polyline\tP");
    AppendMenuW(draw, MF_STRING, kToolArc, L"&Arc\tA");
    AppendMenuW(draw, MF_STRING, kToolRectangle, L"&Rectangle\tB");
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
    AppendMenuW(draw, MF_STRING, kToolBlockInsert, L"Insert &Block\tK");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(view, MF_STRING, kMenuZoomExtents, L"Zoom &Extents");
    AppendMenuW(view, MF_STRING, kMenuToggleSnap, L"Toggle Object &Snap\tF3");
    AppendMenuW(layer, MF_STRING, kMenuNewLayer, L"&New Layer");
    AppendMenuW(layer, MF_STRING, kMenuEditLayerName, L"&Rename Active Layer...");
    AppendMenuW(layer, MF_STRING, kMenuEditLayerWeight, L"Edit Active Layer &Line Weight...");
    AppendMenuW(layer, MF_STRING, kMenuEditLayerColor, L"Edit Active Layer &Color...");
    AppendMenuW(layer, MF_STRING, kMenuEditLayerLineType, L"Edit Active Layer &Linetype...");
    AppendMenuW(layer, MF_STRING, kMenuAssignLayer, L"&Assign Selected to Active");
    AppendMenuW(layer, MF_STRING, kMenuToggleLayerVisible, L"Toggle &Visibility");
    AppendMenuW(layer, MF_STRING, kMenuToggleLayerLock, L"Toggle &Lock");
    AppendMenuW(block_menu, MF_STRING, kMenuCreateBlock, L"&Create from Selected");
    AppendMenuW(block_menu, MF_STRING, kMenuInsertBlock, L"&Insert Active\tK");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(draw), L"&Draw");
    AppendMenuW(layout_menu, MF_STRING, kMenuCyclePaperSize, L"Cycle &Paper Size");
    AppendMenuW(layout_menu, MF_STRING, kMenuToggleOrientation, L"Toggle &Orientation");
    AppendMenuW(layout_menu, MF_STRING, kMenuCyclePrintScale, L"Cycle Print &Scale");
    AppendMenuW(entity, MF_STRING, kMenuToggleEntityVisible, L"Toggle &Visibility");
    AppendMenuW(entity, MF_STRING, kMenuCycleEntityWeight, L"Cycle Line &Weight");
    AppendMenuW(entity, MF_STRING, kMenuEditLineWeight, L"Edit Line Weight...");
    AppendMenuW(entity, MF_STRING, kMenuEditEntityColor, L"Edit Color...");
    AppendMenuW(entity, MF_STRING, kMenuEditEntityLineType, L"Edit Linetype...");
    AppendMenuW(entity, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(entity, MF_STRING, kMenuEditTextContent, L"Text: Edit Content...");
    AppendMenuW(entity, MF_STRING, kMenuEditTextHeight, L"Text: Edit Height...");
    AppendMenuW(entity, MF_STRING, kMenuEditTextRotation, L"Text: Edit Rotation...");
    AppendMenuW(entity, MF_STRING, kMenuEditDimensionOverride, L"Dimension: Edit Override...");
    AppendMenuW(entity, MF_STRING, kMenuEditHatchAngle, L"Hatch: Edit Angle...");
    AppendMenuW(entity, MF_STRING, kMenuEditHatchSpacing, L"Hatch: Edit Spacing...");
    AppendMenuW(entity, MF_STRING, kMenuEditBlockScale, L"Block: Edit Scale...");
    AppendMenuW(entity, MF_STRING, kMenuEditBlockRotation, L"Block: Edit Rotation...");
    AppendMenuW(entity, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(entity, MF_STRING, kMenuTextCycleHeight, L"Text: Cycle &Height");
    AppendMenuW(entity, MF_STRING, kMenuTextRotate15, L"Text: Rotate +15 deg");
    AppendMenuW(entity, MF_STRING, kMenuDimensionShiftLine, L"Dimension: Shift Line +5");
    AppendMenuW(entity, MF_STRING, kMenuHatchToggleSolid, L"Hatch: Toggle Solid");
    AppendMenuW(entity, MF_STRING, kMenuHatchRotate45, L"Hatch: Rotate +45 deg");
    AppendMenuW(entity, MF_STRING, kMenuHatchCycleSpacing, L"Hatch: Cycle Spacing");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(layer), L"&Layer");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(entity), L"&Entity");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(block_menu), L"&Block");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(layout_menu), L"&Layout");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"&View");
    return menu;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    constexpr wchar_t kClassName[] = L"AutoCADProMainWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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

    restore_recovery_if_available(hwnd);
    if (SetTimer(hwnd, kAutosaveTimerId, kAutosaveIntervalMs, nullptr) == 0) {
        MessageBoxW(
            hwnd,
            L"Autosave recovery timer could not be started.",
            L"Auto CAD Pro Recovery",
            MB_OK | MB_ICONWARNING);
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
