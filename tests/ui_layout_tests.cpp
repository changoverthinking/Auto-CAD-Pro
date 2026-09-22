#include "acp/ui_layout.hpp"

#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* name) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

void verify(int width, int height) {
    const auto m = acp::ui_layout::metrics_for_client(width, height);

    expect(m.toolbar_height >= 26 && m.toolbar_height <= 32,
           "toolbar remains compact");
    expect(m.right_panel_width >= 1 && m.right_panel_width <= width / 3,
           "right dock preserves drawing area");
    expect(m.left_rail_width == 0,
           "left rail is collapsed by default");
}
}

int main() {
    verify(640, 480);
    verify(1024, 768);
    verify(1366, 768);
    verify(1680, 925);
    verify(1920, 1080);
    verify(2560, 1440);
    verify(3840, 2160);

    const auto sigma =
        acp::ui_layout::metrics_for_client(1680, 925);
    expect(sigma.toolbar_height == 28,
           "reference-size toolbar is 28px");
    expect(sigma.right_panel_width == 285,
           "reference-size right dock is 17 percent");
    expect(sigma.left_rail_width == 0,
           "reference-size has no permanent left rail");

    const auto full_hd =
        acp::ui_layout::metrics_for_client(1920, 1080);
    expect(full_hd.toolbar_height == 32,
           "1080p toolbar capped at 32px");
    expect(full_hd.right_panel_width == 300,
           "1080p dock capped at 300px");

    expect(acp::ui_layout::compact_right_panel(180),
           "narrow dock uses compact mode");
    expect(!acp::ui_layout::compact_right_panel(285),
           "reference dock uses full mode");

    const auto panel =
        acp::ui_layout::right_panel_metrics(285, 850, 16, 20);
    expect(panel.property_row_height == 20,
           "properties use compact 20px rows");
    expect(panel.visible_layer_rows >= 20,
           "navigator can display many layers vertically");
    expect(panel.navigator_width >= 110 && panel.navigator_width <= 132,
           "navigator uses narrow reference-like column");

    const auto compact =
        acp::ui_layout::right_panel_metrics(160, 430, 16, 20);
    expect(compact.property_row_height >= 14,
           "small-window properties remain readable");
    expect(compact.visible_layer_rows >= 1,
           "small-window navigator remains usable");
    expect(compact.navigator_width < 100,
           "small-window navigator stays narrow");

    if (failures != 0) {
        std::cerr << failures << " UI layout metric test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All UI layout metric tests passed\n";
    return EXIT_SUCCESS;
}
