#include "acp/coordinate_input.hpp"

#include <charconv>
#include <cmath>
#include <numbers>

namespace acp::input {
namespace {
std::string_view trim(std::string_view text) noexcept {
    constexpr std::string_view whitespace = " \t\r\n";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}
std::optional<double> number(std::string_view text) noexcept {
    text = trim(text);
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        if (!text.empty() && (text.front() == '+' || text.front() == '-')) return std::nullopt;
    }
    if (text.empty() || text.front() == '+') return std::nullopt;
    double value{};
    const auto parsed = std::from_chars(text.data(), text.data()+text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size() ||
        !std::isfinite(value)) return std::nullopt;
    return value;
}
bool finite(geo::Vec2 p) noexcept { return std::isfinite(p.x) && std::isfinite(p.y); }
}

std::optional<geo::Vec2> parse_coordinate(
    std::string_view text, std::optional<geo::Vec2> reference) noexcept {
    if (text.size() > 256) return std::nullopt;
    text = trim(text);
    if (text.empty()) return std::nullopt;
    const bool relative = text.front() == '@';
    if (relative) {
        if (!reference || !finite(*reference)) return std::nullopt;
        text.remove_prefix(1);
        text = trim(text);
    }
    const auto comma = text.find(',');
    const auto polar = text.find('<');
    if ((comma == std::string_view::npos) == (polar == std::string_view::npos)) return std::nullopt;
    if (polar != std::string_view::npos && !relative) return std::nullopt;
    const auto separator = comma != std::string_view::npos ? comma : polar;
    const auto first = number(text.substr(0, separator));
    const auto second = number(text.substr(separator+1));
    if (!first || !second) return std::nullopt;
    geo::Vec2 point{*first,*second};
    if (polar != std::string_view::npos) {
        if (*first < 0) return std::nullopt;
        const double radians = std::remainder(*second,360.0) * (std::numbers::pi/180.0);
        point = {*first*std::cos(radians),*first*std::sin(radians)};
    }
    if (relative) point = *reference + point;
    if (!finite(point)) return std::nullopt;
    return point;
}
}
