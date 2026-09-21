#include "acp/pdf.hpp"

#include "acp/annotation.hpp"
#include "acp/bounds.hpp"
#include "acp/hatch.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace acp::pdf {
namespace {

constexpr double kPointsPerMm = 72.0 / 25.4;

bool ascii_pdf_text_supported(std::string_view text) noexcept {
    for (const unsigned char ch : text) {
        if (ch < 32 || ch > 126) return false;
    }
    return true;
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

std::optional<std::vector<std::uint32_t>> decode_utf8(std::string_view text) {
    std::vector<std::uint32_t> out;
    out.reserve(text.size());

    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::uint32_t cp{};
        std::size_t count{};

        if (lead <= 0x7F) {
            cp = lead;
            count = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            cp = lead & 0x1F;
            count = 2;
            if (cp == 0) return std::nullopt;
        } else if ((lead & 0xF0) == 0xE0) {
            cp = lead & 0x0F;
            count = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            cp = lead & 0x07;
            count = 4;
        } else {
            return std::nullopt;
        }

        if (i + count > text.size()) return std::nullopt;
        for (std::size_t j = 1; j < count; ++j) {
            const auto ch = static_cast<unsigned char>(text[i + j]);
            if ((ch & 0xC0) != 0x80) return std::nullopt;
            cp = (cp << 6) | (ch & 0x3F);
        }

        if ((count == 2 && cp < 0x80) ||
            (count == 3 && cp < 0x800) ||
            (count == 4 && cp < 0x10000) ||
            cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF)) {
            return std::nullopt;
        }

        out.push_back(cp);
        i += count;
    }
    return out;
}

std::string hex4(std::uint32_t value) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0') << std::setw(4)
        << (value & 0xFFFFu);
    return out.str();
}

std::string utf16be_hex(std::uint32_t cp) {
    if (cp <= 0xFFFFu) return hex4(cp);
    cp -= 0x10000u;
    const std::uint32_t high = 0xD800u + (cp >> 10);
    const std::uint32_t low = 0xDC00u + (cp & 0x3FFu);
    return hex4(high) + hex4(low);
}

std::string sanitize_pdf_name(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char ch : value) {
        const bool ok =
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_';
        out.push_back(ok ? static_cast<char>(ch) : '-');
    }
    if (out.empty()) out = "AutoCADProUnicode";
    return out;
}

struct FontPlan {
    std::string_view bytes;
    std::string base_name;
    stbtt_fontinfo info{};
    float em_scale{};
    std::map<int, std::uint32_t> glyph_to_unicode;
    std::map<int, int> glyph_widths;

    bool initialize(const FontData& font) {
        if (font.bytes.empty()) return false;
        const auto* data =
            reinterpret_cast<const unsigned char*>(font.bytes.data());
        const int offset = stbtt_GetFontOffsetForIndex(data, 0);
        if (offset < 0 || stbtt_InitFont(&info, data, offset) == 0) {
            return false;
        }
        bytes = font.bytes;
        base_name = sanitize_pdf_name(font.base_font_name);
        em_scale = stbtt_ScaleForMappingEmToPixels(&info, 1000.0f);
        return std::isfinite(em_scale) && em_scale > 0.0f;
    }

    std::optional<int> glyph_for(std::uint32_t cp) {
        if (cp > 0x10FFFFu) return std::nullopt;
        const int glyph = stbtt_FindGlyphIndex(&info, static_cast<int>(cp));
        if (glyph == 0 && cp != 0) return std::nullopt;

        int advance{};
        int lsb{};
        stbtt_GetGlyphHMetrics(&info, glyph, &advance, &lsb);
        const int width = std::max(
            0, static_cast<int>(std::lround(
                static_cast<double>(advance) * em_scale)));

        glyph_to_unicode.try_emplace(glyph, cp);
        glyph_widths.try_emplace(glyph, width);
        return glyph;
    }

    std::optional<std::string> encode(std::string_view utf8) {
        const auto cps = decode_utf8(utf8);
        if (!cps.has_value()) return std::nullopt;

        std::string hex;
        hex.reserve(cps->size() * 4);
        for (const std::uint32_t cp : *cps) {
            const auto glyph = glyph_for(cp);
            if (!glyph.has_value() || *glyph > 0xFFFF) {
                return std::nullopt;
            }
            hex += hex4(static_cast<std::uint32_t>(*glyph));
        }
        return hex;
    }
};

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

