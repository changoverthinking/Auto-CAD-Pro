#pragma once

#include "acp/block.hpp"
#include "acp/document.hpp"
#include "acp/layout.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace acp::pdf {

struct FontData {
    std::string_view bytes;
    std::string_view base_font_name{"NotoSansJP"};
};

[[nodiscard]] std::optional<std::string> export_document(
    const Document& document,
    const BlockLibrary* blocks = nullptr,
    const layout::PageSetup& page = {},
    std::optional<double> fixed_scale_denominator = std::nullopt,
    const FontData* font = nullptr);

} // namespace acp::pdf
