/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "function/render_vk/rend_graph_recipe_builder_vk.hh"
#include "core/data/data_value.hh"
#include "function/render_vk/i_bindable_render_resource_vk.hh"
#include "function/render_vk/rend_graph_recipe_params_vk.hh"
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_task_registry_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/rt_triggered_vk.hh"
#include "generated/serializables_sd.gen.hh"
#include <algorithm>
#include <cstddef>
#include <format>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

namespace {

std::string resource_location(size_t p_index, std::string_view p_id) {
    return std::format("resources[{}] '{}'", p_index, p_id);
}

std::string task_location(size_t p_index, std::string_view p_id) {
    return std::format("tasks[{}] '{}'", p_index, p_id);
}

void bind_imports(RendGraphDescVK& p_graph, const RendGraphRecipeVK& p_recipe, const RendGraphRecipeImportsVK& p_imports) {
    std::set<std::string, std::less<>> declared;
    for (size_t i = 0; i < p_recipe.imports.size(); ++i) {
        const std::string& name = p_recipe.imports[i];
        if (!declared.insert(name).second) {
            throw std::runtime_error(
                std::format("Failed to build render graph recipe: imports[{}] '{}' is listed more than once.", i, name));
        }
        const IBindableRenderResourceVK* resource = p_imports.find(name);
        if (resource == nullptr) {
            throw std::runtime_error(std::format("Failed to build render graph recipe: imports[{}] '{}' is not supplied.", i, name));
        }
        p_graph.add_resource(name, *resource);
    }
    for (const std::string& name : p_imports.get_names()) {
        if (!declared.contains(name)) {
            throw std::runtime_error(
                std::format("Failed to build render graph recipe: the supplied import '{}' is not declared by the recipe.", name));
        }
    }
}

void declare_resources(RendGraphDescVK& p_graph, const RendGraphRecipeVK& p_recipe) {
    std::set<std::string, std::less<>> ids;
    for (size_t i = 0; i < p_recipe.resources.size(); ++i) {
        const RendRecipeResourceVK& resource = p_recipe.resources[i];
        if (resource.id.empty()) {
            throw std::runtime_error(std::format("Failed to build render graph recipe: resources[{}] has an empty id.", i));
        }
        if (!ids.insert(resource.id).second || p_graph.contains_resource(resource.id)) {
            throw std::runtime_error(std::format("Failed to build render graph recipe: {} repeats a resource or import id.",
                resource_location(i, resource.id)));
        }
        const int kind_count = static_cast<int>(resource.buffer.has_value()) + static_cast<int>(resource.image.has_value()) +
                               static_cast<int>(resource.image_view.has_value()) + static_cast<int>(resource.sampler.has_value());
        if (kind_count != 1) {
            throw std::runtime_error(std::format("Failed to build render graph recipe: {} must set exactly one of buffer, image, image_view, and sampler.",
                resource_location(i, resource.id)));
        }
        if (resource.buffer.has_value()) {
            p_graph.add_buffer(resource.id, [params = *resource.buffer](const RendGraphContextVK& /*p_context*/) {
                return params.to_desc();
            });
        } else if (resource.image.has_value()) {
            p_graph.add_image(resource.id, [params = *resource.image](const RendGraphContextVK& p_context) {
                return params.to_desc(p_context);
            });
        } else if (resource.image_view.has_value()) {
            p_graph.add_image_view(
                resource.id, resource.image_view->image, [params = resource.image_view->view](const RendGraphContextVK& /*p_context*/) {
                    return params.to_desc();
                });
        } else {
            p_graph.add_sampler(resource.id, [params = *resource.sampler](const RendGraphContextVK& /*p_context*/) {
                return params.to_desc();
            });
        }
    }
}

void apply_aliases(RendTaskDescVK& p_task, const DataValue& p_aliases, std::string_view p_location) {
    if (p_aliases.is_nil()) {
        return;
    }
    if (p_aliases.get_kind() == DataValueKind::LIST && p_aliases.as_list().empty()) {
        // An empty Lua table reads as an empty list.
        return;
    }
    if (p_aliases.get_kind() != DataValueKind::MAP) {
        throw std::runtime_error(std::format("Failed to build render graph recipe: {} aliases must be a map.", p_location));
    }
    for (const auto& [task_resource_id, graph_resource_id] : p_aliases.as_map()) {
        if (!graph_resource_id.is_string()) {
            throw std::runtime_error(std::format(
                "Failed to build render graph recipe: {} alias '{}' must map to a graph resource id string.", p_location, task_resource_id));
        }
        p_task.alias(task_resource_id, graph_resource_id.as_string());
    }
}

bool sets_aliases(const DataValue& p_aliases) {
    // An empty Lua table reads as an empty list.
    return !p_aliases.is_nil() && !(p_aliases.get_kind() == DataValueKind::LIST && p_aliases.as_list().empty());
}

// Copy the task the caller supplies for an imported recipe task into the graph.
void add_imported_task(RendGraphDescVK& p_graph, const RendRecipeTaskVK& p_recipe_task, const RendGraphRecipeImportsVK& p_imports,
    std::string_view p_location) {
    if (!p_recipe_task.type.empty()) {
        throw std::runtime_error(std::format("Failed to build render graph recipe: {} is imported, so it cannot set a type.", p_location));
    }
    if (!p_recipe_task.params.is_nil()) {
        throw std::runtime_error(std::format("Failed to build render graph recipe: {} is imported, so it cannot set params.", p_location));
    }
    if (sets_aliases(p_recipe_task.aliases)) {
        throw std::runtime_error(std::format("Failed to build render graph recipe: {} is imported, so it cannot set aliases.", p_location));
    }
    if (p_recipe_task.trigger.has_value()) {
        throw std::runtime_error(std::format("Failed to build render graph recipe: {} is imported, so it cannot set a trigger.", p_location));
    }
    const RendTaskDescVK* task = p_imports.find_task(p_recipe_task.id);
    if (task == nullptr) {
        throw std::runtime_error(std::format("Failed to build render graph recipe: {} is imported but not supplied.", p_location));
    }
    p_graph.add_task(*task);
}

void add_tasks(RendGraphDescVK& p_graph, const RendGraphRecipeVK& p_recipe, const RendGraphRecipeImportsVK& p_imports,
    const BaseRenderDataVK* p_render_data) {
    std::set<std::string, std::less<>> ids;
    for (size_t i = 0; i < p_recipe.tasks.size(); ++i) {
        const RendRecipeTaskVK& recipe_task = p_recipe.tasks[i];
        if (recipe_task.id.empty()) {
            throw std::runtime_error(std::format("Failed to build render graph recipe: tasks[{}] has an empty id.", i));
        }
        if (!ids.insert(recipe_task.id).second) {
            throw std::runtime_error(
                std::format("Failed to build render graph recipe: {} repeats a task id.", task_location(i, recipe_task.id)));
        }
        if (recipe_task.import) {
            add_imported_task(p_graph, recipe_task, p_imports, task_location(i, recipe_task.id));
            continue;
        }
        if (recipe_task.type.empty()) {
            throw std::runtime_error(std::format(
                "Failed to build render graph recipe: {} sets no type; set one, or set import for a supplied task.", task_location(i, recipe_task.id)));
        }
        RendTaskDescVK& task =
            RenderTaskRegistryVK::add_task(p_graph, recipe_task.id, recipe_task.type, recipe_task.params, p_render_data);
        apply_aliases(task, recipe_task.aliases, task_location(i, recipe_task.id));
        if (recipe_task.trigger.has_value()) {
            if (recipe_task.trigger->event.empty()) {
                throw std::runtime_error(std::format(
                    "Failed to build render graph recipe: {} trigger event is empty.", task_location(i, recipe_task.id)));
            }
            task.factory = RTTriggeredVK::wrap_factory(
                std::move(task.factory), recipe_task.trigger->event, p_render_data, recipe_task.trigger->activate);
        }
    }
    for (const std::string& task_id : p_imports.get_task_ids()) {
        if (!ids.contains(task_id)) {
            throw std::runtime_error(std::format(
                "Failed to build render graph recipe: the supplied task '{}' is not imported by the recipe.", task_id));
        }
    }
    // Edges may point to later tasks, so they are added once every task exists.
    for (size_t i = 0; i < p_recipe.tasks.size(); ++i) {
        const RendRecipeTaskVK& recipe_task = p_recipe.tasks[i];
        for (size_t j = 0; j < recipe_task.connect.size(); ++j) {
            const RendRecipeEdgeVK& edge = recipe_task.connect[j];
            if (!ids.contains(edge.to)) {
                throw std::runtime_error(std::format("Failed to build render graph recipe: {} connect[{}] targets the unknown task '{}'.",
                    task_location(i, recipe_task.id),
                    j,
                    edge.to));
            }
            p_graph.get_task(recipe_task.id).connect(edge.to, edge.resource);
        }
    }
}

} // namespace

