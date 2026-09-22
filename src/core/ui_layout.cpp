#include "acp/ui_layout.hpp"

#include <algorithm>

namespace acp::ui_layout {

Metrics metrics_for_client(int width, int height) noexcept {
    width = std::max(width, 1);
    height = std::max(height, 1);

    const int toolbar = std::clamp(height / 32, 26, 32);
    const int dock = std::clamp((width * 17) / 100, 160, 300);

    return Metrics{
        toolbar,
        std::min(dock, std::max(1, width / 3)),
        0
    };
}

bool compact_right_panel(int panel_width) noexcept {
    return panel_width < 220;
}

int property_key_width(int panel_width) noexcept {
    panel_width = std::max(panel_width, 1);
    return std::clamp((panel_width * 42) / 100, 42, 78);
}

RightPanelMetrics right_panel_metrics(
    int panel_width,
    int panel_height,
    int property_rows,
    int total_layer_rows) noexcept {

    panel_width = std::max(panel_width, 1);
    panel_height = std::max(panel_height, 1);
    property_rows = std::max(property_rows, 1);
    total_layer_rows = std::max(total_layer_rows, 1);

    constexpr int kHeaderHeight = 24;
    constexpr int kBottomPadding = 4;
    constexpr int kLayerRowStride = 24;
    constexpr int kMinPropertyRowHeight = 14;
    constexpr int kMaxPropertyRowHeight = 20;

    const int property_budget =
        (panel_height - kHeaderHeight - kBottomPadding) / property_rows;
    const int property_row_height =
        std::clamp(
            property_budget,
            kMinPropertyRowHeight,
            kMaxPropertyRowHeight);

    const int layer_capacity =
        std::max(
            1,
            (panel_height - kHeaderHeight - kBottomPadding) /
                kLayerRowStride);
    const int visible_layers =
        std::min(total_layer_rows, layer_capacity);

    const int navigator_width =
        std::clamp((panel_width * 42) / 100, 86, 132);

    return RightPanelMetrics{
        property_row_height,
        visible_layers,
        std::min(navigator_width, std::max(1, panel_width - 84))
    };
}

} // namespace acp::ui_layout
