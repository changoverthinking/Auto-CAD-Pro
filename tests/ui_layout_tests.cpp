#include "acp/ui_layout.hpp"

#include <algorithm>
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

    expect(m.toolbar_height >= 28 && m.toolbar_height <= 54,
           "toolbar stays inside compact bounded range");
    expect(m.right_panel_width >= 160 && m.right_panel_width <= 384,
           "right panel stays inside compact bounds");
    expect(m.left_rail_width >= 32 && m.left_rail_width <= 72,
           "left rail stays inside compact bounds");

    const int expected_toolbar = std::clamp(height / 20, 28, 54);
    const int expected_right = std::clamp(width / 5, 160, 384);
    const int expected_left = std::clamp(width / 25, 32, 72);
    expect(m.toolbar_height == expected_toolbar,
           "toolbar follows 1/20 workspace contract");
    expect(m.right_panel_width == expected_right,
           "right panel follows 2/10 workspace contract");
    expect(m.left_rail_width == expected_left,
           "left rail follows 1/25 workspace contract");

    expect(m.left_rail_width + m.right_panel_width < width,
           "workspace chrome leaves positive canvas width");
    expect(m.toolbar_height < height,
           "workspace chrome leaves positive canvas height");
}
}

int main() {
    verify(640, 480);
    verify(1024, 768);
    verify(1366, 768);
    verify(1920, 1080);
    verify(2560, 1440);
    verify(3840, 2160);

    expect(acp::ui_layout::compact_right_panel(160),
           "160px right panel uses compact layout");
    expect(!acp::ui_layout::compact_right_panel(204),
           "204px right panel uses full layout");
    expect(acp::ui_layout::property_key_width(160) == 67,
           "compact property key width remains readable");
    expect(acp::ui_layout::property_key_width(384) == 112,
           "wide property key width is capped");

    const auto compact_text =
        acp::ui_layout::right_panel_metrics(430, 16, 11);
    expect(compact_text.property_row_height >= 14 &&
               compact_text.property_row_height <= 22,
           "480p text property rows stay within readable bounds");
    expect(compact_text.visible_layer_rows >= 1,
           "480p reserves at least one visible layer row");
    expect(
        compact_text.properties_top_offset +
            26 + 4 +
            compact_text.property_row_height * 16 <= 430,
        "480p text properties stay inside panel");

    const auto tall_text =
        acp::ui_layout::right_panel_metrics(680, 16, 20);
    expect(tall_text.property_row_height == 22,
           "tall panel uses full property row height");
    expect(tall_text.visible_layer_rows >= 8,
           "tall panel exposes multiple layer rows");

    const auto tall_single_layer =
        acp::ui_layout::right_panel_metrics(680, 16, 1);
    expect(tall_single_layer.visible_layer_rows == 1,
           "single layer does not reserve empty layer rows");
    expect(tall_single_layer.properties_top_offset == 62,
           "single layer keeps properties directly below the row");

    const auto compact_empty =
        acp::ui_layout::right_panel_metrics(430, 11, 10);
    expect(compact_empty.property_row_height == 22,
           "480p empty selection keeps full row height");
    expect(compact_empty.visible_layer_rows >= 4,
           "480p empty selection leaves room for layers");

    const auto full_hd = acp::ui_layout::metrics_for_client(1920, 1080);
    expect(full_hd.toolbar_height == 54,
           "1080p toolbar respects 1/20 target and upper bound");
    expect(full_hd.right_panel_width == 384,
           "1080p right panel uses 20 percent target capped at 384px");
    expect(full_hd.left_rail_width == 72,
           "1080p left rail uses 4 percent target capped at 72px");

    if (failures != 0) {
        std::cerr << failures << " UI layout metric test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All UI layout metric tests passed\n";
    return EXIT_SUCCESS;
}
