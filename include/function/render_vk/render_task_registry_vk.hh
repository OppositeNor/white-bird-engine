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
#ifndef WBE_FILE_RENDER_TASK_REGISTRY_VK_HH
#define WBE_FILE_RENDER_TASK_REGISTRY_VK_HH

#include "core/parser/parser_json.hh"
#include "core/parser/parser_toml.hh"
#include "core/script/script_value.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include <concepts>
#include <span>
#include <string_view>

namespace WhiteBirdEngine {

/**
 * @brief The parser data types RenderTaskRegistryVK accepts task parameters as: JSONData, TOMLData, and ScriptValue.
 *
 * The registry is explicitly instantiated for exactly these types; add a type here and in render_task_registry.cpp.jinja
 * together.
 */
template <typename TParserData>
concept RenderTaskParamsDataConcept =
    std::same_as<TParserData, JSONData> || std::same_as<TParserData, TOMLData> || std::same_as<TParserData, ScriptValue>;

/**
 * @class RenderTaskRegistryVK
 * @brief Creates render-task factories from a task type name and parser data parameters.
 *
 * The registry is generated from every class annotated with WBE_META(WBE_RENDER_TASK). A task type is registered under its
 * class name and under each WBE_ALIASES entry; a name used twice fails the build. Every such class must declare
 *
 * @code
 * WBE_META(WBE_REFLECT)
 * static Unique<IRenderTaskVK> create(const BaseRenderDataVK* p_render_data, const TParams& p_params,
 *     const RendGraphContextVK& p_context);
 * @endcode
 *
 * where TParams is a copyable reflected struct (for example WBE_SERIALIZABLE_STATIC). A missing create() or a different
 * signature fails the build. The registry has no state; its methods may be called from any thread.
 */
class RenderTaskRegistryVK final {
public:
    RenderTaskRegistryVK() = delete;
    ~RenderTaskRegistryVK() = delete;
    RenderTaskRegistryVK(const RenderTaskRegistryVK&) = delete;
    RenderTaskRegistryVK(RenderTaskRegistryVK&&) noexcept = delete;
    RenderTaskRegistryVK& operator=(const RenderTaskRegistryVK&) = delete;
    RenderTaskRegistryVK& operator=(RenderTaskRegistryVK&&) noexcept = delete;

    /**
     * @brief Create the factory of one render task.
     *
     * The parameters are deserialized into the task type's TParams immediately, so invalid parameters fail here rather than
     * during graph construction. The factory keeps a copy of them and the render-data pointer, and calls the task type's
     * create() every time the render graph evaluates it.
     *
     * @tparam TParserData The parser data type of the parameters; see RenderTaskParamsDataConcept.
     * @param p_task_type A registered task type name.
     * @param p_params The task parameters, deserialized through SerializableSD into the task type's TParams.
     * @param p_render_data Render data passed to create(); it must outlive the factory.
     * @return The factory, suitable for RendTaskDescVK::factory.
     * @throws std::runtime_error If the task type is unknown, p_render_data is null, or the parameters cannot be
     * deserialized.
     */
    template <RenderTaskParamsDataConcept TParserData>
    static RendTaskFactoryVK create_factory(std::string_view p_task_type, const TParserData& p_params, const BaseRenderDataVK* p_render_data);

    /**
     * @brief Check whether a task type name is registered.
     */
    static bool contains(std::string_view p_task_type);

    /**
     * @brief Get every registered task type name: class names and aliases.
     */
    static std::span<const std::string_view> get_task_types();
};

} // namespace WhiteBirdEngine

#endif
