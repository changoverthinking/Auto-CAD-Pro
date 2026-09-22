#include "acp/ui_layout.hpp"

#include <algorithm>

namespace acp::ui_layout {

Metrics metrics_for_client(int width, int height) noexcept {
    width = std::max(width, 1);
    height = std::max(height, 1);

    return Metrics{
        std::max(1, height / 20),
        std::max(1, (width * 2) / 10),
        std::max(1, width / 25)
    };
}

bool compact_right_panel(int panel_width) noexcept {
    return panel_width < 170;
}

int property_key_width(int panel_width) noexcept {
    panel_width = std::max(panel_width, 1);
    return std::clamp((panel_width * 45) / 100, 48, 100);
}

RightPanelMetrics right_panel_metrics(
    int panel_height,
    int property_rows) noexcept {

    panel_height = std::max(panel_height, 1);
    property_rows = std::max(property_rows, 1);

    constexpr int kLayersHeaderHeight = 32;
    constexpr int kLayerRowStride = 26;
    constexpr int kSectionGap = 10;
    constexpr int kPropertiesHeaderHeight = 28;
    constexpr int kPropertiesGap = 6;
    constexpr int kMinPropertyRowHeight = 14;
    constexpr int kMaxPropertyRowHeight = 22;

    const int fixed_height =
        kLayersHeaderHeight +
        kSectionGap +
        kPropertiesHeaderHeight +
        kPropertiesGap;

    const int property_budget =
        std::max(
            kMinPropertyRowHeight,
            (panel_height - fixed_height - kLayerRowStride) /
                property_rows);
    const int row_height =
        std::clamp(
            property_budget,
            kMinPropertyRowHeight,
            kMaxPropertyRowHeight);

    const int remaining_for_layers =
        panel_height - fixed_height - row_height * property_rows;
    const int visible_layers =
        std::max(1, remaining_for_layers / kLayerRowStride);

    return RightPanelMetrics{
        row_height,
        visible_layers,
        kLayersHeaderHeight + visible_layers * kLayerRowStride +
            kSectionGap
    };
}

} // namespace acp::ui_layout
