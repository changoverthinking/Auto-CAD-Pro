#pragma once

namespace acp::ui_layout {

struct Metrics {
    int toolbar_height{};
    int right_panel_width{};
    int left_rail_width{};
};

struct RightPanelMetrics {
    int property_row_height{};
    int visible_layer_rows{};
    int properties_top_offset{};
};

[[nodiscard]] Metrics metrics_for_client(int width, int height) noexcept;
[[nodiscard]] bool compact_right_panel(int panel_width) noexcept;
[[nodiscard]] int property_key_width(int panel_width) noexcept;
[[nodiscard]] RightPanelMetrics right_panel_metrics(
    int panel_height,
    int property_rows) noexcept;

} // namespace acp::ui_layout
