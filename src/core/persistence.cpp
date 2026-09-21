#include "acp/persistence.hpp"

#include <iomanip>
#include <sstream>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace acp::persistence {

namespace {

void write_points(std::ostream& out, const std::vector<geo::Vec2>& points) {
    out << points.size();
    for (const auto point : points) {
        out << ' ' << point.x << ' ' << point.y;
    }
}

bool read_points(std::istream& in, std::vector<geo::Vec2>& points) {
    std::size_t count{};
    if (!(in >> count)) {
        return false;
    }

    points.clear();
    points.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        geo::Vec2 point;
        if (!(in >> point.x >> point.y)) {
            return false;
        }
        points.push_back(point);
    }
    return true;
}

void write_primitive(std::ostream& out, const BlockPrimitive& primitive) {
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, LineEntity>) {
            out << "P LINE "
                << value.segment.a.x << ' ' << value.segment.a.y << ' '
                << value.segment.b.x << ' ' << value.segment.b.y << '\n';
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            out << "P CIRCLE "
                << value.circle.center.x << ' ' << value.circle.center.y << ' '
                << value.circle.radius << '\n';
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            out << "P ARC "
                << value.arc.center.x << ' ' << value.arc.center.y << ' '
                << value.arc.radius << ' '
                << value.arc.start_angle << ' '
                << value.arc.end_angle << ' '
                << (value.arc.counter_clockwise ? 1 : 0) << '\n';
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            out << "P POLY " << (value.closed ? 1 : 0) << ' ';
            write_points(out, value.points);
            out << '\n';
        }
    }, primitive);
}

bool read_primitive(std::istream& in, BlockPrimitive& primitive) {
    std::string record;
    std::string type;
    if (!(in >> record >> type) || record != "P") {
        return false;
    }

    if (type == "LINE") {
        LineEntity value;
        if (!(in >> value.segment.a.x >> value.segment.a.y
                 >> value.segment.b.x >> value.segment.b.y)) {
            return false;
        }
        primitive = value;
        return true;
    }

    if (type == "CIRCLE") {
        CircleEntity value;
        if (!(in >> value.circle.center.x >> value.circle.center.y
                 >> value.circle.radius)) {
            return false;
        }
        primitive = value;
        return true;
    }

    if (type == "ARC") {
        ArcEntity value;
        int ccw{};
        if (!(in >> value.arc.center.x >> value.arc.center.y
                 >> value.arc.radius
                 >> value.arc.start_angle
                 >> value.arc.end_angle
                 >> ccw)) {
            return false;
        }
        value.arc.counter_clockwise = ccw != 0;
        primitive = value;
        return true;
    }

    if (type == "POLY") {
        PolylineEntity value;
        int closed{};
        if (!(in >> closed) || !read_points(in, value.points)) {
            return false;
        }
        value.closed = closed != 0;
        primitive = std::move(value);
        return true;
    }

    return false;
}

void write_entity_payload(std::ostream& out, const Entity& entity) {
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, LineEntity>) {
            out << "LINE "
                << value.segment.a.x << ' ' << value.segment.a.y << ' '
                << value.segment.b.x << ' ' << value.segment.b.y;
        } else if constexpr (std::is_same_v<T, CircleEntity>) {
            out << "CIRCLE "
                << value.circle.center.x << ' ' << value.circle.center.y << ' '
                << value.circle.radius;
        } else if constexpr (std::is_same_v<T, ArcEntity>) {
            out << "ARC "
                << value.arc.center.x << ' ' << value.arc.center.y << ' '
                << value.arc.radius << ' '
                << value.arc.start_angle << ' '
                << value.arc.end_angle << ' '
                << (value.arc.counter_clockwise ? 1 : 0);
        } else if constexpr (std::is_same_v<T, PolylineEntity>) {
            out << "POLY " << (value.closed ? 1 : 0) << ' ';
            write_points(out, value.points);
        } else if constexpr (std::is_same_v<T, BlockReferenceEntity>) {
            out << "BLOCKREF "
                << value.block_id << ' '
                << value.insertion_point.x << ' ' << value.insertion_point.y << ' '
                << value.rotation << ' ' << value.scale;
        } else if constexpr (std::is_same_v<T, TextEntity>) {
            out << "TEXT "
                << value.position.x << ' ' << value.position.y << ' '
                << value.height << ' ' << value.rotation << ' '
                << std::quoted(value.text);
        } else if constexpr (std::is_same_v<T, LinearDimensionEntity>) {
            out << "DIM "
                << value.first.x << ' ' << value.first.y << ' '
                << value.second.x << ' ' << value.second.y << ' '
                << value.line_point.x << ' ' << value.line_point.y << ' '
                << (value.text_override.has_value() ? 1 : 0);
            if (value.text_override.has_value()) {
                out << ' ' << std::quoted(*value.text_override);
            }
        } else if constexpr (std::is_same_v<T, HatchEntity>) {
            out << "HATCH "
                << std::quoted(value.pattern) << ' '
                << value.angle << ' '
                << value.spacing << ' '
                << (value.solid ? 1 : 0) << ' ';
            write_points(out, value.boundary);
        }
    }, entity);
}

