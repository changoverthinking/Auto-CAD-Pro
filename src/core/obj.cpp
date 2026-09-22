#include "acp/obj.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace acp::obj {

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

        out << "o "
            << (object->name.empty()
                    ? "Object_" + std::to_string(id)
                    : object->name)
            << "\n";

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
