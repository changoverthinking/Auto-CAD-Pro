#include "acp/obj.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace acp::obj {
namespace {

std::string_view kind_prefix(model3d::ObjectKind kind) noexcept {
    switch (kind) {
        case model3d::ObjectKind::Wall: return "Wall_";
        case model3d::ObjectKind::Slab: return "Slab_";
        case model3d::ObjectKind::Column: return "Column_";
        case model3d::ObjectKind::Beam: return "Beam_";
        case model3d::ObjectKind::Door: return "Door_";
        case model3d::ObjectKind::Window: return "Window_";
        case model3d::ObjectKind::Roof: return "Roof_";
        case model3d::ObjectKind::Stair: return "Stair_";
        case model3d::ObjectKind::Generic: break;
    }
    return {};
}

std::string export_name(const model3d::Object3D& object) {
    std::string name = object.name.empty()
        ? "Object_" + std::to_string(object.id)
        : object.name;
    const std::string_view prefix = kind_prefix(object.kind);
    if (!prefix.empty() && !std::string_view{name}.starts_with(prefix)) {
        name = std::string{prefix} + name;
    }
    return name;
}

} // namespace

std::string serialize(const model3d::Scene& scene) {
    std::ostringstream out;
    out << "# Auto CAD Pro 3D OBJ\n";
    out << std::setprecision(17);

    std::size_t vertex_offset = 1;
    for (const model3d::ObjectId id : scene.ids()) {
        const model3d::Object3D* object = scene.find(id);
        if (object == nullptr || !object->visible) {
            continue;
        }

        out << "o " << export_name(*object) << "\n";

        for (const geo3d::Vec3 vertex : object->mesh.vertices) {
            out << "v " << vertex.x << ' '
                << vertex.y << ' '
                << vertex.z << "\n";
        }

        for (const geo3d::Triangle triangle : object->mesh.triangles) {
            out << "f "
                << vertex_offset + triangle.a << ' '
                << vertex_offset + triangle.b << ' '
                << vertex_offset + triangle.c << "\n";
        }
        vertex_offset += object->mesh.vertices.size();
    }

    return out.str();
}

bool save(
    const std::filesystem::path& path,
    const model3d::Scene& scene) {

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    const std::string data = serialize(scene);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.flush();
    return file.good();
}

} // namespace acp::obj
