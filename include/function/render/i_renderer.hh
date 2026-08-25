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
#ifndef WBE_FILE_RENDERER_HH
#define WBE_FILE_RENDERER_HH

#include "core/job/job_handler.hh"
#include "core/memory/reference_strong.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render/render_database.hh"
#include "function/render/render_graph_resource_binding.hh"
#include "utils/interface/i_singleton.hh"
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace WhiteBirdEngine {

WBE_LABEL(WBE_CHANNEL_RENDER, WBE_CHANNEL);

/**
 * @struct RendererCapabilities
 * @brief Optional renderer capabilities requested by the caller and enabled by the renderer.
 */
struct RendererCapabilities {
    /**
     * @brief Whether ray tracing is requested and was enabled.
     */
    bool ray_tracing = false;
};

/**
 * @class IRenderer
 * @brief IRenderer interface for different types of renderers, usually refer
 * to the rendering API.
 *
 */
class IRenderer : public ISingleton<IRenderer>, public JobHandler {
public:
    IRenderer() = default;
    virtual ~IRenderer() override = default;
    IRenderer(const IRenderer&) = delete;
    IRenderer(IRenderer&&) noexcept = delete;
    IRenderer& operator=(const IRenderer&) = delete;
    IRenderer& operator=(IRenderer&&) noexcept = delete;

    /**
     * @brief Called every frame.
     *
     * @param p_delta The difference between the last two calls of this
     * function.
     */
    virtual void tick(double p_delta) = 0;

    /**
     * @brief Get number of workers working on rendering.
     *
     * @return The number of workers working on rendering.
     */
    virtual uint32_t get_worker_nums() = 0;

    /**
     * @brief Get the instance of the render database.
     *
     * @return The renderer's render database.
     */
    virtual Ref<RenderDatabase> get_render_database() = 0;

    /**
     * @brief Acquire a registered render graph by resource key.
     *
     * The concrete graph descriptor must already be available to the backend.
     * Repeated registrations acquire additional references to the same
     * compiled graph.
     *
     * @param p_resource_key The render graph resource key.
     */
    virtual void register_render_graph(std::string_view p_resource_key) = 0;

    /**
     * @brief Release a registered render graph by resource key.
     *
     * @param p_resource_key The render graph resource key.
     */
    virtual void unregister_render_graph(std::string_view p_resource_key) = 0;

    /**
     * @brief Activate a render graph.
     *
     * @param p_resource_key The resource key of the graph to activate.
     */
    virtual void activate_render_graph(std::string_view p_resource_key) = 0;

    /**
     * @brief Apply transient named resources to a registered graph.
     *
     * Overrides remain active until that graph completes one frame, then its
     * descriptor defaults are restored.
     *
     * @param p_resource_key The registered render graph resource key.
     * @param p_bindings The contiguous binding array, or nullptr when p_binding_count is zero.
     * @param p_binding_count Number of entries in the binding array.
     */
    virtual void update_render_graph_resources(
        std::string_view p_resource_key, const RenderGraphResourceBinding* p_bindings, size_t p_binding_count) = 0;

    /**
     * @brief Update the static opaque triangles data.
     */
    virtual void update_static_opaque_triangles() = 0;
};

} // namespace WhiteBirdEngine
#endif
