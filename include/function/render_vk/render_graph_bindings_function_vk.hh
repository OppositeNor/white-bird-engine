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
#ifndef WBE_FILE_RENDER_GRAPH_BINDINGS_FUNCTION_VK_HH
#define WBE_FILE_RENDER_GRAPH_BINDINGS_FUNCTION_VK_HH

#include "function/render_vk/i_render_graph_bindings_vk.hh"
#include "function/render_vk/rend_recipe_context_vk.hh"
#include "function/render_vk/render_context_vk.hh"
#include <functional>
#include <string_view>
#include <utility>

namespace WhiteBirdEngine {

/**
 * @class RenderGraphBindingsFunctionVK
 * @brief Vulkan render graph bindings whose prepare() calls a function. Neither copyable nor movable.
 *
 * Without a function, the bindings supply the renderer facts alone and import nothing (the graph still supplies the swap
 * chain), which suits recipes that need no application resources. The function's captures follow the bindings' lifetime
 * rules: the graph retains the bindings, so captures must not retain the graph, and borrowed objects must outlive it.
 */
class RenderGraphBindingsFunctionVK final : public IRenderGraphBindingsVK {
public:
    /**
     * @brief Prepares one build; see IRenderGraphBindingsVK::prepare().
     */
    using PrepareFunction = std::function<RendGraphPreparedBindingsVK(const RenderContextVK&, const RendRecipeContextVK&)>;

    /**
     * @brief Construct the bindings of an instance.
     * @param p_instance_key The instance key; see IRenderGraphBindings.
     * @param p_prepare The function, or empty to supply nothing.
     * @throws std::runtime_error If p_instance_key is empty.
     */
    explicit RenderGraphBindingsFunctionVK(std::string_view p_instance_key, PrepareFunction p_prepare = {})
        : IRenderGraphBindingsVK(p_instance_key), prepare_function(std::move(p_prepare)) {
    }
    virtual ~RenderGraphBindingsFunctionVK() override = default;
    RenderGraphBindingsFunctionVK(const RenderGraphBindingsFunctionVK&) = delete;
    RenderGraphBindingsFunctionVK(RenderGraphBindingsFunctionVK&&) noexcept = delete;
    RenderGraphBindingsFunctionVK& operator=(const RenderGraphBindingsFunctionVK&) = delete;
    RenderGraphBindingsFunctionVK& operator=(RenderGraphBindingsFunctionVK&&) noexcept = delete;

    virtual RendGraphPreparedBindingsVK prepare(const RenderContextVK& p_render_context, const RendRecipeContextVK& p_recipe_context) override {
        if (!prepare_function) {
            return {};
        }
        return prepare_function(p_render_context, p_recipe_context);
    }

private:
    PrepareFunction prepare_function;
};

} // namespace WhiteBirdEngine

#endif
