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
#include "function/scene/scene.hh"
#include "core/allocator/i_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/ecs/systems.hh"
#include "function/render/i_renderer.hh"
#include "function/render/global_render_cache.hh"
#include "function/render/i_render_graph.hh"
#include "function/render/i_render_graph_bindings.hh"
#include "function/render/render_graph_instance_cache.hh"
#include "function/render/render_resource_unit.hh"
#include "function/scene/world.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <chrono>
#include <cstddef>
#include <format>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

Scene::Scene(HeapAllocatorDefault* p_allocator, Ref<IRenderer> p_renderer, std::string_view p_resource_manifest_key)
    : allocator(p_allocator), renderer(std::move(p_renderer)), worlds(p_allocator), resource_manifest_key(p_allocator),
      render_graph_bindings(p_allocator), pending_render_operations(p_allocator) {
    if (allocator == nullptr) {
        throw std::runtime_error("Scene allocator cannot be null.");
    }
    if (renderer == MEM_NULL) {
        throw std::runtime_error("Scene renderer cannot be null.");
    }
    resource_manifest_key.assign(p_resource_manifest_key);
    render_state = make_ref<RenderState>(allocator, allocator);
}

Scene::~Scene() = default;

void Scene::add_world(Ref<World> p_world) {
    if (lifecycle_state != LifecycleState::CONSTRUCTED) {
        throw std::runtime_error("Worlds can only be added before scene setup.");
    }
    if (p_world == MEM_NULL) {
        throw std::runtime_error("Scene world cannot be null.");
    }
    worlds.push_back(std::move(p_world));
}

Ref<World> Scene::get_world(size_t p_world_index) const {
    if (p_world_index >= worlds.size()) {
        throw std::runtime_error("Scene world index is out of range.");
    }
    return worlds[p_world_index];
}

void Scene::set_active_world(size_t p_world_index) {
    if (p_world_index >= worlds.size()) {
        throw std::runtime_error("Scene active world index is out of range.");
    }
    active_world_index = p_world_index;
}

Ref<World> Scene::get_active_world() const {
    if (worlds.empty()) {
        return Ref<World>();
    }
    return worlds[active_world_index];
}

void Scene::add_render_graph_bindings(std::string_view p_resource_key, Ref<IRenderGraphBindings> p_bindings) {
    if (lifecycle_state != LifecycleState::CONSTRUCTED && lifecycle_state != LifecycleState::TORN_DOWN) {
        throw std::runtime_error("Render graph bindings can only be added before scene setup.");
    }
    if (p_resource_key.empty()) {
        throw std::runtime_error("Scene render graph bindings require a definition key.");
    }
    if (p_bindings == MEM_NULL) {
        throw std::runtime_error(std::format("Scene render graph bindings of '{}' cannot be null.", p_resource_key));
    }
    std::string key = RenderGraphInstanceCache::make_cache_key(p_resource_key, p_bindings->get_instance_key());
    if (render_graph_bindings.contains(key)) {
        throw std::runtime_error(std::format(
            "Scene render graph '{}' instance '{}' already has bindings.", p_resource_key, p_bindings->get_instance_key()));
    }
    render_graph_bindings.emplace(std::move(key), std::move(p_bindings));
}

