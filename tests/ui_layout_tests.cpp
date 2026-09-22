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

    expect(m.toolbar_height >= 28 && m.toolbar_height <= 34,
           "toolbar stays in compact 28-34px range");
    expect(m.right_panel_width >= 160 && m.right_panel_width <= 360,
           "right panel stays within compact bounds");
    expect(m.left_rail_width == 0,
           "classic workspace has no permanent left rail");

    expect(m.right_panel_width <= std::max(160, (width * 17) / 100 + 1),
           "right panel remains approximately 16 percent where unclamped");
}
}

int main() {
    verify(640, 480);
    verify(1024, 768);
    verify(1366, 768);
    verify(1920, 1080);
    verify(2560, 1440);
    verify(3840, 2160);

    expect(acp::ui_layout::compact_right_panel(128),
           "128px right panel uses compact layout");
    expect(!acp::ui_layout::compact_right_panel(204),
           "204px right panel uses full layout");
    expect(acp::ui_layout::property_key_width(128) == 57,
           "compact property key width remains readable");
    expect(acp::ui_layout::property_key_width(384) == 100,
           "wide property key width is capped");

    const auto compact_text =
        acp::ui_layout::right_panel_metrics(430, 16, 11);
    expect(compact_text.property_row_height == 20,
           "480p text property rows compact to fit");
    expect(compact_text.visible_layer_rows == 1,
           "480p reserves at least one visible layer row");
    expect(
        compact_text.properties_top_offset +
            28 + 6 +
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
    expect(tall_single_layer.properties_top_offset == 68,
           "single layer keeps properties directly below the row");

    const auto compact_empty =
        acp::ui_layout::right_panel_metrics(430, 11, 10);
    expect(compact_empty.property_row_height == 22,
           "480p empty selection keeps full row height");
    expect(compact_empty.visible_layer_rows >= 4,
           "480p empty selection leaves room for layers");

    const auto full_hd = acp::ui_layout::metrics_for_client(1920, 1080);
    expect(full_hd.toolbar_height == 33,
           "1080p toolbar remains compact");
    expect(full_hd.right_panel_width == 307,
           "1080p right panel uses approximately 16 percent");
    expect(full_hd.left_rail_width == 0,
           "1080p canvas starts at the client left edge");

    if (failures != 0) {
        std::cerr << failures << " UI layout metric test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All UI layout metric tests passed\n";
    return EXIT_SUCCESS;
}
