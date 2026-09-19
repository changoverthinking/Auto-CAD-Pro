#pragma once

#include "acp/block.hpp"
#include "acp/document.hpp"

#include <optional>

namespace acp::bounds {

struct Bounds2 {
    geo::Vec2 min{};
    geo::Vec2 max{};
    bool valid{false};

    void include(geo::Vec2 point) noexcept;
    void include(const Bounds2& other) noexcept;

    [[nodiscard]] double width() const noexcept;
    [[nodiscard]] double height() const noexcept;
    [[nodiscard]] geo::Vec2 center() const noexcept;
};

struct ViewportFit {
    geo::Vec2 center{};
    double world_width{};
    double world_height{};
};

[[nodiscard]] std::optional<Bounds2> entity_bounds(
    const Entity& entity,
    const BlockLibrary* blocks = nullptr) noexcept;

[[nodiscard]] std::optional<Bounds2> drawing_bounds(
    const Document& document,
    const BlockLibrary* blocks = nullptr) noexcept;

[[nodiscard]] std::optional<ViewportFit> fit_to_aspect(
    const Bounds2& bounds,
    double viewport_aspect,
    double margin_fraction = 0.05) noexcept;

} // namespace acp::bounds
