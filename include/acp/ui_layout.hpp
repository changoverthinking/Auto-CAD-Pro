#pragma once

namespace acp::ui_layout {

struct Metrics {
    int toolbar_height{};
    int right_panel_width{};
    int left_rail_width{};
};

[[nodiscard]] Metrics metrics_for_client(int width, int height) noexcept;

} // namespace acp::ui_layout
