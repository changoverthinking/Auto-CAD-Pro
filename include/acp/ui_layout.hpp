#pragma once

namespace acp::ui_layout {

struct Metrics {
    int toolbar_height{};
    int right_panel_width{};
    int left_rail_width{};
};

[[nodiscard]] Metrics metrics_for_client(int width, int height) noexcept;
[[nodiscard]] bool compact_right_panel(int panel_width) noexcept;
[[nodiscard]] int property_key_width(int panel_width) noexcept;

} // namespace acp::ui_layout
