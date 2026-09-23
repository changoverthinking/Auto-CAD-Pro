#pragma once

#include "acp/document.hpp"

#include <cstdint>

namespace acp::model3d {

struct SceneRevisionCache {
    DocumentRevision built_document_revision{0};
    std::uint64_t built_settings_revision{0};
    bool initialized{false};

    [[nodiscard]] bool stale(
        const Document& document,
        std::uint64_t settings_revision) const noexcept {
        return !initialized ||
               built_document_revision != document.revision() ||
               built_settings_revision != settings_revision;
    }

    void mark_built(
        const Document& document,
        std::uint64_t settings_revision) noexcept {
        built_document_revision = document.revision();
        built_settings_revision = settings_revision;
        initialized = true;
    }

    void invalidate() noexcept {
        initialized = false;
    }
};

} // namespace acp::model3d