bool read_entity_payload(std::istream& in, const BlockLibrary& blocks, Entity& entity) {
    std::string type;
    if (!(in >> type)) {
        return false;
    }

    if (type == "LINE") {
        LineEntity value;
        if (!(in >> value.segment.a.x >> value.segment.a.y
                 >> value.segment.b.x >> value.segment.b.y)) return false;
        entity = value;
        return true;
    }
    if (type == "CIRCLE") {
        CircleEntity value;
        if (!(in >> value.circle.center.x >> value.circle.center.y >> value.circle.radius)) return false;
        entity = value;
        return true;
    }
    if (type == "ARC") {
        ArcEntity value;
        int ccw{};
        if (!(in >> value.arc.center.x >> value.arc.center.y >> value.arc.radius
                 >> value.arc.start_angle >> value.arc.end_angle >> ccw)) return false;
        value.arc.counter_clockwise = ccw != 0;
        entity = value;
        return true;
    }
    if (type == "POLY") {
        PolylineEntity value;
        int closed{};
        if (!(in >> closed) || !read_points(in, value.points)) return false;
        value.closed = closed != 0;
        entity = std::move(value);
        return true;
    }
    if (type == "BLOCKREF") {
        BlockReferenceEntity value;
        if (!(in >> value.block_id >> value.insertion_point.x >> value.insertion_point.y
                 >> value.rotation >> value.scale) ||
            blocks.find(value.block_id) == nullptr) return false;
        entity = value;
        return true;
    }
    if (type == "TEXT") {
        TextEntity value;
        if (!(in >> value.position.x >> value.position.y >> value.height >> value.rotation
                 >> std::quoted(value.text))) return false;
        entity = std::move(value);
        return true;
    }
    if (type == "DIM") {
        LinearDimensionEntity value;
        int has_override{};
        if (!(in >> value.first.x >> value.first.y >> value.second.x >> value.second.y
                 >> value.line_point.x >> value.line_point.y >> has_override)) return false;
        if (has_override != 0) {
            std::string override_text;
            if (!(in >> std::quoted(override_text))) return false;
            value.text_override = std::move(override_text);
        }
        entity = std::move(value);
        return true;
    }
    if (type == "HATCH") {
        HatchEntity value;
        int solid{};
        if (!(in >> std::quoted(value.pattern) >> value.angle >> value.spacing >> solid) ||
            !read_points(in, value.boundary)) return false;
        value.solid = solid != 0;
        entity = std::move(value);
        return true;
    }

    return false;
}

} // namespace

std::string serialize_project(
    const Document& document,
    const BlockLibrary& blocks,
    const ProjectSettings& settings) {
    std::ostringstream out;
    out << std::setprecision(17);
    out << "ACP2D 1\n";

    for (const LayerId id : document.layer_ids()) {
        const Layer* layer = document.layer(id);
        if (layer == nullptr) continue;
        out << "L " << layer->id << ' ' << std::quoted(layer->name) << ' '
            << (layer->visible ? 1 : 0) << ' '
            << (layer->locked ? 1 : 0) << ' '
            << layer->line_weight << '\n';
    }

    for (const BlockId id : blocks.ids()) {
        const BlockDefinition* block = blocks.find(id);
        if (block == nullptr) continue;
        out << "B " << block->id << ' ' << std::quoted(block->name) << ' '
            << block->base_point.x << ' ' << block->base_point.y << ' '
            << block->geometry.size() << '\n';
        for (const auto& primitive : block->geometry) write_primitive(out, primitive);
    }

    for (const EntityId id : document.ids()) {
        const Entity* entity = document.find(id);
        const EntityProperties* properties = document.properties(id);
        if (entity == nullptr || properties == nullptr) continue;

        out << "E " << id << ' ' << properties->layer_id << ' '
            << (properties->visible ? 1 : 0) << ' '
            << (properties->line_weight_override.has_value() ? 1 : 0) << ' '
            << properties->line_weight_override.value_or(0.0) << ' ';
        write_entity_payload(out, *entity);
        out << '\n';
    }

    const auto& page = settings.page_setup;
    out << "PAGE "
        << static_cast<int>(page.paper) << ' '
        << static_cast<int>(page.orientation) << ' '
        << page.margins.left << ' '
        << page.margins.right << ' '
        << page.margins.top << ' '
        << page.margins.bottom << ' '
        << (settings.print_scale_denominator.has_value() ? 1 : 0) << ' '
        << settings.print_scale_denominator.value_or(0.0) << '\n';

    out << "END\n";
    return out.str();
}

