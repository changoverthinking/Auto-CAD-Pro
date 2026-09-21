#include "acp/pdf.hpp"

#include "acp/annotation.hpp"
#include "acp/bounds.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace acp::pdf {
namespace {

constexpr double kPointsPerMm = 72.0 / 25.4;

bool pdf_text_supported(std::string_view text) noexcept {
    for (const unsigned char ch : text) {
        if (ch < 32 || ch > 126) {
            return false;
        }
    }
    return true;
}

bool entity_has_unsupported_text(const Entity& value) noexcept {
    return std::visit([](const auto& item) noexcept {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, TextEntity>) {
            return !pdf_text_supported(item.text);
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            return item.text_override.has_value() &&
                   !pdf_text_supported(*item.text_override);
        } else {
            return false;
        }
    }, value);
}

std::string pdf_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const unsigned char ch : text) {
        if (ch == '(' || ch == ')' || ch == '\\') out.push_back('\\');
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

struct Transform {
    double min_x{};
    double min_y{};
    double scale{};
    double offset_x{};
    double offset_y{};

    geo::Vec2 point(geo::Vec2 value) const noexcept {
        return {
            offset_x + (value.x - min_x) * scale,
            offset_y + (value.y - min_y) * scale
        };
    }
};

void segment(std::ostream& out, const Transform& tx, const geo::Segment& s) {
    const auto a = tx.point(s.a);
    const auto b = tx.point(s.b);
    out << a.x << ' ' << a.y << " m " << b.x << ' ' << b.y << " l S\n";
}

void polyline(
    std::ostream& out,
    const Transform& tx,
    const std::vector<geo::Vec2>& points,
    bool closed) {

    if (points.size() < 2) return;
    auto p = tx.point(points.front());
    out << p.x << ' ' << p.y << " m ";
    for (std::size_t i = 1; i < points.size(); ++i) {
        p = tx.point(points[i]);
        out << p.x << ' ' << p.y << " l ";
    }
    if (closed) out << "h ";
    out << "S\n";
}

void sampled_arc(
    std::ostream& out,
    const Transform& tx,
    const geo::Arc& arc,
    bool full_circle = false) {

    if (!std::isfinite(arc.radius) || arc.radius <= 0.0) return;
    double sweep = full_circle ? 2.0 * std::numbers::pi : geo::arc_sweep(arc);
    if (!std::isfinite(sweep) || sweep <= 0.0) return;
    constexpr int kSegmentsPerCircle = 96;
    const int count = std::max(
        4, static_cast<int>(std::ceil(
            kSegmentsPerCircle * sweep / (2.0 * std::numbers::pi))));
    std::vector<geo::Vec2> points;
    points.reserve(static_cast<std::size_t>(count + 1));
    const double direction = arc.counter_clockwise ? 1.0 : -1.0;
    for (int i = 0; i <= count; ++i) {
        const double t = static_cast<double>(i) / count;
        const double angle = arc.start_angle + direction * sweep * t;
        points.push_back({
            arc.center.x + std::cos(angle) * arc.radius,
            arc.center.y + std::sin(angle) * arc.radius
        });
    }
    polyline(out, tx, points, false);
}

void text(
    std::ostream& out,
    const Transform& tx,
    const TextEntity& value) {

    if (!annotation::valid_text(value) || !pdf_text_supported(value.text)) return;
    const auto p = tx.point(value.position);
    const double size = std::max(5.0, value.height * tx.scale);
    const double c = std::cos(value.rotation);
    const double s = std::sin(value.rotation);
    out << "BT /F1 " << size << " Tf "
        << c << ' ' << s << ' ' << -s << ' ' << c << ' '
        << p.x << ' ' << p.y << " Tm ("
        << pdf_escape(value.text) << ") Tj ET\n";
}

void primitive(
    std::ostream& out,
    const Transform& tx,
    const BlockPrimitive& value) {

    std::visit([&](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            segment(out, tx, item.segment);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            sampled_arc(out, tx, geo::Arc{
                item.circle.center, item.circle.radius, 0.0,
                2.0 * std::numbers::pi, true}, true);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            sampled_arc(out, tx, item.arc);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            polyline(out, tx, item.points, item.closed);
        }
    }, value);
}

