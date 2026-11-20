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
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "function/rend_vk/ro_image_vk.hh"
#include "function/rend_vk/ro_image_view_vk.hh"
#include "function/rend_vk/ro_sampler_vk.hh"
#include "function/rend_vk/ro_staging_buffer_vk.hh"
#include "function/rend_vk/ro_shader_vk.hh"
#include "function/rend_vk/ro_graphics_pipeline_vk.hh"
#include "function/rend_vk/ro_compute_pipeline_vk.hh"
#include <stdexcept>

namespace WhiteBirdEngine {

RendContextVK::RendContextVK(RendAlloc* p_alloc) : alloc(p_alloc), queue_family_indices(p_alloc) {
    if (p_alloc == nullptr) {
        throw std::runtime_error("Failed to construct render context: allocator is null.");
    }
}

Ref<ROBufferVK> RendContextVK::create_buffer(const BufferDescVK& p_description) const {
    return make_ref<ROBufferVK>(alloc, this, p_description);
}

Ref<ROBufferVK> RendContextVK::create_buffer(const BufferAliasDescVK& p_description) const {
    return make_ref<ROBufferVK>(alloc, this, p_description);
}

Ref<ROImageVK> RendContextVK::create_image(const ImageDescVK& p_description) const {
    return make_ref<ROImageVK>(alloc, this, p_description);
}

Ref<ROImageVK> RendContextVK::create_image(const ImageAliasDescVK& p_description) const {
    return make_ref<ROImageVK>(alloc, this, p_description);
}

Ref<ROImageViewVK> RendContextVK::create_image_view(const ImageViewDescVK& p_description) const {
    return make_ref<ROImageViewVK>(alloc, this, p_description);
}

Ref<ROSamplerVK> RendContextVK::create_sampler(const SamplerDescVK& p_description) const {
    return make_ref<ROSamplerVK>(alloc, this, p_description);
}

Ref<ROStagingBufferVK> RendContextVK::create_staging_buffer(const StagingBufferDescVK& p_description) const {
    return make_ref<ROStagingBufferVK>(alloc, this, p_description);
}

Ref<ROShaderVK> RendContextVK::create_shader(const ShaderDescVK& p_description) const {
    return make_ref<ROShaderVK>(alloc, this, p_description);
}

Ref<ROGraphicsPipelineVK> RendContextVK::create_graphics_pipeline(const GraphicsPipelineDescVK& p_description) const {
    return make_ref<ROGraphicsPipelineVK>(alloc, this, p_description);
}

Ref<ROComputePipelineVK> RendContextVK::create_compute_pipeline(const ComputePipelineDescVK& p_description) const {
    return make_ref<ROComputePipelineVK>(alloc, this, p_description);
}

} // namespace WhiteBirdEngine
