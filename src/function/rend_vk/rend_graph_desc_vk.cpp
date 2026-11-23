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
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "core/alloc/i_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_rend_graph.hh"
#include "function/render/i_rend_task.hh"
#include "function/render/rend_defs.hh"
#include "function/render/rend_task_resource_map.hh"
#include "function/rend_vk/i_bindable_rend_resource_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_vk.hh"
#include "function/render/i_rend_object_container.hh"
#include "function/render/roc_single.hh"
#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

namespace {

// Renames the identifiers of a subgraph being added: task and subgraph ids, and the resource identifiers its scope maps.
class SubgraphScope {
public:
    SubgraphScope(RendAlloc* p_alloc, std::string_view p_subgraph_id)
        : prefix(std::format("{}.", p_subgraph_id)), local_ids(p_alloc), resource_sources(p_alloc),
          resource_targets(p_alloc) {
    }

    std::string prefixed(std::string_view p_id) const {
        return prefix + std::string(p_id);
    }

    void add_local_id(std::string_view p_id) {
        local_ids.emplace_back(p_id);
    }

    void add_resource(std::string_view p_source, std::string p_target) {
        resource_sources.emplace_back(p_source);
        resource_targets.push_back(std::move(p_target));
    }

    // The id a task or subgraph inside the scope has outside it.
    std::string task_id(std::string_view p_id, std::string_view p_subgraph_id) const {
        if (std::ranges::find(local_ids, p_id) == local_ids.end()) {
            throw std::runtime_error(std::format(
                "Failed to add render graph subgraph '{}': an edge inside it connects to '{}', which is not in the subgraph.", p_subgraph_id, p_id));
        }
        return prefixed(p_id);
    }

    // The identifier a resource named inside the scope has outside it.
    std::string resource_id(std::string_view p_id) const {
        for (size_t i = 0; i < resource_sources.size(); ++i) {
            if (resource_sources[i] == p_id) {
                return resource_targets[i];
            }
        }
        return std::string(p_id);
    }

    // Renames edges declared inside the scope to the ids their tasks and resources have outside it.
    void rename_edges(Vector<RendResEdgeDescVK, RendAlloc>& p_edges, std::string_view p_subgraph_id) const {
        for (RendResEdgeDescVK& edge : p_edges) {
            edge.next_task_id = task_id(edge.next_task_id, p_subgraph_id);
            edge.res_id = resource_id(edge.res_id);
        }
    }