void entity(
    std::ostream& out,
    const Transform& tx,
    const Entity& value,
    const BlockLibrary* blocks) {

    std::visit([&](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            segment(out, tx, item.segment);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            sampled_arc(out, tx, geo::Arc{
                item.circle.center, item.circle.radius, 0.0,
                2.0 * std::numbers::pi, true}, true);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            sampled_arc(out, tx, item.arc);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            polyline(out, tx, item.points, item.closed);
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            if (blocks != nullptr) {
                for (const auto& p : blocks->instantiate(item)) primitive(out, tx, p);
            }
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            text(out, tx, item);
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            if (!annotation::valid_linear_dimension(item)) return;
            segment(out, tx, annotation::first_extension_line(item));
            segment(out, tx, annotation::second_extension_line(item));
            const auto line = annotation::dimension_line(item);
            segment(out, tx, line);
            TextEntity label;
            label.position = geo::midpoint(line);
            label.height = 2.5;
            if (item.text_override.has_value()) {
                label.text = *item.text_override;
            } else {
                std::ostringstream value_text;
                value_text << std::fixed << std::setprecision(2)
                           << annotation::measurement(item);
                label.text = value_text.str();
            }
            text(out, tx, label);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            polyline(out, tx, item.boundary, true);
        }
    }, value);
}

std::string assemble_pdf(
    double page_width,
    double page_height,
    const std::string& stream) {

    std::vector<std::string> objects;
    objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
    objects.push_back("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");

    std::ostringstream page;
    page << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
         << page_width << ' ' << page_height
         << "] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>";
    objects.push_back(page.str());

    std::ostringstream content;
    content << "<< /Length " << stream.size() << " >>\nstream\n"
            << stream << "endstream";
    objects.push_back(content.str());
    objects.push_back("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

    std::ostringstream pdf;
    pdf << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<std::streamoff> offsets;
    offsets.push_back(0);
    for (std::size_t i = 0; i < objects.size(); ++i) {
        offsets.push_back(pdf.tellp());
        pdf << (i + 1) << " 0 obj\n" << objects[i] << "\nendobj\n";
    }
    const auto xref = pdf.tellp();
    pdf << "xref\n0 " << (objects.size() + 1) << "\n"
        << "0000000000 65535 f \n";
    for (std::size_t i = 1; i < offsets.size(); ++i) {
        pdf << std::setw(10) << std::setfill('0') << offsets[i]
            << " 00000 n \n";
    }
    pdf << "trailer\n<< /Size " << (objects.size() + 1)
        << " /Root 1 0 R >>\nstartxref\n"
        << xref << "\n%%EOF\n";
    return pdf.str();
}

} // namespace

std::optional<std::string> export_document(
    const Document& document,
    const BlockLibrary* blocks,
    const layout::PageSetup& page,
    std::optional<double> fixed_scale_denominator) {

    const auto drawing = bounds::drawing_bounds(document, blocks);
    const auto printable = layout::printable_size_mm(page);
    if (!drawing.has_value() || !printable.has_value()) return std::nullopt;

    const auto paper = layout::paper_size_mm(page.paper, page.orientation);
    if (paper.width <= 0.0 || paper.height <= 0.0) return std::nullopt;

    const double drawing_width = std::max(drawing->width(), 1e-9);
    const double drawing_height = std::max(drawing->height(), 1e-9);

    double scale_mm{};
    if (fixed_scale_denominator.has_value()) {
        const double denominator = *fixed_scale_denominator;
        if (!std::isfinite(denominator) || denominator <= 0.0) {
            return std::nullopt;
        }
        const auto viewport = layout::viewport_at_scale(
            drawing->center(), page, denominator);
        if (!viewport.has_value() ||
            drawing_width > viewport->world_width + geo::kEpsilon ||
            drawing_height > viewport->world_height + geo::kEpsilon) {
            return std::nullopt;
        }
        scale_mm = 1.0 / denominator;
    } else {
        scale_mm = std::min(
            printable->width / drawing_width,
            printable->height / drawing_height);
    }
    if (!std::isfinite(scale_mm) || scale_mm <= 0.0) return std::nullopt;

    const double content_width = drawing_width * scale_mm;
    const double content_height = drawing_height * scale_mm;
    const double left_mm = page.margins.left +
        (printable->width - content_width) * 0.5;
    const double bottom_mm = page.margins.bottom +
        (printable->height - content_height) * 0.5;

    Transform tx{
        drawing->min.x,
        drawing->min.y,
        scale_mm * kPointsPerMm,
        left_mm * kPointsPerMm,
        bottom_mm * kPointsPerMm
    };

    std::ostringstream content;
    content << std::setprecision(10)
            << "1 J 1 j\n";
    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) continue;
        const Entity* value = document.find(id);
        if (value == nullptr) continue;
        if (entity_has_unsupported_text(*value)) {
            return std::nullopt;
        }
        const double width_points = std::clamp(
            document.effective_line_weight(id), 0.05, 2.0) * kPointsPerMm;
        content << width_points << " w\n";
        entity(content, tx, *value, blocks);
    }

    return assemble_pdf(
        paper.width * kPointsPerMm,
        paper.height * kPointsPerMm,
        content.str());
}

} // namespace acp::pdf
