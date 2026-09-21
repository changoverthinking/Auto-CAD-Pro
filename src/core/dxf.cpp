#include "acp/dxf.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace acp::dxf {

namespace {

struct Pair {
    int code{};
    std::string value;
};

std::string trim_cr(std::string value) {
    if (!value.empty() && value.back() == '\r') value.pop_back();
    return value;
}

bool parse_int(std::string_view text, int& value) {
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parse_double(std::string_view text, double& value) {
    std::string copy{text};
    char* end = nullptr;
    value = std::strtod(copy.c_str(), &end);
    return end != copy.c_str() && *end == '\0' && std::isfinite(value);
}

std::optional<std::vector<Pair>> parse_pairs(std::string_view data) {
    std::istringstream in{std::string(data)};
    std::vector<Pair> pairs;
    std::string code_line;
    std::string value_line;

    while (std::getline(in, code_line)) {
        if (!std::getline(in, value_line)) return std::nullopt;
        code_line = trim_cr(std::move(code_line));
        value_line = trim_cr(std::move(value_line));

        code_line.erase(0, code_line.find_first_not_of(" \t"));
        const auto last = code_line.find_last_not_of(" \t");
        if (last == std::string::npos) return std::nullopt;
        code_line.erase(last + 1);

        int code{};
        if (!parse_int(code_line, code)) return std::nullopt;
        pairs.push_back({code, std::move(value_line)});
    }

    return pairs;
}

void write_pair(std::ostream& out, int code, std::string_view value) {
    out << code << '\n' << value << '\n';
}

void write_pair(std::ostream& out, int code, double value) {
    out << code << '\n' << value << '\n';
}

void write_pair(std::ostream& out, int code, int value) {
    out << code << '\n' << value << '\n';
}

std::string layer_name_for(const Document& document, EntityId id) {
    const auto* props = document.properties(id);
    if (props == nullptr) return "0";
    const auto* layer = document.layer(props->layer_id);
    return layer == nullptr ? "0" : layer->name;
}

int true_color_value(RgbColor color) noexcept {
    return (static_cast<int>(color.r) << 16) |
           (static_cast<int>(color.g) << 8) |
           static_cast<int>(color.b);
}

std::optional<RgbColor> color_from_true_color(int value) noexcept {
    if (value < 0 || value > 0xFFFFFF) {
        return std::nullopt;
    }
    return RgbColor{
        static_cast<std::uint8_t>((value >> 16) & 0xFF),
        static_cast<std::uint8_t>((value >> 8) & 0xFF),
        static_cast<std::uint8_t>(value & 0xFF)};
}

const char* dxf_line_type_name(LineType line_type) noexcept {
    switch (line_type) {
        case LineType::Continuous: return "CONTINUOUS";
        case LineType::Dashed: return "DASHED";
        case LineType::Center: return "CENTER";
    }
    return "CONTINUOUS";
}

std::optional<LineType> line_type_from_dxf(std::string_view value) {
    if (value == "CONTINUOUS" || value == "BYLAYER" || value.empty()) {
        return LineType::Continuous;
    }
    if (value == "DASHED") return LineType::Dashed;
    if (value == "CENTER") return LineType::Center;
    return std::nullopt;
}

void write_entity_style(
    std::ostream& out,
    const Document& document,
    EntityId id) {

    write_pair(
        out, 420,
        true_color_value(document.effective_color(id)));
    write_pair(
        out, 6,
        dxf_line_type_name(document.effective_line_type(id)));
}

void write_ltype_table(std::ostream& out) {
    write_pair(out, 0, "SECTION");
    write_pair(out, 2, "TABLES");
    write_pair(out, 0, "TABLE");
    write_pair(out, 2, "LTYPE");
    write_pair(out, 70, 3);

    write_pair(out, 0, "LTYPE");
    write_pair(out, 2, "CONTINUOUS");
    write_pair(out, 70, 0);
    write_pair(out, 3, "Solid line");
    write_pair(out, 72, 65);
    write_pair(out, 73, 0);
    write_pair(out, 40, 0.0);

    write_pair(out, 0, "LTYPE");
    write_pair(out, 2, "DASHED");
    write_pair(out, 70, 0);
    write_pair(out, 3, "Dashed");
    write_pair(out, 72, 65);
    write_pair(out, 73, 2);
    write_pair(out, 40, 20.0);
    write_pair(out, 49, 12.0);
    write_pair(out, 74, 0);
    write_pair(out, 49, -8.0);
    write_pair(out, 74, 0);

    write_pair(out, 0, "LTYPE");
    write_pair(out, 2, "CENTER");
    write_pair(out, 70, 0);
    write_pair(out, 3, "Center");
    write_pair(out, 72, 65);
    write_pair(out, 73, 4);
    write_pair(out, 40, 33.0);
    write_pair(out, 49, 18.0);
    write_pair(out, 74, 0);
    write_pair(out, 49, -6.0);
    write_pair(out, 74, 0);
    write_pair(out, 49, 3.0);
    write_pair(out, 74, 0);
    write_pair(out, 49, -6.0);
    write_pair(out, 74, 0);

    write_pair(out, 0, "ENDTAB");
    write_pair(out, 0, "ENDSEC");
}

std::string sanitize_text(std::string text) {
    std::replace(text.begin(), text.end(), '\r', ' ');
    std::replace(text.begin(), text.end(), '\n', ' ');
    return text;
}

LayerId ensure_layer(Document& document, const std::string& name) {
    if (name.empty() || name == "0") return kDefaultLayerId;
    for (const LayerId id : document.layer_ids()) {
        const Layer* layer = document.layer(id);
        if (layer != nullptr && layer->name == name) return id;
    }
    return document.create_layer(name);
}

const Pair* find_first(const std::vector<Pair>& fields, int code) {
    for (const auto& field : fields) {
        if (field.code == code) return &field;
    }
    return nullptr;
}

bool get_double(const std::vector<Pair>& fields, int code, double& value) {
    const Pair* field = find_first(fields, code);
    return field != nullptr && parse_double(field->value, value);
}

bool get_int(const std::vector<Pair>& fields, int code, int& value) {
    const Pair* field = find_first(fields, code);
    return field != nullptr && parse_int(field->value, value);
}

std::string get_string(const std::vector<Pair>& fields, int code, std::string fallback = {}) {
    const Pair* field = find_first(fields, code);
    return field == nullptr ? std::move(fallback) : field->value;
}

bool import_entity(
    Document& document,
    std::string_view type,
    const std::vector<Pair>& fields,
    std::size_t& imported,
    std::size_t& skipped) {

    Entity entity;

    if (type == "LINE") {
        LineEntity line;
        if (!get_double(fields, 10, line.segment.a.x) ||
            !get_double(fields, 20, line.segment.a.y) ||
            !get_double(fields, 11, line.segment.b.x) ||
            !get_double(fields, 21, line.segment.b.y) ||
            geo::distance(line.segment.a, line.segment.b) <= geo::kEpsilon) {
            return false;
        }
        entity = line;
    } else if (type == "CIRCLE") {
        CircleEntity circle;
        if (!get_double(fields, 10, circle.circle.center.x) ||
            !get_double(fields, 20, circle.circle.center.y) ||
            !get_double(fields, 40, circle.circle.radius) ||
            circle.circle.radius <= geo::kEpsilon) {
            return false;
        }
        entity = circle;
    } else if (type == "ARC") {
        ArcEntity arc;
        double start_degrees{};
        double end_degrees{};
        if (!get_double(fields, 10, arc.arc.center.x) ||
            !get_double(fields, 20, arc.arc.center.y) ||
            !get_double(fields, 40, arc.arc.radius) ||
            !get_double(fields, 50, start_degrees) ||
            !get_double(fields, 51, end_degrees) ||
            arc.arc.radius <= geo::kEpsilon) {
            return false;
        }
        arc.arc.start_angle = start_degrees * std::numbers::pi / 180.0;
        arc.arc.end_angle = end_degrees * std::numbers::pi / 180.0;
        arc.arc.counter_clockwise = true;
        entity = arc;
    } else if (type == "LWPOLYLINE") {
        PolylineEntity polyline;
        int flags{};
        get_int(fields, 70, flags);
        polyline.closed = (flags & 1) != 0;

        std::optional<double> pending_x;
        for (const auto& field : fields) {
            if (field.code == 10) {
                double x{};
                if (!parse_double(field.value, x)) return false;
                pending_x = x;
            } else if (field.code == 20 && pending_x.has_value()) {
                double y{};
                if (!parse_double(field.value, y)) return false;
                polyline.points.push_back({*pending_x, y});
                pending_x.reset();
            }
        }

        if (pending_x.has_value() ||
            polyline.points.size() < 2 ||
            (polyline.closed && polyline.points.size() < 3)) {
            return false;
        }
        entity = std::move(polyline);
    } else if (type == "TEXT") {
        TextEntity text;
        double degrees{};
        if (!get_double(fields, 10, text.position.x) ||
            !get_double(fields, 20, text.position.y) ||
            !get_double(fields, 40, text.height) ||
            text.height <= geo::kEpsilon) {
            return false;
        }
        const Pair* rotation = find_first(fields, 50);
        if (rotation != nullptr && !parse_double(rotation->value, degrees)) return false;
        text.rotation = degrees * std::numbers::pi / 180.0;
        text.text = get_string(fields, 1);
        if (text.text.empty()) return false;
        entity = std::move(text);
    } else {
        ++skipped;
        return true;
    }

    const EntityId id = document.insert(std::move(entity));
    const std::string layer_name = get_string(fields, 8, "0");
    const LayerId layer_id = ensure_layer(document, layer_name);
    if (layer_id == 0 || !document.set_entity_layer(id, layer_id)) return false;

    EntityProperties* properties = document.properties(id);
    if (properties == nullptr) return false;

    if (const Pair* true_color = find_first(fields, 420)) {
        int value{};
        if (!parse_int(true_color->value, value)) return false;
        const auto color = color_from_true_color(value);
        if (!color.has_value()) return false;
        properties->color_override = *color;
    }

    if (const Pair* line_type = find_first(fields, 6)) {
        const auto parsed = line_type_from_dxf(line_type->value);
        if (!parsed.has_value()) return false;
        properties->line_type_override = *parsed;
    }

    ++imported;
    return true;
}

} // namespace

ExportResult export_ascii_report(const Document& document) {
    ExportResult result;
    std::ostringstream out;
    out << std::setprecision(17);

    write_ltype_table(out);
    write_pair(out, 0, "SECTION");
    write_pair(out, 2, "ENTITIES");

    for (const EntityId id : document.ids()) {
        const Entity* entity = document.find(id);
        if (entity == nullptr || !document.entity_visible(id)) continue;
        const std::string layer = layer_name_for(document, id);

        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, LineEntity>) {
                ++result.exported;
                write_pair(out, 0, "LINE");
                write_pair(out, 8, layer);
                write_entity_style(out, document, id);
                write_pair(out, 10, value.segment.a.x);
                write_pair(out, 20, value.segment.a.y);
                write_pair(out, 11, value.segment.b.x);
                write_pair(out, 21, value.segment.b.y);
            } else if constexpr (std::is_same_v<T, CircleEntity>) {
                ++result.exported;
                write_pair(out, 0, "CIRCLE");
                write_pair(out, 8, layer);
                write_entity_style(out, document, id);
                write_pair(out, 10, value.circle.center.x);
                write_pair(out, 20, value.circle.center.y);
                write_pair(out, 40, value.circle.radius);
            } else if constexpr (std::is_same_v<T, ArcEntity>) {
                ++result.exported;
                write_pair(out, 0, "ARC");
                write_pair(out, 8, layer);
                write_entity_style(out, document, id);
                write_pair(out, 10, value.arc.center.x);
                write_pair(out, 20, value.arc.center.y);
                write_pair(out, 40, value.arc.radius);
                const double start = value.arc.counter_clockwise ? value.arc.start_angle : value.arc.end_angle;
                const double end = value.arc.counter_clockwise ? value.arc.end_angle : value.arc.start_angle;
                write_pair(out, 50, start * 180.0 / std::numbers::pi);
                write_pair(out, 51, end * 180.0 / std::numbers::pi);
            } else if constexpr (std::is_same_v<T, PolylineEntity>) {
                if (value.points.size() < 2 ||
                    (value.closed && value.points.size() < 3)) {
                    ++result.skipped;
                    return;
                }
                ++result.exported;
                write_pair(out, 0, "LWPOLYLINE");
                write_pair(out, 8, layer);
                write_entity_style(out, document, id);
                write_pair(out, 90, static_cast<int>(value.points.size()));
                write_pair(out, 70, value.closed ? 1 : 0);
                for (const auto& point : value.points) {
                    write_pair(out, 10, point.x);
                    write_pair(out, 20, point.y);
                }
            } else if constexpr (std::is_same_v<T, TextEntity>) {
                if (!annotation::valid_text(value)) {
                    ++result.skipped;
                    return;
                }
                ++result.exported;
                write_pair(out, 0, "TEXT");
                write_pair(out, 8, layer);
                write_entity_style(out, document, id);
                write_pair(out, 10, value.position.x);
                write_pair(out, 20, value.position.y);
                write_pair(out, 40, value.height);
                write_pair(out, 1, sanitize_text(value.text));
                write_pair(out, 50, value.rotation * 180.0 / std::numbers::pi);
            } else {
                ++result.skipped;
            }
        }, *entity);
    }

    write_pair(out, 0, "ENDSEC");
    write_pair(out, 0, "EOF");
    result.data = out.str();
    return result;
}

