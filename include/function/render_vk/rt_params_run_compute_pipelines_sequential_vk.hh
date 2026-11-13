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
#ifndef WBE_FILE_RT_PARAMS_RUN_COMPUTE_PIPELINES_SEQUENTIAL_VK_HH
#define WBE_FILE_RT_PARAMS_RUN_COMPUTE_PIPELINES_SEQUENTIAL_VK_HH

#include "core/data/data_value.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include <cstdint>
#include <string>
#include <vector>

namespace WhiteBirdEngine {

/**
 * @class RTParamsComputeDispatchVK
 * @brief One dispatch of RTParamsRunComputePipelinesSequentialVK. Copyable and movable.
 *
 * Deserialization is strict: an unknown key, such as a misspelt parameter, throws instead of being ignored.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT) RTParamsComputeDispatchVK {
    /**
     * @brief Registration key of the compute pipeline on the task's render graph (IRenderGraph::register_resource()).
     */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED, WBE_RECIPE_RESOURCE(registered, ROComputePipelineVK))
    std::string pipeline;
    /**
     * @brief Resource id of the buffer holding the dispatch's VkDispatchIndirectCommand, resolved through the task's
     * aliases; empty for a direct dispatch, which uses the group counts.
     */
    WBE_META(WBE_REFLECT, WBE_RECIPE_RESOURCE)
    std::string indirect_buffer;
    /**
     * @brief Byte offset of the command in the indirect buffer; a multiple of 4, and 0 for a direct dispatch.
     */
    WBE_META(WBE_REFLECT)
    uint64_t indirect_offset = 0;
    /**
     * @brief Bindings of the dispatch's pipeline: a map from a resource id the pipeline reflects to the resource id it
     * binds, resolved through the task's aliases. Lets dispatches of one task bind one shader resource to different
     * resources.
     */
    WBE_META(WBE_REFLECT, WBE_RECIPE_RESOURCE)
    DataValue bindings;
    /**
     * @brief Dispatch workgroup count along x passed to vkCmdDispatch.
     */
    WBE_META(WBE_REFLECT)
    uint32_t group_count_x = 1;
    /**
     * @brief Dispatch workgroup count along y passed to vkCmdDispatch.
     */
    WBE_META(WBE_REFLECT)
    uint32_t group_count_y = 1;
    /**
     * @brief Dispatch workgroup count along z passed to vkCmdDispatch.
     */
    WBE_META(WBE_REFLECT)
    uint32_t group_count_z = 1;
    /**
     * @brief Custom wbe::push_constant values: a map from identifier to number, typed as the pipeline declares each one.
     */
    WBE_META(WBE_REFLECT)
    DataValue push_constants;
};

/**
 * @class RTParamsRunComputePipelinesSequentialVK
 * @brief Recipe parameters of RTRunComputePipelinesSequentialVK. Copyable and movable.
 *
 * Deserialization is strict: an unknown key, such as a misspelt parameter, throws instead of being ignored.
 */
struct WBE_META(WBE_SERIALIZABLE_STATIC, WBE_STRICT_DESERIALIZATION, WBE_REFLECT_SCRIPT) RTParamsRunComputePipelinesSequentialVK {
    /**
     * @brief Dispatches recorded in order; at least one.
     */
    WBE_META(WBE_REFLECT, WBE_REFLECT_REQUIRED)
    std::vector<RTParamsComputeDispatchVK> dispatches;
    /**
     * @brief Queue the dispatches are submitted to: a QueueFamilyVK name or alias, such as "compute_1".
     */
    WBE_META(WBE_REFLECT)
    QueueFamilyVK queue_family = QueueFamilyVK::GRAPHICS;
};

} // namespace WhiteBirdEngine

#endif