    Vector<RendTaskResourceMap::ScopeBinding, RendAlloc> bindings(RendAlloc* p_alloc) const {
        Vector<RendTaskResourceMap::ScopeBinding, RendAlloc> result(p_alloc);
        for (size_t i = 0; i < resource_sources.size(); ++i) {
            result.push_back({.source = resource_sources[i], .target = resource_targets[i]});
        }
        return result;
    }

private:
    std::string prefix;
    Vector<std::string, RendAlloc> local_ids;
    Vector<std::string, RendAlloc> resource_sources;
    Vector<std::string, RendAlloc> resource_targets;
};

// Whether two imports bind the same resource. Importing a render object wraps it in a new single-object container each
// time, so those match by the object they hold.
bool is_same_import(const RendResVK& p_left, const RendResVK& p_right) {
    if (p_left.res_type_id != p_right.res_type_id || p_left.resource_list != p_right.resource_list) {
        return false;
    }
    if (p_left.container == p_right.container) {
        return true;
    }
    if (p_left.container == MEM_NULL || p_right.container == MEM_NULL) {
        return false;
    }
    const Ref<ROCSingle> left = p_left.container.dynamic_cast_ref<ROCSingle>();
    const Ref<ROCSingle> right = p_right.container.dynamic_cast_ref<ROCSingle>();
    return left != MEM_NULL && right != MEM_NULL && left->get().get() == right->get().get();
}

// The debug name of a resource, task, or subgraph nested under a subgraph name: its own debug name, or its id when it
// has none.
std::string nested_debug_name(std::string_view p_name_prefix, std::string_view p_debug_name, std::string_view p_id) {
    return std::format("{}.{}", p_name_prefix, p_debug_name.empty() ? p_id : p_debug_name);
}

// Checks that a subgraph id is not empty and is not the id of a task or subgraph of the enclosing graph.
void validate_subgraph_id(const RendGraphDescVK& p_graph, std::string_view p_subgraph_id) {
    if (p_subgraph_id.empty()) {
        throw std::runtime_error("Failed to add render graph subgraph: the subgraph id is empty.");
    }
    const auto has_task = std::ranges::any_of(p_graph.tasks, [p_subgraph_id](const RendTaskDescVK& p_task) {
        return p_task.task_id == p_subgraph_id;
    });
    const auto has_subgraph = std::ranges::any_of(p_graph.subgraphs, [p_subgraph_id](const RendSubgraphVK& p_record) {
        return p_record.subgraph_id == p_subgraph_id;
    });
    if (has_task || has_subgraph) {
        throw std::runtime_error(
            std::format("Failed to add render graph subgraph '{}': a task or subgraph already has the id.", p_subgraph_id));
    }
}

// Checks that a subgraph exports nothing; only the outermost graph exports.
void validate_subgraph_exports(const RendGraphDescVK& p_inner, std::string_view p_subgraph_id) {
    if (!p_inner.exports.empty()) {
        throw std::runtime_error(std::format("Failed to add render graph subgraph '{}': it exports '{}'; only the outermost graph exports resources.",
            p_subgraph_id,
            p_inner.exports.begin()->first));
    }
}

// Checks that every port is bound exactly once to a non-empty resource of the enclosing graph, that every binding names a
// port, and that no port shadows what the subgraph declares or imports.
void validate_subgraph_ports(const RendSubgraphDescVK& p_subgraph,
    std::span<const RendSubgraphPortBindingVK> p_ports,
    std::string_view p_subgraph_id) {
    for (const RendSubgraphPortBindingVK& binding : p_ports) {
        if (std::ranges::find(p_subgraph.ports, binding.port) == p_subgraph.ports.end()) {
            throw std::runtime_error(
                std::format("Failed to add render graph subgraph '{}': it has no port '{}'.", p_subgraph_id, binding.port));
        }
        if (binding.resource.empty()) {
            throw std::runtime_error(std::format("Failed to add render graph subgraph '{}': port '{}' is bound to an empty resource id.",
                p_subgraph_id,
                binding.port));
        }
    }
    for (const std::string& port : p_subgraph.ports) {
        const auto count = std::ranges::count_if(p_ports, [&port](const RendSubgraphPortBindingVK& p_binding) {
            return p_binding.port == port;
        });
        if (count != 1) {
            throw std::runtime_error(std::format(
                "Failed to add render graph subgraph '{}': port '{}' is bound {} times instead of once.", p_subgraph_id, port, count));
        }
        if (p_subgraph.graph.contains_resource(port)) {
            throw std::runtime_error(std::format(
                "Failed to add render graph subgraph '{}': port '{}' names a resource the subgraph declares or imports.", p_subgraph_id, port));
        }
    }
}

// Builds the scope of a subgraph: its ports map to their bound resources, its declared resources to prefixed ids, and its
// tasks and nested subgraphs are its local ids.
SubgraphScope make_subgraph_scope(RendAlloc* p_alloc,
    std::string_view p_subgraph_id,
    const RendGraphDescVK& p_inner,
    std::span<const RendSubgraphPortBindingVK> p_ports) {
    SubgraphScope scope(p_alloc, p_subgraph_id);
    for (const RendSubgraphPortBindingVK& binding : p_ports) {
        scope.add_resource(binding.port, std::string(binding.resource));
    }
    const auto add_owned = [&scope](const auto& p_declarations) {
        for (const auto& declaration : p_declarations) {
            scope.add_resource(declaration.res_id, scope.prefixed(declaration.res_id));
        }
    };
    add_owned(p_inner.buffers);
    add_owned(p_inner.images);
    add_owned(p_inner.image_views);
    add_owned(p_inner.samplers);
    for (const RendTaskDescVK& task : p_inner.tasks) {
        scope.add_local_id(task.task_id);
    }
    for (const RendSubgraphVK& nested : p_inner.subgraphs) {
        scope.add_local_id(nested.subgraph_id);
    }
    return scope;
}

// Adds a subgraph's imports to the enclosing graph; one it already imports under the same id must be the same resource.
void merge_subgraph_imports(RendGraphDescVK& p_outer, const RendGraphDescVK& p_inner, std::string_view p_subgraph_id) {
    for (const RendResVK& resource : p_inner.resources) {
        const auto existing = std::ranges::find_if(p_outer.resources, [&resource](const RendResVK& p_resource) {
            return p_resource.res_id == resource.res_id;
        });
        if (existing == p_outer.resources.end()) {
            if (p_outer.contains_resource(resource.res_id)) {
                throw std::runtime_error(std::format("Failed to add render graph subgraph '{}': it imports '{}', which the graph declares.",
                    p_subgraph_id,
                    resource.res_id));
            }
            p_outer.resources.push_back(resource);
            continue;
        }
        if (!is_same_import(*existing, resource)) {
            throw std::runtime_error(std::format("Failed to add render graph subgraph '{}': it imports '{}', which the graph imports as another resource.",
                p_subgraph_id,
                resource.res_id));
        }
    }
}

// Adds a subgraph's graph-owned resource declarations to the enclosing graph under their prefixed ids.
void add_subgraph_declarations(RendGraphDescVK& p_outer, const RendGraphDescVK& p_inner, const SubgraphScope& p_scope) {
    for (const RendBufferDeclVK& buffer : p_inner.buffers) {
        p_outer.buffers.push_back({.res_id = p_scope.prefixed(buffer.res_id), .factory = buffer.factory});
    }
    for (const RendImageDeclVK& image : p_inner.images) {
        p_outer.images.push_back({.res_id = p_scope.prefixed(image.res_id), .factory = image.factory});
    }
    for (const RendImageViewDeclVK& view : p_inner.image_views) {
        p_outer.image_views.push_back(
            {.res_id = p_scope.prefixed(view.res_id), .image_id = p_scope.resource_id(view.image_id), .factory = view.factory});
    }
    for (const RendSamplerDeclVK& sampler : p_inner.samplers) {
        p_outer.samplers.push_back({.res_id = p_scope.prefixed(sampler.res_id), .factory = sampler.factory});
    }
}

// Adds a subgraph's resource debug names to the enclosing graph, nested under the subgraph's name.
void add_subgraph_debug_names(RendGraphDescVK& p_outer,
    const RendGraphDescVK& p_inner,
    const SubgraphScope& p_scope,
    std::string_view p_name_prefix) {
    for (const auto& [resource_id, debug_name] : p_inner.debug_names) {
        p_outer.debug_names.insert_or_assign(
            p_scope.prefixed(resource_id), nested_debug_name(p_name_prefix, debug_name, resource_id));
    }
}

// Adds a subgraph's tasks to the enclosing graph, renamed into its scope, and records them as members of p_record.
void add_subgraph_tasks(RendGraphDescVK& p_outer,
    const RendGraphDescVK& p_inner,
    const SubgraphScope& p_scope,
    std::string_view p_name_prefix,
    std::string_view p_subgraph_id,
    RendSubgraphVK& p_record) {
    const Vector<RendTaskResourceMap::ScopeBinding, RendAlloc> bindings = p_scope.bindings(p_outer.alloc);
    for (const RendTaskDescVK& inner_task : p_inner.tasks) {
        RendTaskDescVK task = inner_task;
        task.task_id = p_scope.prefixed(inner_task.task_id);
        task.debug_name = nested_debug_name(p_name_prefix, inner_task.debug_name, inner_task.task_id);
        p_scope.rename_edges(task.next, p_subgraph_id);
        task.resource_map.enclose(bindings);
        p_record.members.push_back(task.task_id);
        p_outer.tasks.push_back(std::move(task));
    }
}

// Adds a subgraph's nested subgraph records to the enclosing graph, renamed into its scope.
void add_nested_subgraphs(RendGraphDescVK& p_outer,
    const RendGraphDescVK& p_inner,
    const SubgraphScope& p_scope,
    std::string_view p_name_prefix,
    std::string_view p_subgraph_id) {
    for (const RendSubgraphVK& inner_record : p_inner.subgraphs) {
        RendSubgraphVK nested = inner_record;
        nested.subgraph_id = p_scope.prefixed(inner_record.subgraph_id);
        nested.debug_name = nested_debug_name(p_name_prefix, inner_record.debug_name, inner_record.subgraph_id);
        for (RendTaskID& member : nested.members) {
            member = p_scope.prefixed(member);
        }
        p_scope.rename_edges(nested.next, p_subgraph_id);
        p_outer.subgraphs.push_back(std::move(nested));
    }
}

} // namespace

