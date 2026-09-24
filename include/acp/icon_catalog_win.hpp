#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cstddef>
#include <string_view>
namespace acp::icons {
struct Descriptor { std::string_view pack; std::string_view name; std::size_t offset; std::size_t length; };
[[nodiscard]] std::size_t count() noexcept;
[[nodiscard]] const Descriptor* find(std::string_view name) noexcept;
[[nodiscard]] bool draw(HDC dc, std::string_view name, const RECT& area, COLORREF color);
}
#endif
