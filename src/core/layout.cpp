#include "acp/layout.hpp"

#include <algorithm>
#include <cmath>

namespace acp::layout {

namespace {

SizeMm portrait_size(PaperSize paper) noexcept {
    switch (paper) {
        case PaperSize::A4: return {210.0, 297.0};
        case PaperSize::A3: return {297.0, 420.0};
        case PaperSize::A2: return {420.0, 594.0};
        case PaperSize::A1: return {594.0, 841.0};
        case PaperSize::A0: return {841.0, 1189.0};
    }
    return {};
}

bool finite_nonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

} // namespace

SizeMm paper_size_mm(PaperSize paper, Orientation orientation) noexcept {
    SizeMm size = portrait_size(paper);
    if (orientation == Orientation::Landscape) {
        std::swap(size.width, size.height);
    }
    return size;
}

std::optional<SizeMm> printable_size_mm(const PageSetup& page) noexcept {
    if (!finite_nonnegative(page.margins.left) ||
        !finite_nonnegative(page.margins.right) ||
        !finite_nonnegative(page.margins.top) ||
        !finite_nonnegative(page.margins.bottom)) {
        return std::nullopt;
    }

    const SizeMm paper = paper_size_mm(page.paper, page.orientation);
    const double width = paper.width - page.margins.left - page.margins.right;
    const double height = paper.height - page.margins.top - page.margins.bottom;

    if (width <= geo::kEpsilon || height <= geo::kEpsilon) {
        return std::nullopt;
    }

    return SizeMm{width, height};
}

std::optional<PrintViewport> fit_to_page(
    const bounds::Bounds2& drawing,
    const PageSetup& page,
    double extra_margin_fraction) noexcept {

    if (!drawing.valid ||
        !std::isfinite(extra_margin_fraction) ||
        extra_margin_fraction < 0.0) {
        return std::nullopt;
    }

    const auto printable = printable_size_mm(page);
    if (!printable.has_value()) {
        return std::nullopt;
    }

    const double margin_scale = 1.0 + 2.0 * extra_margin_fraction;
    const double drawing_width = std::max(drawing.width(), geo::kEpsilon) * margin_scale;
    const double drawing_height = std::max(drawing.height(), geo::kEpsilon) * margin_scale;

    const double scale_denominator = std::max(
        drawing_width / printable->width,
        drawing_height / printable->height);

    if (!std::isfinite(scale_denominator) || scale_denominator <= 0.0) {
        return std::nullopt;
    }

    return PrintViewport{
        drawing.center(),
        scale_denominator,
        printable->width * scale_denominator,
        printable->height * scale_denominator,
        *printable
    };
}

std::optional<PrintViewport> viewport_at_scale(
    geo::Vec2 center,
    const PageSetup& page,
    double scale_denominator) noexcept {

    if (!std::isfinite(center.x) ||
        !std::isfinite(center.y) ||
        !std::isfinite(scale_denominator) ||
        scale_denominator <= 0.0) {
        return std::nullopt;
    }

    const auto printable = printable_size_mm(page);
    if (!printable.has_value()) {
        return std::nullopt;
    }

    return PrintViewport{
        center,
        scale_denominator,
        printable->width * scale_denominator,
        printable->height * scale_denominator,
        *printable
    };
}

} // namespace acp::layout