RendTaskDescVK::RendTaskDescVK(RendAlloc* p_alloc) : next(p_alloc), resource_map(p_alloc) {
}

RendTaskDescVK::RendTaskDescVK(RendAlloc* p_alloc, std::string_view p_task_id, RendTaskFactoryVK p_factory)
    : task_id(p_task_id), next(p_alloc), resource_map(p_alloc), factory(std::move(p_factory)) {
    if (factory == nullptr) {
        throw std::runtime_error("Failed to construct render task descriptor: task factory is null.");
    }
}

RendTaskDescVK& RendTaskDescVK::connect(std::string_view p_next_task_id, std::string_view p_resource_id) {
    next.push_back({
        .next_task_id = std::string(p_next_task_id),
        .res_id = std::string(p_resource_id),
    });
    return *this;
}

RendTaskDescVK& RendTaskDescVK::alias(std::string_view p_task_resource_id, std::string_view p_graph_resource_id) {
    resource_map.bind(p_task_resource_id, p_graph_resource_id);
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(RendTaskDescVK p_task_desc) {
    tasks.emplace_back(std::move(p_task_desc));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_task(std::string_view p_task_id, RendTaskFactoryVK p_factory) {
    tasks.emplace_back(alloc, p_task_id, std::move(p_factory));
    return *this;
}

RendTaskDescVK& RendGraphDescVK::get_task(std::string_view p_task_id) {
    for (RendTaskDescVK& task : tasks) {
        if (task.task_id == p_task_id) {
            return task;
        }
    }
    throw std::runtime_error(std::format("Failed to get render graph task: no task has id '{}'.", p_task_id));
}

RendSubgraphVK::RendSubgraphVK(RendAlloc* p_alloc, std::string_view p_subgraph_id)
    : subgraph_id(p_subgraph_id), members(p_alloc), next(p_alloc) {
}

RendSubgraphVK& RendSubgraphVK::connect(std::string_view p_next_task_id, std::string_view p_resource_id) {
    next.push_back({.next_task_id = RendTaskID(p_next_task_id), .res_id = std::string(p_resource_id)});
    return *this;
}

IRendGraph& RendGraphDescVK::get_rend_graph() const {
    if (rend_graph == nullptr) {
        throw std::runtime_error("The render graph descriptor is not bound to a render graph; construct it with the graph it is built for.");
    }
    return *rend_graph;
}

RendSubgraphDescVK& RendSubgraphDescVK::add_port(std::string_view p_port) {
    if (p_port.empty()) {
        throw std::runtime_error("Failed to add render graph subgraph port: the port is empty.");
    }
    if (std::ranges::find(ports, p_port) != ports.end()) {
        throw std::runtime_error(std::format("Failed to add render graph subgraph port: port '{}' is declared twice.", p_port));
    }
    ports.emplace_back(p_port);
    return *this;
}

RendSubgraphVK& RendGraphDescVK::add_subgraph(
    std::string_view p_subgraph_id, const RendSubgraphDescVK& p_subgraph, std::span<const RendSubgraphPortBindingVK> p_ports) {
    validate_subgraph_id(*this, p_subgraph_id);
    const RendGraphDescVK& inner = p_subgraph.graph;
    validate_subgraph_exports(inner, p_subgraph_id);
    validate_subgraph_ports(p_subgraph, p_ports, p_subgraph_id);
    const SubgraphScope scope = make_subgraph_scope(alloc, p_subgraph_id, inner, p_ports);
    merge_subgraph_imports(*this, inner, p_subgraph_id);
    add_subgraph_declarations(*this, inner, scope);
    // Debug names nest under the subgraph's name, as ids nest under its id.
    const std::string name_prefix = p_subgraph.debug_name.empty() ? std::string(p_subgraph_id) : p_subgraph.debug_name;
    add_subgraph_debug_names(*this, inner, scope, name_prefix);

    RendSubgraphVK record(alloc, p_subgraph_id);
    record.debug_name = p_subgraph.debug_name;
    add_subgraph_tasks(*this, inner, scope, name_prefix, p_subgraph_id, record);
    add_nested_subgraphs(*this, inner, scope, name_prefix, p_subgraph_id);
    subgraphs.push_back(std::move(record));
    return subgraphs.back();
}

RendSubgraphVK& RendGraphDescVK::get_subgraph(std::string_view p_subgraph_id) {
    for (RendSubgraphVK& record : subgraphs) {
        if (record.subgraph_id == p_subgraph_id) {
            return record;
        }
    }
    throw std::runtime_error(std::format("Failed to get render graph subgraph: no subgraph has id '{}'.", p_subgraph_id));
}

RendGraphDescVK& RendGraphDescVK::add_resource(RendResVK p_resource) {
    if (p_resource.container != MEM_NULL) {
        RendResTID concrete_type_id = p_resource.container->get_resource_type_id();
        if (concrete_type_id == 0) {
            throw std::runtime_error("Failed to add render graph resource: container has no concrete resource type.");
        }
        if (p_resource.res_type_id != 0 && p_resource.res_type_id != concrete_type_id) {
            throw std::runtime_error("Failed to add render graph resource: container type does not match the declared type.");
        }
        p_resource.res_type_id = concrete_type_id;
    }
    resources.emplace_back(std::move(p_resource));
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, const IBindableRendResourceVK& p_resource) {
    p_resource.bind(*this, p_resource_id);
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_resource(std::string_view p_resource_id, Ref<IRendObjectContainer> p_container) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to bind render resource: resource id is empty.");
    }
    if (p_container == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to bind render resource '{}': container is null.", p_resource_id));
    }
    const RendResTID type_id = p_container->get_resource_type_id();
    return add_resource(RendResVK{
        .res_id = std::string(p_resource_id),
        .res_type_id = type_id,
        .container = std::move(p_container),
    });
}

bool RendGraphDescVK::contains_resource(std::string_view p_resource_id) const {
    const auto has_id = [p_resource_id](const auto& p_entries) {
        return std::ranges::any_of(p_entries, [p_resource_id](const auto& p_entry) {
            return p_entry.res_id == p_resource_id;
        });
    };
    return has_id(resources) || has_id(buffers) || has_id(images) || has_id(image_views) || has_id(samplers);
}

RendGraphDescVK& RendGraphDescVK::add_buffer(std::string_view p_resource_id, RendBufferFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph buffer: resource id is empty.");
    }
    if (p_factory == nullptr) {
        throw std::runtime_error("Failed to add render graph buffer: factory is null.");
    }
    buffers.push_back({.res_id = std::string(p_resource_id), .factory = std::move(p_factory)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image(std::string_view p_resource_id, RendImageFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph image: resource id is empty.");
    }
    if (p_factory == nullptr) {
        throw std::runtime_error("Failed to add render graph image: factory is null.");
    }
    images.push_back({.res_id = std::string(p_resource_id), .factory = std::move(p_factory)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_image_view(
    std::string_view p_resource_id, std::string_view p_image_id, RendImageViewFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph image view: resource id is empty.");
    }
    if (p_image_id.empty()) {
        throw std::runtime_error("Failed to add render graph image view: image id is empty.");
    }
    if (p_factory == nullptr) {
        p_factory = [](const RendGraphContextVK& /*p_context*/) {
            return RendImageViewDescVK{};
        };
    }
    image_views.push_back({.res_id = std::string(p_resource_id), .image_id = std::string(p_image_id), .factory = std::move(p_factory)});
    return *this;
}

RendGraphDescVK& RendGraphDescVK::set_debug_name(std::string_view p_resource_id, std::string_view p_debug_name) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to name render graph resource: resource id is empty.");
    }
    if (p_debug_name.empty()) {
        throw std::runtime_error(std::format("Failed to name render graph resource '{}': the debug name is empty.", p_resource_id));
    }
    if (!debug_names.emplace(std::string(p_resource_id), std::string(p_debug_name)).second) {
        throw std::runtime_error(std::format("Failed to name render graph resource '{}': it is already named.", p_resource_id));
    }
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_export(std::string_view p_export_name, std::string_view p_resource_id) {
    if (p_export_name.empty()) {
        throw std::runtime_error("Failed to export render graph resource: the export name is empty.");
    }
    if (p_resource_id.empty()) {
        throw std::runtime_error(std::format("Failed to export render graph resource as '{}': resource id is empty.", p_export_name));
    }
    if (!exports.emplace(std::string(p_export_name), std::string(p_resource_id)).second) {
        throw std::runtime_error(
            std::format("Failed to export render graph resource as '{}': the name is already exported.", p_export_name));
    }
    return *this;
}

RendGraphDescVK& RendGraphDescVK::add_sampler(std::string_view p_resource_id, RendSamplerFactoryVK p_factory) {
    if (p_resource_id.empty()) {
        throw std::runtime_error("Failed to add render graph sampler: resource id is empty.");
    }
    if (p_factory == nullptr) {
        p_factory = [](const RendGraphContextVK& /*p_context*/) {
            return RendSamplerDescVK{};
        };
    }
    samplers.push_back({.res_id = std::string(p_resource_id), .factory = std::move(p_factory)});
    return *this;
}

bool RendBufferDescVK::operator==(const RendBufferDescVK& p_other) const {
    return usage == p_other.usage && additional_usage == p_other.additional_usage && size == p_other.size &&
           domain == p_other.domain && per_frame == p_other.per_frame && init_data == p_other.init_data;
}

bool RendImageDescVK::operator==(const RendImageDescVK& p_other) const {
    return usage == p_other.usage && additional_usage == p_other.additional_usage && format == p_other.format &&
           extent.width == p_other.extent.width && extent.height == p_other.extent.height &&
           extent.depth == p_other.extent.depth && image_type == p_other.image_type && mip_levels == p_other.mip_levels &&
           layers == p_other.layers && per_frame == p_other.per_frame;
}

bool RendImageViewDescVK::operator==(const RendImageViewDescVK& p_other) const {
    return view_type == p_other.view_type && format == p_other.format && aspect_mask == p_other.aspect_mask &&
           base_mip_level == p_other.base_mip_level && mip_level_count == p_other.mip_level_count &&
           base_array_layer == p_other.base_array_layer && layer_count == p_other.layer_count;
}

bool RendSamplerDescVK::operator==(const RendSamplerDescVK& p_other) const {
    return min_filter == p_other.min_filter && mag_filter == p_other.mag_filter && mipmap_mode == p_other.mipmap_mode &&
           address_mode_u == p_other.address_mode_u && address_mode_v == p_other.address_mode_v &&
           address_mode_w == p_other.address_mode_w && mip_lod_bias == p_other.mip_lod_bias && max_anisotropy == p_other.max_anisotropy &&
           compare_op == p_other.compare_op && min_lod == p_other.min_lod && max_lod == p_other.max_lod &&
           border_color == p_other.border_color && unnormalized_coordinates == p_other.unnormalized_coordinates;
}

} // namespace WhiteBirdEngine
