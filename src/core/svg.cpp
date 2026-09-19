#include "acp/svg.hpp"

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

namespace acp::svg {

namespace {

std::string xml_escape(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char ch : text) {
        switch (ch) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result.push_back(ch); break;
        }
    }
    return result;
}

double sy(double y) noexcept {
    return -y;
}

void write_segment(
    std::ostream& out,
    const geo::Segment& segment,
    double stroke_width) {

    out << "<line x1=\"" << segment.a.x
        << "\" y1=\"" << sy(segment.a.y)
        << "\" x2=\"" << segment.b.x
        << "\" y2=\"" << sy(segment.b.y)
        << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\""
        << stroke_width << "\" />\n";
}

void write_circle(
    std::ostream& out,
    const CircleEntity& value,
    double stroke_width) {

    out << "<circle cx=\"" << value.circle.center.x
        << "\" cy=\"" << sy(value.circle.center.y)
        << "\" r=\"" << value.circle.radius
        << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\""
        << stroke_width << "\" />\n";
}

void write_arc(
    std::ostream& out,
    const ArcEntity& value,
    double stroke_width) {

    if (!geo::valid_arc(value.arc)) {
        return;
    }

    const auto start = geo::arc_start_point(value.arc);
    const auto end = geo::arc_end_point(value.arc);
    const int large_arc = geo::arc_sweep(value.arc) > std::numbers::pi ? 1 : 0;
    const int sweep = value.arc.counter_clockwise ? 0 : 1;

    out << "<path d=\"M " << start.x << ' ' << sy(start.y)
        << " A " << value.arc.radius << ' ' << value.arc.radius
        << " 0 " << large_arc << ' ' << sweep << ' '
        << end.x << ' ' << sy(end.y)
        << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\""
        << stroke_width << "\" />\n";
}

void write_polyline(
    std::ostream& out,
    const PolylineEntity& value,
    double stroke_width) {

    if (value.points.empty()) {
        return;
    }

    out << (value.closed ? "<polygon points=\"" : "<polyline points=\"");
    for (std::size_t i = 0; i < value.points.size(); ++i) {
        if (i != 0) out << ' ';
        out << value.points[i].x << ',' << sy(value.points[i].y);
    }
    out << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\""
        << stroke_width << "\" />\n";
}

void write_text(
    std::ostream& out,
    const TextEntity& value) {

    if (!annotation::valid_text(value)) {
        return;
    }

    const double angle_degrees =
        -value.rotation * 180.0 / std::numbers::pi;
    out << "<text x=\"" << value.position.x
        << "\" y=\"" << sy(value.position.y)
        << "\" font-size=\"" << value.height
        << "\" fill=\"currentColor\"";
    if (std::abs(angle_degrees) > geo::kEpsilon) {
        out << " transform=\"rotate(" << angle_degrees << ' '
            << value.position.x << ' ' << sy(value.position.y) << ")\"";
    }
    out << '>' << xml_escape(value.text) << "</text>\n";
}

void write_dimension(
    std::ostream& out,
    const LinearDimensionEntity& value,
    double stroke_width) {

    if (!annotation::valid_linear_dimension(value)) {
        return;
    }

    write_segment(out, annotation::first_extension_line(value), stroke_width);
    write_segment(out, annotation::second_extension_line(value), stroke_width);
    const auto dimension = annotation::dimension_line(value);
    write_segment(out, dimension, stroke_width);

    const geo::Vec2 label_point = geo::midpoint(dimension);
    std::ostringstream label;
    if (value.text_override.has_value()) {
        label << *value.text_override;
    } else {
        label << std::fixed << std::setprecision(2)
              << annotation::measurement(value);
    }

    out << "<text x=\"" << label_point.x
        << "\" y=\"" << sy(label_point.y)
        << "\" font-size=\"2.5\" fill=\"currentColor\">"
        << xml_escape(label.str())
        << "</text>\n";
}

void write_hatch(
    std::ostream& out,
    const HatchEntity& value,
    double stroke_width) {

    if (value.boundary.size() < 3) {
        return;
    }

    out << "<polygon points=\"";
    for (std::size_t i = 0; i < value.boundary.size(); ++i) {
        if (i != 0) out << ' ';
        out << value.boundary[i].x << ',' << sy(value.boundary[i].y);
    }
    out << "\" stroke=\"currentColor\" stroke-width=\""
        << stroke_width << "\" fill=\""
        << (value.solid ? "currentColor" : "none")
        << "\"";
    if (value.solid) {
        out << " fill-opacity=\"0.18\"";
    }
    out << " />\n";
}

void write_primitive(
    std::ostream& out,
    const BlockPrimitive& primitive,
    double stroke_width) {

    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            write_segment(out, value.segment, stroke_width);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            write_circle(out, value, stroke_width);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            write_arc(out, value, stroke_width);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            write_polyline(out, value, stroke_width);
        }
    }, primitive);
}

void write_entity(
    std::ostream& out,
    const Entity& entity,
    const BlockLibrary* blocks,
    double stroke_width) {

    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            write_segment(out, value.segment, stroke_width);
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            write_circle(out, value, stroke_width);
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            write_arc(out, value, stroke_width);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            write_polyline(out, value, stroke_width);
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            if (blocks != nullptr) {
                for (const auto& primitive : blocks->instantiate(value)) {
                    write_primitive(out, primitive, stroke_width);
                }
            }
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            write_text(out, value);
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            write_dimension(out, value, stroke_width);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            write_hatch(out, value, stroke_width);
        }
    }, entity);
}

} // namespace

std::optional<std::string> export_document(
    const Document& document,
    const BlockLibrary* blocks,
    double margin) {

    if (!std::isfinite(margin) || margin < 0.0) {
        return std::nullopt;
    }

    const auto drawing = bounds::drawing_bounds(document, blocks);
    if (!drawing.has_value()) {
        return std::nullopt;
    }

    const double min_x = drawing->min.x - margin;
    const double max_y = drawing->max.y + margin;
    const double width = std::max(drawing->width() + margin * 2.0, 1.0);
    const double height = std::max(drawing->height() + margin * 2.0, 1.0);

    std::ostringstream out;
    out << std::setprecision(12);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\""
        << min_x << ' ' << sy(max_y) << ' ' << width << ' ' << height
        << "\" color=\"#111111\">\n";

    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) {
            continue;
        }
        const Entity* entity = document.find(id);
        if (entity == nullptr) {
            continue;
        }
        const double stroke_width = std::clamp(
            document.effective_line_weight(id), 0.05, 2.0);
        write_entity(out, *entity, blocks, stroke_width);
    }

    out << "</svg>\n";
    return out.str();
}

} // namespace acp::svg
