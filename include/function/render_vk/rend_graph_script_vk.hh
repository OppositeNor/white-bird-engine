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
#ifndef WBE_FILE_REND_GRAPH_SCRIPT_VK_HH
#define WBE_FILE_REND_GRAPH_SCRIPT_VK_HH

#include "core/data/data_value.hh"
#include "core/memory/reference_strong.hh"
#include "function/render_vk/rend_graph_recipe_builder_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <string_view>

namespace WhiteBirdEngine {

class RendererVK;

/**
 * @class RendGraphScriptVK
 * @brief Runs render graph recipe scripts: Lua modules exporting `build(context)`, which returns a RendGraphRecipeVK.
 *
 * A recipe script is a `script` resource (RUScript). It runs in a fresh SBLua with the default ScriptSandboxConfig, so
 * scripts share no state and can only describe a graph; the recipe is built by RendGraphRecipeBuilderVK. `context` is the
 * map make_context() returns. Stateless.
 */
class RendGraphScriptVK final {
public:
    RendGraphScriptVK() = delete;
    ~RendGraphScriptVK() = delete;
    RendGraphScriptVK(const RendGraphScriptVK&) = delete;
    RendGraphScriptVK(RendGraphScriptVK&&) noexcept = delete;
    RendGraphScriptVK& operator=(const RendGraphScriptVK&) = delete;
    RendGraphScriptVK& operator=(RendGraphScriptVK&&) noexcept = delete;

    /**
     * @brief Get the context a recipe's `build(context)` receives.
     *
     * A map of renderer facts that do not change while a graph lives: `frames_in_flight` (integer) and
     * `swap_chain_format` (an ImageFormatVK name, or the VkFormat value when ImageFormatVK has no name for it). Sizes that
     * follow the swap chain are expressed in the recipe with RendExtent2DParamsVK instead.
     */
    static DataValue make_context(const BaseRenderDataVK& p_render_data);

    /**
     * @brief Run a recipe script's `build(context)`.
     *
     * The script can `require` other `script` resources by key (see SMRResourceManager), such as the helpers under
     * res/assets/scripts/lua/utils.
     *
     * @param p_module_name The name the script is loaded under, used in script error messages.
     * @param p_source The Lua source of the module.
     * @param p_context The argument passed to `build`.
     * @return The recipe.
     * @throws std::runtime_error If the source fails to load, exports no `build` function, or `build` fails.
     */
    static DataValue run(std::string_view p_module_name, std::string_view p_source, const DataValue& p_context);

    /**
     * @brief Run the recipe script stored as the `script` resource p_script_key; see run().
     * @throws std::runtime_error If no script resource has the key, or under the conditions of run().
     */
    static DataValue run(std::string_view p_script_key, const DataValue& p_context);

    /**
     * @brief Run a recipe script, build its graph descriptor, and register it with the renderer.
     *
     * Call on the rendering thread.
     *
     * @param p_renderer The renderer to register the graph with.
     * @param p_graph_key The key the graph is registered under.
     * @param p_script_key The `script` resource key of the recipe.
     * @param p_imports The resources the recipe imports.
     * @return The registered graph.
     * @throws std::runtime_error Under the conditions of run(), RendGraphRecipeBuilderVK::build(), and
     * RendererVK::register_render_graph().
     */
    static Ref<RenderGraphVK> register_render_graph(
        RendererVK& p_renderer, std::string_view p_graph_key, std::string_view p_script_key, const RendGraphRecipeImportsVK& p_imports);
};

} // namespace WhiteBirdEngine

#endif
