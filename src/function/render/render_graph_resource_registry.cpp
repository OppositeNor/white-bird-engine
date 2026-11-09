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
#include "function/render/render_graph_resource_registry.hh"
#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_object.hh"
#include "function/render/i_resource_description.hh"
#include "function/render/i_resource_registrator.hh"
#include "function/render/render_defs.hh"
#include "utils/utils.hh"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

namespace {

RenderAllocator* validate_registry_allocator(RenderAllocator* p_allocator) {
    if (p_allocator == nullptr) {
        throw std::runtime_error("Failed to create render graph resource registry: the allocator is null.");
    }
    return p_allocator;
}

} // namespace

RenderGraphResourceRegistry::RenderGraphResourceRegistry(RenderAllocator* p_allocator, IRenderGraph* p_owner, ReleaseCallback p_release)
    : allocator(validate_registry_allocator(p_allocator)), owner(p_owner), release(std::move(p_release)), registrators(p_allocator),
      entries(p_allocator) {
    if (p_owner == nullptr) {
        throw std::runtime_error("Failed to create render graph resource registry: the owning graph is null.");
    }
}

void RenderGraphResourceRegistry::add_registrator(Unique<IResourceRegistrator> p_registrator) {
    if (p_registrator == MEM_NULL) {
        throw std::runtime_error("Failed to add resource registrator: the registrator is null.");
    }
    const TypeID description_type = p_registrator->get_description_type_uuid();
    if (registrators.contains(description_type)) {
        throw std::runtime_error(
            std::format("Failed to add resource registrator: description type {} already has a registrator.", description_type));
    }
    registrators.emplace(description_type, std::move(p_registrator));
}

bool RenderGraphResourceRegistry::has_registrator(TypeID p_description_type) const {
    return registrators.contains(p_description_type);
}

IResourceRegistrator& RenderGraphResourceRegistry::find_registrator(TypeID p_description_type, std::string_view p_key) {
    auto registrator = registrators.find(p_description_type);
    if (registrator == registrators.end()) {
        throw std::runtime_error(std::format(
            "Failed to register resource '{}': no registrator is installed for description type {}.", p_key, p_description_type));
    }
    return *registrator->second;
}

Ref<IRenderObject> RenderGraphResourceRegistry::register_resource(std::string_view p_key, const IResourceDescription& p_description) {
    if (p_key.empty()) {
        throw std::runtime_error("Failed to register a resource: the key is empty.");
    }
    IResourceRegistrator& registrator = find_registrator(p_description.get_type_uuid(), p_key);
    if (auto existing = entries.find(p_key); existing != entries.end() && existing->second.description->equals(p_description)) {
        return existing->second.object;
    }
    // Compile before touching the entries: the registrator may register or look up other keys.
    Ref<IRenderObject> object = registrator.compile(*owner, p_key, p_description);
    if (object == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to register resource '{}': its registrator compiled no object.", p_key));
    }
    Entry entry{
        .description = p_description.clone(allocator),
        .object = object,
    };
    if (auto existing = entries.find(p_key); existing != entries.end()) {
        Ref<IRenderObject> replaced = existing->second.object;
        existing->second = std::move(entry);
        if (replaced != object) {
            release_object(std::move(replaced));
        }
    } else {
        entries.emplace(std::string(p_key), std::move(entry));
    }
    return object;
}

Ref<IRenderObject> RenderGraphResourceRegistry::register_resource(
    std::string_view p_key, TypeID p_description_type, const DataValue& p_description) {
    if (p_key.empty()) {
        throw std::runtime_error("Failed to register a resource: the key is empty.");
    }
    Unique<IResourceDescription> description = find_registrator(p_description_type, p_key).deserialize_description(p_description, allocator);
    if (description == MEM_NULL) {
        throw std::runtime_error(std::format("Failed to register resource '{}': its registrator deserialized no description.", p_key));
    }
    if (description->get_type_uuid() != p_description_type) {
        throw std::runtime_error(
            std::format("Failed to register resource '{}': its registrator deserialized a description of another type.", p_key));
    }
    return register_resource(p_key, *description);
}

void RenderGraphResourceRegistry::unregister_resource(std::string_view p_key) {
    auto entry = entries.find(p_key);
    if (entry == entries.end()) {
        throw std::runtime_error(std::format("Failed to unregister resource '{}': the key is not registered.", p_key));
    }
    Ref<IRenderObject> object = entry->second.object;
    entries.erase(entry);
    release_object(std::move(object));
}

bool RenderGraphResourceRegistry::contains(std::string_view p_key) const {
    return entries.contains(p_key);
}

Ref<IRenderObject> RenderGraphResourceRegistry::try_get(std::string_view p_key) const {
    auto entry = entries.find(p_key);
    if (entry == entries.end()) {
        return MEM_NULL;
    }
    return entry->second.object;
}

const IResourceDescription& RenderGraphResourceRegistry::get_description(std::string_view p_key) const {
    auto entry = entries.find(p_key);
    if (entry == entries.end()) {
        throw std::runtime_error(std::format("Failed to get the description of resource '{}': the key is not registered.", p_key));
    }
    return *entry->second.description;
}

void RenderGraphResourceRegistry::release_object(Ref<IRenderObject> p_object) const {
    if (release != nullptr) {
        release(std::move(p_object));
    }
}

} // namespace WhiteBirdEngine
