#pragma once

#include "acp/document.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace acp::dxf {

struct ImportResult {
    Document document;
    std::size_t imported{};
    std::size_t skipped{};
};

[[nodiscard]] std::string export_ascii(const Document& document);
[[nodiscard]] std::optional<ImportResult> import_ascii(std::string_view data);

} // namespace acp::dxf
