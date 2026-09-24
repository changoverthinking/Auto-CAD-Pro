#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace acp::icons {

struct AtlasCell {
    int x{0};
    int y{0};
    int width{32};
    int height{32};
};

inline constexpr int kAtlasIconSize = 32;
inline constexpr int kAtlasColumns = 24;
inline constexpr int kAtlasWidth = 768;
inline constexpr int kAtlasHeight = 768;
inline constexpr std::size_t kIconCount = 553;

[[nodiscard]] std::span<const std::string_view> ids() noexcept;
[[nodiscard]] std::optional<std::size_t> index_of(std::string_view id) noexcept;
[[nodiscard]] AtlasCell cell_for(std::size_t index) noexcept;

} // namespace acp::icons