std::string export_ascii(const Document& document) {
    return export_ascii_report(document).data;
}

std::optional<ImportResult> import_ascii(std::string_view data) {
    const auto parsed = parse_pairs(data);
    if (!parsed.has_value()) return std::nullopt;

    ImportResult result;
    const auto& pairs = *parsed;
    bool in_entities = false;
    bool saw_entities = false;

    std::size_t i = 0;
    while (i < pairs.size()) {
        if (pairs[i].code != 0) {
            ++i;
            continue;
        }

        const std::string& marker = pairs[i].value;
        if (marker == "SECTION") {
            if (i + 1 >= pairs.size() || pairs[i + 1].code != 2) return std::nullopt;
            in_entities = pairs[i + 1].value == "ENTITIES";
            if (in_entities) saw_entities = true;
            i += 2;
            continue;
        }

        if (marker == "ENDSEC") {
            in_entities = false;
            ++i;
            continue;
        }

        if (marker == "EOF") break;

        if (!in_entities) {
            ++i;
            continue;
        }

        const std::string type = marker;
        ++i;
        std::vector<Pair> fields;
        while (i < pairs.size() && pairs[i].code != 0) {
            fields.push_back(pairs[i]);
            ++i;
        }

        if (!import_entity(result.document, type, fields, result.imported, result.skipped)) {
            return std::nullopt;
        }
    }

    return saw_entities ? std::optional<ImportResult>{std::move(result)} : std::nullopt;
}

} // namespace acp::dxf
