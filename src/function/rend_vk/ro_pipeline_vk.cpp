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
#include "function/rend_vk/ro_pipeline_vk.hh"
#include "core/core_utils.hh"
#include "function/render/i_rend_object.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/shader_metadata_vk_utils.hh"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROPipelineVK::ROPipelineVK(const RendContextVK* p_rend_data) : rend_data(WBE_REQUIRES_VALID(p_rend_data)) {
}

ROPipelineVK::~ROPipelineVK() {
    if (pipeline == VK_NULL_HANDLE) {
        return;
    }
    rend_data->register_delayed_call(
        [rend_data = rend_data, pipeline = pipeline]() {
            vkDestroyPipeline(rend_data->logic_device, pipeline, rend_data->allocation_callbacks);
        },
        rend_data->frames_in_flight);
}

void ROPipelineVK::set_desc_layout(const ShaderDescHeapLayoutVK& p_layout) {
    if (p_layout.size > rend_data->desc_heap_properties.maxPushDataSize) {
        throw std::runtime_error("Failed to set pipeline descriptor layout: reflected push data exceeds the device limit.");
    }
    desc_layout = p_layout;
}

} // namespace WhiteBirdEngine
