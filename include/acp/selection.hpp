#pragma once

#include "acp/document.hpp"

#include <optional>

namespace acp {
class BlockLibrary;
}

namespace acp::selection {

struct Hit {
    EntityId id{};
    double distance{};
    geo::Vec2 nearest{};
};

[[nodiscard]] double distance_to_entity(const Entity& entity, geo::Vec2 point) noexcept;

[[nodiscard]] std::optional<Hit> hit_test(
    const Document& document,
    geo::Vec2 point,
    double aperture) noexcept;

[[nodiscard]] std::optional<Hit> hit_test(
    const Document& document,
    const BlockLibrary& blocks,
    geo::Vec2 point,
    double aperture) noexcept;

} // namespace acp::selection
