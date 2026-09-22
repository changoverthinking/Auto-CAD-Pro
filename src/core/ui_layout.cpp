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

} // namespace acp::ui_layout
