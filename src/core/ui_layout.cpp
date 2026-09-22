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

} // namespace acp::ui_layout
