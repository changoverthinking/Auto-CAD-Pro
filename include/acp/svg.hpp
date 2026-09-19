#pragma once

#include "acp/block.hpp"
#include "acp/document.hpp"

#include <optional>
#include <string>

namespace acp::svg {

[[nodiscard]] std::optional<std::string> export_document(
    const Document& document,
    const BlockLibrary* blocks = nullptr,
    double margin = 10.0);

} // namespace acp::svg