void filled_polygon(
    std::ostream& out,
    const Transform& tx,
    const std::vector<geo::Vec2>& points) {

    if (points.size() < 3) return;
    auto p = tx.point(points.front());
    out << p.x << ' ' << p.y << " m ";
    for (std::size_t i = 1; i < points.size(); ++i) {
        p = tx.point(points[i]);
        out << p.x << ' ' << p.y << " l ";
    }
    out << "h B\n";
}

void sampled_arc(
    std::ostream& out,
    const Transform& tx,
    const geo::Arc& arc,
    bool full_circle = false) {

    if (!std::isfinite(arc.radius) || arc.radius <= 0.0) return;
    const double sweep =
        full_circle ? 2.0 * std::numbers::pi : geo::arc_sweep(arc);
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

bool text(
    std::ostream& out,
    const Transform& tx,
    const TextEntity& value,
    FontPlan* font) {

    if (!annotation::valid_text(value)) return false;

    const auto p = tx.point(value.position);
    const double size = std::max(5.0, value.height * tx.scale);
    const double c = std::cos(value.rotation);
    const double s = std::sin(value.rotation);

    out << "BT /F1 " << size << " Tf "
        << c << ' ' << s << ' ' << -s << ' ' << c << ' '
        << p.x << ' ' << p.y << " Tm ";

    if (font != nullptr) {
        const auto encoded = font->encode(value.text);
        if (!encoded.has_value()) return false;
        out << '<' << *encoded << "> Tj ET\n";
        return true;
    }

    if (!ascii_pdf_text_supported(value.text)) return false;
    out << '(' << pdf_escape(value.text) << ") Tj ET\n";
    return true;
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

bool entity(
    std::ostream& out,
    const Transform& tx,
    const Entity& value,
    const BlockLibrary* blocks,
    FontPlan* font) {

    return std::visit([&](const auto& item) -> bool {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, LineEntity>) {
            segment(out, tx, item.segment);
            return true;
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            sampled_arc(out, tx, geo::Arc{
                item.circle.center, item.circle.radius, 0.0,
                2.0 * std::numbers::pi, true}, true);
            return true;
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            sampled_arc(out, tx, item.arc);
            return true;
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            polyline(out, tx, item.points, item.closed);
            return true;
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            if (blocks != nullptr) {
                for (const auto& p : blocks->instantiate(item)) {
                    primitive(out, tx, p);
                }
            }
            return true;
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            return text(out, tx, item, font);
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            if (!annotation::valid_linear_dimension(item)) return false;
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
            return text(out, tx, label, font);
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            if (!hatch::valid(item)) return false;
            if (item.solid) {
                filled_polygon(out, tx, item.boundary);
            } else {
                polyline(out, tx, item.boundary, true);
                for (const auto& hatch_segment : hatch::pattern_segments(item)) {
                    segment(out, tx, hatch_segment);
                }
            }
            return true;
        }
        return false;
    }, value);
}

std::string make_to_unicode_cmap(const FontPlan& font) {
    std::ostringstream out;
    out << "/CIDInit /ProcSet findresource begin\n"
        << "12 dict begin\n"
        << "begincmap\n"
        << "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
        << "/CMapName /AutoCADProUnicode def\n"
        << "/CMapType 2 def\n"
        << "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n";

    auto it = font.glyph_to_unicode.begin();
    while (it != font.glyph_to_unicode.end()) {
        const auto remaining =
            static_cast<std::size_t>(std::distance(it, font.glyph_to_unicode.end()));
        const std::size_t count = std::min<std::size_t>(100, remaining);
        out << count << " beginbfchar\n";
        for (std::size_t i = 0; i < count; ++i, ++it) {
            out << '<' << hex4(static_cast<std::uint32_t>(it->first)) << "> <"
                << utf16be_hex(it->second) << ">\n";
        }
        out << "endbfchar\n";
    }

    out << "endcmap\n"
        << "CMapName currentdict /CMap defineresource pop\n"
        << "end\nend\n";
    return out.str();
}

std::string stream_object(std::string_view bytes, bool include_length1 = false) {
    std::ostringstream head;
    head << "<< /Length " << bytes.size();
    if (include_length1) head << " /Length1 " << bytes.size();
    head << " >>\nstream\n";

    std::string out = head.str();
    out.append(bytes.data(), bytes.size());
    out += "\nendstream";
    return out;
}

std::string assemble_pdf(
    double page_width,
    double page_height,
    const std::string& content_stream,
    const FontPlan* font) {

    std::vector<std::string> objects;
    objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
    objects.push_back("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");

    std::ostringstream page;
    page << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
         << page_width << ' ' << page_height
         << "] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>";
    objects.push_back(page.str());
    objects.push_back(stream_object(content_stream));

    if (font == nullptr) {
        objects.push_back(
            "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    } else {
        objects.push_back(
            "<< /Type /Font /Subtype /Type0 /BaseFont /" + font->base_name +
            " /Encoding /Identity-H /DescendantFonts [6 0 R] /ToUnicode 9 0 R >>");

        std::ostringstream descendant;
        descendant
            << "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /"
            << font->base_name
            << " /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >>"
            << " /FontDescriptor 7 0 R /CIDToGIDMap /Identity /DW 1000";

        if (!font->glyph_widths.empty()) {
            descendant << " /W [";
            for (const auto& [glyph, width] : font->glyph_widths) {
                descendant << glyph << " [" << width << "] ";
            }
            descendant << ']';
        }
        descendant << " >>";
        objects.push_back(descendant.str());

        int ascent{};
        int descent{};
        int line_gap{};
        stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
        int x0{}, y0{}, x1{}, y1{};
        stbtt_GetFontBoundingBox(&font->info, &x0, &y0, &x1, &y1);

        const auto metric = [&](int value) {
            return static_cast<int>(std::lround(
                static_cast<double>(value) * font->em_scale));
        };

        std::ostringstream descriptor;
        descriptor
            << "<< /Type /FontDescriptor /FontName /" << font->base_name
            << " /Flags 32 /FontBBox ["
            << metric(x0) << ' ' << metric(y0) << ' '
            << metric(x1) << ' ' << metric(y1) << "]"
            << " /ItalicAngle 0 /Ascent " << metric(ascent)
            << " /Descent " << metric(descent)
            << " /CapHeight " << metric(ascent)
            << " /StemV 80 /FontFile2 8 0 R >>";
        objects.push_back(descriptor.str());
        objects.push_back(stream_object(font->bytes, true));

        const std::string cmap = make_to_unicode_cmap(*font);
        objects.push_back(stream_object(cmap));
    }

    std::ostringstream pdf;
    pdf << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<std::streamoff> offsets;
    offsets.push_back(0);

    for (std::size_t i = 0; i < objects.size(); ++i) {
        offsets.push_back(pdf.tellp());
        pdf << (i + 1) << " 0 obj\n";
        const std::string& object = objects[i];
        pdf.write(object.data(), static_cast<std::streamsize>(object.size()));
        pdf << "\nendobj\n";
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
    std::optional<double> fixed_scale_denominator,
    const FontData* font_data) {

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

    std::optional<FontPlan> font;
    if (font_data != nullptr) {
        font.emplace();
        if (!font->initialize(*font_data)) return std::nullopt;
    }

    std::ostringstream content;
    content << std::setprecision(10) << "1 J 1 j\n";
    for (const EntityId id : document.ids()) {
        if (!document.entity_visible(id)) continue;
        const Entity* value = document.find(id);
        if (value == nullptr) continue;

        const double width_points = std::clamp(
            document.effective_line_weight(id), 0.05, 2.0) * kPointsPerMm;
        content << width_points << " w\n";
        if (!entity(content, tx, *value, blocks,
                    font.has_value() ? &*font : nullptr)) {
            return std::nullopt;
        }
    }

    return assemble_pdf(
        paper.width * kPointsPerMm,
        paper.height * kPointsPerMm,
        content.str(),
        font.has_value() ? &*font : nullptr);
}

} // namespace acp::pdf
