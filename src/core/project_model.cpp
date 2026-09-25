#include "acp/project_model.hpp"

#include <algorithm>
#include <utility>

namespace acp::bim {

LevelId ProjectModel::add_level(std::string name, double elevation_mm) {
    const LevelId id = next_level_id_++;
    levels_.emplace(id, Level{id, std::move(name), elevation_mm});
    return id;
}

MaterialId ProjectModel::add_material(std::string name, ParameterSet parameters) {
    const MaterialId id = next_material_id_++;
    materials_.emplace(id, Material{id, std::move(name), std::move(parameters)});
    return id;
}

ElementTypeId ProjectModel::add_type(
    ElementKind kind,
    std::string name,
    ParameterSet parameters,
    std::vector<MaterialId> materials) {

    const ElementTypeId id = next_type_id_++;
    types_.emplace(
        id,
        ElementType{id, kind, std::move(name), std::move(parameters), std::move(materials)});
    return id;
}

ElementId ProjectModel::add_element(Element element) {
    if (element.id == kInvalidElementId) {
        element.id = next_element_id_++;
    } else {
        if (elements_.contains(element.id)) {
            return kInvalidElementId;
        }
        next_element_id_ = std::max(next_element_id_, element.id + 1);
    }

    const ElementId id = element.id;
    elements_.emplace(id, std::move(element));
    return id;
}

bool ProjectModel::erase_element(ElementId id) {
    if (elements_.erase(id) == 0) {
        return false;
    }

    host_by_element_.erase(id);
    for (auto it = host_by_element_.begin(); it != host_by_element_.end();) {
        if (it->second == id) {
            it = host_by_element_.erase(it);
        } else {
            ++it;
        }
    }
    return true;
}

bool ProjectModel::set_host(ElementId hosted, ElementId host) {
    if (hosted == host || !elements_.contains(hosted) || !elements_.contains(host)) {
        return false;
    }
    // Reject cycles before touching the old host relationship. The bounded
    // traversal also refuses a pre-existing corrupt chain instead of hanging.
    ElementId ancestor = host;
    for (std::size_t depth = 0; ; ++depth) {
        if (ancestor == hosted || depth >= elements_.size()) return false;
        const auto parent = host_by_element_.find(ancestor);
        if (parent == host_by_element_.end()) break;
        ancestor = parent->second;
    }
    host_by_element_[hosted] = host;
    return true;
}

bool ProjectModel::clear_host(ElementId hosted) {
    return host_by_element_.erase(hosted) != 0;
}

const Level* ProjectModel::find_level(LevelId id) const {
    const auto it = levels_.find(id);
    return it == levels_.end() ? nullptr : &it->second;
}

const Material* ProjectModel::find_material(MaterialId id) const {
    const auto it = materials_.find(id);
    return it == materials_.end() ? nullptr : &it->second;
}

const ElementType* ProjectModel::find_type(ElementTypeId id) const {
    const auto it = types_.find(id);
    return it == types_.end() ? nullptr : &it->second;
}

const Element* ProjectModel::find_element(ElementId id) const {
    const auto it = elements_.find(id);
    return it == elements_.end() ? nullptr : &it->second;
}

Element* ProjectModel::find_element(ElementId id) {
    const auto it = elements_.find(id);
    return it == elements_.end() ? nullptr : &it->second;
}

std::optional<ElementId> ProjectModel::host_of(ElementId hosted) const {
    const auto it = host_by_element_.find(hosted);
    if (it == host_by_element_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<LevelId> ProjectModel::level_ids() const {
    std::vector<LevelId> ids;
    ids.reserve(levels_.size());
    for (const auto& [id, level] : levels_) {
        (void)level;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<ElementId> ProjectModel::element_ids() const {
    std::vector<ElementId> ids;
    ids.reserve(elements_.size());
    for (const auto& [id, element] : elements_) {
        (void)element;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace acp::bim