RendGraphRecipeImportsVK& RendGraphRecipeImportsVK::add(std::string_view p_name, const IBindableRenderResourceVK& p_resource) {
    if (p_name.empty()) {
        throw std::runtime_error("Failed to add render graph recipe import: the name is empty.");
    }
    if (!resources.emplace(std::string(p_name), &p_resource).second) {
        throw std::runtime_error(std::format("Failed to add render graph recipe import: '{}' is already supplied.", p_name));
    }
    return *this;
}

const IBindableRenderResourceVK* RendGraphRecipeImportsVK::find(std::string_view p_name) const {
    auto resource = resources.find(p_name);
    return resource == resources.end() ? nullptr : resource->second;
}

RendGraphRecipeImportsVK& RendGraphRecipeImportsVK::add_task(RendTaskDescVK p_task) {
    if (p_task.task_id.empty()) {
        throw std::runtime_error("Failed to add render graph recipe task import: the task id is empty.");
    }
    const std::string task_id = p_task.task_id;
    if (!tasks.emplace(task_id, std::move(p_task)).second) {
        throw std::runtime_error(std::format("Failed to add render graph recipe task import: '{}' is already supplied.", task_id));
    }
    return *this;
}

const RendTaskDescVK* RendGraphRecipeImportsVK::find_task(std::string_view p_task_id) const {
    auto task = tasks.find(p_task_id);
    return task == tasks.end() ? nullptr : &task->second;
}

