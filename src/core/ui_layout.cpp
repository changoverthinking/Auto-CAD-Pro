#include "acp/ui_layout.hpp"

#include <algorithm>

namespace acp::ui_layout {

Metrics metrics_for_client(int width, int height) noexcept {
    width = std::max(width, 1);
    height = std::max(height, 1);

    // Workspace contract:
    // - top command/tool area targets 1/20 of client height;
    // - right properties/layers panel targets 2/10 of client width;
    // - left tool rail targets 1/25 of client width.
    // Bounds keep the UI usable on small windows and prevent oversized chrome
    // on high-resolution displays.
    return Metrics{
        std::clamp(height / 20, 28, 54),
        std::clamp(width / 5, 160, 384),
        std::clamp(width / 25, 32, 72)
    };
}

bool compact_right_panel(int panel_width) noexcept {
    return panel_width < 190;
}

int property_key_width(int panel_width) noexcept {
    panel_width = std::max(panel_width, 1);
    return std::clamp((panel_width * 42) / 100, 48, 112);
}

RightPanelMetrics right_panel_metrics(
    int panel_height,
    int property_rows,
    int total_layer_rows) noexcept {

    panel_height = std::max(panel_height, 1);
    property_rows = std::max(property_rows, 1);
    total_layer_rows = std::max(total_layer_rows, 1);

    constexpr int kLayersHeaderHeight = 30;
    constexpr int kLayerRowStride = 24;
    constexpr int kSectionGap = 8;
    constexpr int kPropertiesHeaderHeight = 26;
    constexpr int kPropertiesGap = 4;
    constexpr int kMinPropertyRowHeight = 14;
    constexpr int kMaxPropertyRowHeight = 22;

    const int fixed_height =
        kLayersHeaderHeight + kSectionGap +
        kPropertiesHeaderHeight + kPropertiesGap;

    const int property_budget = std::max(
        kMinPropertyRowHeight,
        (panel_height - fixed_height - kLayerRowStride) / property_rows);
    const int row_height = std::clamp(
        property_budget, kMinPropertyRowHeight, kMaxPropertyRowHeight);

    const int remaining_for_layers =
        panel_height - fixed_height - row_height * property_rows;
    const int layer_capacity =
        std::max(1, remaining_for_layers / kLayerRowStride);
    const int visible_layers = std::min(total_layer_rows, layer_capacity);

    return RightPanelMetrics{
        row_height,
        visible_layers,
        kLayersHeaderHeight + visible_layers * kLayerRowStride + kSectionGap
    };
}

} // namespace acp::ui_layout
