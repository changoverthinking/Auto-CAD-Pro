#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include "acp/document.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cwchar>
#include <numbers>
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
    Circle
};

struct AppState {
    Document document;
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
constexpr int kToolSelect = 2001;
constexpr int kToolLine = 2002;
constexpr int kToolCircle = 2003;

const wchar_t* tool_name(Tool tool) {
    switch (tool) {
        case Tool::Select: return L"Select";
        case Tool::Line: return L"Line";
        case Tool::Circle: return L"Circle";
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

    if (g_app.tool == Tool::Line) {
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
        {{344, 7, 419, 37}, L"Circle", Tool::Circle}
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
    swprintf_s(buffer, L"Tool: %s    X: %.2f    Y: %.2f    Zoom: %.0f%%    Entities: %zu",
               tool_name(g_app.tool), cursor_world.x, cursor_world.y,
               g_app.zoom * 100.0, g_app.document.size());

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(190, 195, 205));
    RECT text_rect{10, bar.top, bar.right - 10, bar.bottom};
    DrawTextW(dc, buffer, -1, &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void handle_left_click(HWND hwnd, POINT point) {
    if (point.y < kToolbarHeight) {
        if (point.x >= 190 && point.x <= 265) set_tool(hwnd, Tool::Select);
        else if (point.x >= 272 && point.x <= 337) set_tool(hwnd, Tool::Line);
        else if (point.x >= 344 && point.x <= 419) set_tool(hwnd, Tool::Circle);
        return;
    }

    const RECT canvas = canvas_rect(hwnd);
    if (!PtInRect(&canvas, point) || g_app.tool == Tool::Select) {
        return;
    }

    const Vec2 world = screen_to_world(hwnd, point);
    if (!g_app.has_first_point) {
        g_app.first_point = world;
        g_app.has_first_point = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    if (g_app.tool == Tool::Line) {
        if (acp::geo::distance(g_app.first_point, world) > acp::geo::kEpsilon) {
            (void)g_app.document.insert(LineEntity{{g_app.first_point, world}});
        }
    } else if (g_app.tool == Tool::Circle) {
        const double radius = acp::geo::distance(g_app.first_point, world);
        if (radius > acp::geo::kEpsilon) {
            (void)g_app.document.insert(CircleEntity{{g_app.first_point, radius}});
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
                    g_app.has_first_point = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                case kMenuExit:
                    DestroyWindow(hwnd);
                    return 0;
                case kToolSelect: set_tool(hwnd, Tool::Select); return 0;
                case kToolLine: set_tool(hwnd, Tool::Line); return 0;
                case kToolCircle: set_tool(hwnd, Tool::Circle); return 0;
                default: break;
            }
            break;

        case WM_KEYDOWN:
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

    AppendMenuW(file, MF_STRING, kMenuNew, L"&New");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kMenuExit, L"E&xit");

    AppendMenuW(draw, MF_STRING, kToolSelect, L"&Select\tEsc");
    AppendMenuW(draw, MF_STRING, kToolLine, L"&Line\tL");
    AppendMenuW(draw, MF_STRING, kToolCircle, L"&Circle\tC");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(draw), L"&Draw");
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