void Scene::setup() {
    if (lifecycle_state != LifecycleState::CONSTRUCTED && lifecycle_state != LifecycleState::TORN_DOWN) {
        throw std::runtime_error("Scene setup requires a constructed or torn-down scene.");
    }
    observe_render_operations(false);
    // Every instance needs bindings; check them all before queueing any acquisition.
    for (size_t i = 0; i < worlds.size(); ++i) {
        const WorldGraph world_graph = get_world_graph(i);
        if (!world_graph.resource_key.empty() && !render_graph_bindings.contains(world_graph.instance_cache_key)) {
            throw std::runtime_error(std::format("Scene setup requires bindings for render graph '{}' instance '{}'.",
                world_graph.resource_key,
                worlds[i]->get_render_graph_instance_key()));
        }
    }
    for (size_t i = 0; i < worlds.size(); ++i) {
        const WorldGraph world_graph = get_world_graph(i);
        bool first_use = !world_graph.resource_key.empty();
        for (size_t j = 0; first_use && j < i; ++j) {
            first_use = get_world_graph(j).instance_cache_key != world_graph.instance_cache_key;
        }
        if (first_use) {
            Ref<IRenderGraphBindings> bindings = render_graph_bindings.find(world_graph.instance_cache_key)->second;
            queue_render_operation([state = render_state,
                                       target_renderer = renderer.get(),
                                       resource_key = world_graph.resource_key,
                                       instance_cache_key = world_graph.instance_cache_key,
                                       bindings = std::move(bindings)] mutable {
                state->graphs.insert_or_assign(instance_cache_key, target_renderer->acquire_render_graph(resource_key, bindings));
            });
        }
        system_driver_setup(*worlds[i]);
    }
    lifecycle_state = LifecycleState::SETUP;
}

void Scene::ready() {
    if (lifecycle_state != LifecycleState::SETUP) {
        throw std::runtime_error("Scene ready requires setup to complete first.");
    }
    for (auto& world : worlds) {
        system_driver_ready(*world);
    }
    lifecycle_state = LifecycleState::READY;
}

void Scene::update(double p_delta) {
    if (lifecycle_state != LifecycleState::READY) {
        throw std::runtime_error("Scene update requires a ready scene.");
    }
    observe_render_operations(false);

    if (!worlds.empty()) {
        const WorldGraph world_graph = get_world_graph(active_world_index);
        if (!world_graph.resource_key.empty() && world_graph.instance_cache_key != activated_instance) {
            queue_render_operation([state = render_state, target_renderer = renderer.get(), instance_cache_key = world_graph.instance_cache_key] mutable {
                auto graph = state->graphs.find(instance_cache_key);
                if (graph == state->graphs.end()) {
                    throw std::runtime_error("Failed to activate a scene render graph: it was not acquired.");
                }
                target_renderer->activate_render_graph(graph->second);
            });
            activated_instance = world_graph.instance_cache_key;
        }
    }

    for (auto& world : worlds) {
        system_driver_update(*world, p_delta);
    }
}

void Scene::teardown() {
    if (lifecycle_state != LifecycleState::SETUP && lifecycle_state != LifecycleState::READY) {
        throw std::runtime_error("Scene teardown requires a setup or ready scene.");
    }
    for (size_t i = worlds.size(); i > 0; --i) {
        system_driver_teardown(*worlds[i - 1]);
    }
    // The renderer keeps an activated graph alive; the scene only drops its own handles.
    queue_render_operation([state = render_state] mutable {
        state->graphs.clear();
    });
    activated_instance.clear();
    lifecycle_state = LifecycleState::TORN_DOWN;
    observe_render_operations(true);
}

Scene::WorldGraph Scene::get_world_graph(size_t p_world_index) const {
    const Ref<World>& world = worlds[p_world_index];
    const std::string_view resource_key = world->get_render_graph_resource_key();
    if (resource_key.empty()) {
        return {};
    }
    return WorldGraph{
        .resource_key = std::string(resource_key),
        .instance_cache_key = RenderGraphInstanceCache::make_cache_key(resource_key, world->get_render_graph_instance_key()),
    };
}

void Scene::queue_render_operation(std::function<void()> p_operation) {
    std::optional<std::future<void>> future = renderer->try_add_job(p_operation);
    if (!future.has_value()) {
        // The renderer stopped accepting jobs, so its rendering thread has stopped and this thread may run the operation.
        p_operation();
        return;
    }
    pending_render_operations.push_back(std::move(*future));
}

void Scene::observe_render_operations(bool p_wait) {
    // Operations complete in queue order; the first failure is rethrown.
    while (!pending_render_operations.empty()) {
        std::future<void>& front = pending_render_operations.front();
        if (!p_wait && front.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return;
        }
        std::future<void> completed = std::move(front);
        pending_render_operations.erase(pending_render_operations.begin());
        completed.get();
    }
}

} // namespace WhiteBirdEngine