#pragma once

#include "acp/bounds.hpp"

#include <optional>

namespace acp::layout {

enum class PaperSize {
    A4,
    A3,
    A2,
    A1,
    A0
};

enum class Orientation {
    Portrait,
    Landscape
};

struct MarginsMm {
    double left{10.0};
    double right{10.0};
    double top{10.0};
    double bottom{10.0};
};

struct PageSetup {
    PaperSize paper{PaperSize::A4};
    Orientation orientation{Orientation::Landscape};
    MarginsMm margins{};
};

struct SizeMm {
    double width{};
    double height{};
};

struct PrintViewport {
    geo::Vec2 center{};
    double scale_denominator{1.0};
    double world_width{};
    double world_height{};
    SizeMm printable_mm{};
};

[[nodiscard]] SizeMm paper_size_mm(PaperSize paper, Orientation orientation) noexcept;
[[nodiscard]] std::optional<SizeMm> printable_size_mm(const PageSetup& page) noexcept;

[[nodiscard]] std::optional<PrintViewport> fit_to_page(
    const bounds::Bounds2& drawing,
    const PageSetup& page,
    double extra_margin_fraction = 0.0) noexcept;

[[nodiscard]] std::optional<PrintViewport> viewport_at_scale(
    geo::Vec2 center,
    const PageSetup& page,
    double scale_denominator) noexcept;

} // namespace acp::layout