std::optional<ProjectData> deserialize_project(std::string_view data) {
    std::istringstream in{std::string(data)};
    std::string magic;
    int version{};
    if (!(in >> magic >> version) || magic != "ACP2D" || version != 1) return std::nullopt;

    ProjectData project;
    std::string record;
    bool saw_default_layer = false;
    bool saw_page_settings = false;

    while (in >> record) {
        if (record == "END") {
            return saw_default_layer ? std::optional<ProjectData>{std::move(project)} : std::nullopt;
        }

        if (record == "PAGE") {
            if (saw_page_settings) {
                return std::nullopt;
            }

            int paper{};
            int orientation{};
            int has_scale{};
            double scale{};
            layout::PageSetup page;
            if (!(in >> paper >> orientation
                     >> page.margins.left >> page.margins.right
                     >> page.margins.top >> page.margins.bottom
                     >> has_scale >> scale)) {
                return std::nullopt;
            }

            if (paper < static_cast<int>(layout::PaperSize::A4) ||
                paper > static_cast<int>(layout::PaperSize::A0) ||
                orientation < static_cast<int>(layout::Orientation::Portrait) ||
                orientation > static_cast<int>(layout::Orientation::Landscape) ||
                (has_scale != 0 && has_scale != 1)) {
                return std::nullopt;
            }

            page.paper = static_cast<layout::PaperSize>(paper);
            page.orientation = static_cast<layout::Orientation>(orientation);
            if (!layout::printable_size_mm(page).has_value()) {
                return std::nullopt;
            }

            if (has_scale != 0) {
                if (!std::isfinite(scale) || scale <= 0.0) {
                    return std::nullopt;
                }
                project.settings.print_scale_denominator = scale;
            } else {
                project.settings.print_scale_denominator.reset();
            }
            project.settings.page_setup = page;
            saw_page_settings = true;
            continue;
        }

        if (record == "L") {
            Layer value;
            int visible{};
            int locked{};
            if (!(in >> value.id >> std::quoted(value.name) >> visible >> locked >> value.line_weight)) {
                return std::nullopt;
            }
            value.visible = visible != 0;
            value.locked = locked != 0;

            if (value.id == kDefaultLayerId) {
                if (saw_default_layer || value.name.empty() || !std::isfinite(value.line_weight) || value.line_weight < 0.0) {
                    return std::nullopt;
                }
                Layer* default_layer = project.document.layer(kDefaultLayerId);
                if (default_layer == nullptr) return std::nullopt;
                *default_layer = std::move(value);
                saw_default_layer = true;
            } else if (!project.document.insert_layer_with_id(std::move(value))) {
                return std::nullopt;
            }
            continue;
        }

        if (record == "B") {
            BlockDefinition definition;
            std::size_t count{};
            if (!(in >> definition.id >> std::quoted(definition.name)
                     >> definition.base_point.x >> definition.base_point.y >> count)) return std::nullopt;

            definition.geometry.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                BlockPrimitive primitive;
                if (!read_primitive(in, primitive)) return std::nullopt;
                definition.geometry.push_back(std::move(primitive));
            }

            if (!project.blocks.insert_with_id(std::move(definition))) return std::nullopt;
            continue;
        }

        if (record == "E") {
            EntityId id{};
            LayerId layer_id{};
            int visible{};
            int has_weight{};
            double weight{};
            if (!(in >> id >> layer_id >> visible >> has_weight >> weight)) return std::nullopt;

            Entity entity;
            if (!read_entity_payload(in, project.blocks, entity) ||
                project.document.layer(layer_id) == nullptr ||
                !project.document.insert_with_id(id, std::move(entity))) return std::nullopt;

            EntityProperties* properties = project.document.properties(id);
            if (properties == nullptr) return std::nullopt;
            properties->layer_id = layer_id;
            properties->visible = visible != 0;
            if (has_weight != 0) properties->line_weight_override = weight;
            continue;
        }

        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace acp::persistence
