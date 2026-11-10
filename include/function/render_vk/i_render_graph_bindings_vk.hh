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
#ifndef WBE_FILE_I_RENDER_GRAPH_BINDINGS_VK_HH
#define WBE_FILE_I_RENDER_GRAPH_BINDINGS_VK_HH

#include "core/data/data_value.hh"
#include "function/render/i_render_graph_bindings.hh"
#include "function/render_vk/rend_graph_recipe_builder_vk.hh"
#include "function/render_vk/rend_recipe_context_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @class RendGraphPreparedBindingsVK
 * @brief What Vulkan render graph bindings prepare for one build of their graph. Copyable and movable.
 */
struct RendGraphPreparedBindingsVK {
    /**
     * @brief The serialized recipe context, a map; nil for the renderer facts alone.
     *
     * Applications that add context entries serialize a WBE_REFLECT_SCRIPT struct derived from RendRecipeContextVK with
     * RendGraphScriptVK::serialize_context(). The graph replaces the RendRecipeContextVK entries with the authoritative
     * renderer facts, so only the entries the application adds are its own.
     */
    DataValue context;
    /**
     * @brief The resources and native tasks the recipe imports.
     *
     * The referenced resources must stay alive until the build completes; bindings usually own them, or borrow
     * application objects that outlive the graph. The renderer's swap-chain container is supplied by the graph when the
     * recipe imports it and the bindings do not.
     */
    RendGraphRecipeImportsVK imports;
};

/**
 * @class IRenderGraphBindingsVK
 * @brief Render graph bindings of the Vulkan renderer (see IRenderGraphBindings). Neither copyable nor movable.
 *
 * RendererVK::acquire_render_graph() accepts only bindings of this type. The graph calls prepare() for its first build and
 * for every rebuild (a context change, such as a swap chain resize, or RenderGraphVK::rebuild()), never on a frame whose
 * context is unchanged. Each call must supply everything the recipe imports for that build, creating the imported task
 * factories again; keep state that must survive rebuilds outside the tasks. Called on the rendering thread.
 */
class IRenderGraphBindingsVK : public IRenderGraphBindings {
public:
    virtual ~IRenderGraphBindingsVK() override = default;
    IRenderGraphBindingsVK(const IRenderGraphBindingsVK&) = delete;
    IRenderGraphBindingsVK(IRenderGraphBindingsVK&&) noexcept = delete;
    IRenderGraphBindingsVK& operator=(const IRenderGraphBindingsVK&) = delete;
    IRenderGraphBindingsVK& operator=(IRenderGraphBindingsVK&&) noexcept = delete;

    /**
     * @brief Prepare the recipe context and imports of one build.
     *
     * @param p_render_context The render data of the graph's renderer.
     * @param p_recipe_context The authoritative renderer facts of the build: the frames in flight, and the swap chain
     * format and extent the graph is built for.
     * @return The prepared context and imports.
     * @throws std::runtime_error If the bindings cannot supply the build; the graph does not catch it.
     */
    virtual RendGraphPreparedBindingsVK prepare(const RenderContextVK& p_render_context, const RendRecipeContextVK& p_recipe_context) = 0;

protected:
    /**
     * @brief Construct bindings for an instance; see IRenderGraphBindings::IRenderGraphBindings().
     */
    explicit IRenderGraphBindingsVK(std::string_view p_instance_key) : IRenderGraphBindings(p_instance_key) {
    }
};

} // namespace WhiteBirdEngine

#endif
