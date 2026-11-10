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
#ifndef WBE_FILE_SCENE_HH
#define WBE_FILE_SCENE_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/core_utils.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/i_render_graph.hh"
#include "function/render/i_render_graph_bindings.hh"
#include "function/render/render_resource_unit.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <string>
#include <string_view>

namespace WhiteBirdEngine {

class IRenderer;
class World;

/**
 * @class Scene
 * @brief Orchestrates the lifecycle and active render state of a set of worlds.
 *
 * Each world names a render graph definition and an instance key (World::get_render_graph_resource_key() and
 * World::get_render_graph_instance_key()). Setup acquires one graph instance per distinct definition and instance key, with
 * the bindings supplied for it (add_render_graph_bindings()), so the worlds sharing a pair share its instance; scenes share
 * an instance only when they supply the same bindings object. The scene owns the graph handles in a rendering-thread state
 * that it never touches directly: acquisitions, activations, and releases are queued through IRenderer::add_job() in
 * order, and their futures are observed so that their failures are rethrown by the next update(), teardown(), or setup().
 * Teardown releases the scene's graph handles and waits for the release, but leaves the renderer's activation, which keeps
 * the active graph alive until the application activates another graph or clears it. When the renderer no longer accepts
 * jobs (its rendering thread has stopped), the scene runs the operations itself.
 *
 * The allocator must outlive the scene. The scene retains strong references to its renderer, worlds, and bindings and is
 * neither copyable nor movable.
 */
class Scene {
public:
    /**
     * @brief Construct an empty scene.
     *
     * @param p_allocator Allocator used by scene-owned containers and strings.
     * @param p_renderer Renderer receiving graph lifecycle and world render state.
     * @param p_resource_manifest_key Resource manifest associated with this scene.
     */
    Scene(HeapAllocatorDefault* p_allocator, Ref<IRenderer> p_renderer, std::string_view p_resource_manifest_key = {});
    ~Scene();
    Scene(const Scene&) = delete;
    Scene(Scene&&) noexcept = delete;
    Scene& operator=(const Scene&) = delete;
    Scene& operator=(Scene&&) noexcept = delete;

    /**
     * @brief Add a world before scene setup.
     *
     * @param p_world The world to add to the scene.
     */
    void add_world(Ref<World> p_world);

    /**
     * @brief Get the number of worlds owned by this scene.
     *
     * @return The number of worlds in the scene.
     */
    size_t get_world_count() const {
        return worlds.size();
    }

    /**
     * @brief Get a world by index.
     *
     * @param p_world_index The zero-based world index.
     * @return The world at the requested index.
     */
    Ref<World> get_world(size_t p_world_index) const;

    /**
     * @brief Select the world whose graph and static mesh are published by update.
     *
     * @param p_world_index The zero-based index of the world to select.
     */
    void set_active_world(size_t p_world_index);

    /**
     * @brief Get the selected world, or null when this scene has no worlds.
     *
     * @return The selected world, or null when the scene has no worlds.
     */
    Ref<World> get_active_world() const;

    /**
     * @brief Get the scene resource manifest key.
     *
     * @return The resource-root-relative manifest key associated with this scene.
     */
    std::string_view get_resource_manifest_key() const {
        return resource_manifest_key;
    }

    /**
     * @brief Supply the bindings of a render graph instance, before setup.
     *
     * The bindings serve every world whose definition key is p_resource_key and whose instance key is the bindings' instance
     * key.
     *
     * @param p_resource_key The render graph definition key.
     * @param p_bindings The bindings, of the renderer's bindings type.
     * @throws std::runtime_error If the scene is set up, p_resource_key is empty, p_bindings is null, or the definition and
     * instance key already have bindings.
     */
    void add_render_graph_bindings(std::string_view p_resource_key, Ref<IRenderGraphBindings> p_bindings);

    /**
     * @brief Queue the acquisition of every world's render graph instance and run every driver setup callback.
     *
     * @throws std::runtime_error If the scene is neither constructed nor torn down, a world's definition and instance key
     * have no bindings, or an earlier queued render operation failed.
     */
    void setup();

    /**
     * @brief Run every driver ready callback after setup completes.
     */
    void ready();

    /**
     * @brief Publish active-world render state and update all world drivers.
     *
     * Queues the activation of the active world's graph instance when it changed since the last activation; a world without
     * a graph activates nothing.
     *
     * @param p_delta The elapsed time since the previous update, in seconds.
     * @throws std::runtime_error If the scene is not ready, or a queued render operation failed.
     */
    void update(double p_delta);

    /**
     * @brief Run driver teardown, then release the scene's graph instances and wait for the release.
     *
     * @throws std::runtime_error If the scene is neither set up nor ready, or a queued render operation failed.
     */
    void teardown();

private:
    enum class LifecycleState : uint8_t {
        CONSTRUCTED,
        SETUP,
        READY,
        TORN_DOWN,
    };

    // The rendering-thread state of the scene; only queued render operations access it.
    struct RenderState {
        explicit RenderState(HeapAllocatorDefault* p_allocator) : graphs(p_allocator) {
        }
        // The acquired instances, keyed by RenderGraphInstanceCache::make_cache_key().
        Map<std::string, Ref<IRenderGraph>, HeapAllocatorDefault> graphs;
    };

    // The definition key of a world's graph, or empty, and the cache key of its instance.
    struct WorldGraph {
        std::string resource_key;
        std::string instance_cache_key;
    };

    WorldGraph get_world_graph(size_t p_world_index) const;
    void queue_render_operation(std::function<void()> p_operation);
    void observe_render_operations(bool p_wait);

    HeapAllocatorDefault* allocator;
    Ref<IRenderer> renderer;
    Vector<Ref<World>, HeapAllocatorDefault> worlds;
    String<HeapAllocatorDefault> resource_manifest_key;
    // Bindings supplied with add_render_graph_bindings(), keyed by RenderGraphInstanceCache::make_cache_key().
    Map<std::string, Ref<IRenderGraphBindings>, HeapAllocatorDefault> render_graph_bindings;
    Ref<RenderState> render_state;
    // Futures of the queued render operations that have not been observed yet.
    Vector<std::future<void>, HeapAllocatorDefault> pending_render_operations;
    // The cache key of the instance last queued for activation, or empty.
    std::string activated_instance;
    size_t active_world_index = 0;
    LifecycleState lifecycle_state = LifecycleState::CONSTRUCTED;
};

} // namespace WhiteBirdEngine

#endif
