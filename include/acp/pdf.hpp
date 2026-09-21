#pragma once

#include "acp/block.hpp"
#include "acp/document.hpp"
#include "acp/layout.hpp"

#include <optional>
#include <string>

namespace acp::pdf {

[[nodiscard]] std::optional<std::string> export_document(
    const Document& document,
    const BlockLibrary* blocks = nullptr,
    const layout::PageSetup& page = {},
    std::optional<double> fixed_scale_denominator = std::nullopt);

} // namespace acp::pdf
