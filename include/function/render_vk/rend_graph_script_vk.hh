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
#include "function/render_vk/render_graph_context_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <string_view>

namespace WhiteBirdEngine {

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
     * A map of renderer facts: `frames_in_flight` (integer), `swap_chain_format` (an ImageFormatVK name, or the VkFormat
     * value when ImageFormatVK has no name for it), and `swap_chain_extent` (`{ width, height }` of p_graph_context).
     * A graph whose rebuild callback (RendererVK::register_render_graph()) runs the recipe re-runs it with a new
     * context whenever the graph context changes, so recipes may size resources and dispatches from the extent.
     *
     * @param p_render_data The render data of the renderer.
     * @param p_graph_context The context the graph is being built for.
     */
    static DataValue make_context(const BaseRenderDataVK& p_render_data, const RendGraphContextVK& p_graph_context);

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
};

} // namespace WhiteBirdEngine

#endif
