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
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/renderer_vk.hh"
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

ROPipelineLayoutVK::ROPipelineLayoutVK(const RendererVK::BaseRenderDataVK* p_render_data)
    : render_data(p_render_data) {
    (void)render_data;
    // TODO
}

ROPipelineLayoutVK::~ROPipelineLayoutVK() {
    // TODO
}

}