std::vector<std::string> RendGraphRecipeImportsVK::get_task_ids() const {
    std::vector<std::string> task_ids;
    task_ids.reserve(tasks.size());
    for (const auto& [task_id, task] : tasks) {
        static_cast<void>(task);
        task_ids.push_back(task_id);
    }
    return task_ids;
}

std::vector<std::string> RendGraphRecipeImportsVK::get_names() const {
    std::vector<std::string> names;
    names.reserve(resources.size());
    for (const auto& [name, resource] : resources) {
        static_cast<void>(resource);
        names.push_back(name);
    }
    return names;
}

RendGraphDescVK RendGraphRecipeBuilderVK::build(
    const DataValue& p_recipe, const RendGraphRecipeImportsVK& p_imports, const BaseRenderDataVK* p_render_data) const {
    if (p_render_data == nullptr || p_render_data->allocator == nullptr) {
        throw std::runtime_error("Failed to build render graph recipe: render data is unavailable.");
    }
    RendGraphRecipeVK recipe;
    SerializableSD<RendGraphRecipeVK>::deserialize(p_recipe, recipe);
    RendGraphDescVK graph(p_render_data->allocator);
    bind_imports(graph, recipe, p_imports);
    declare_resources(graph, recipe);
    add_tasks(graph, recipe, p_imports, p_render_data);
    return graph;
}

} // namespace WhiteBirdEngine
