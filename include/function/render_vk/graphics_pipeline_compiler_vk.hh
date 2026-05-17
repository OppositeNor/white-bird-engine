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
#ifndef WBE_FILE_GRAPHICS_PIPELINE_COMPILER_VK_HH
#define WBE_FILE_GRAPHICS_PIPELINE_COMPILER_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_render_pass_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendererVK;

/**
 * @class GraphicsPipelineCompilerVKContext
 * @brief Runtime state required to compile a graphics pipeline resource.
 */
struct GraphicsPipelineCompilerVKContext {
    /**
     * @brief Allocator used for generated render graph descriptors.
     */
    RenderAllocator* allocator = nullptr;
    /**
     * @brief Vulkan renderer state used by render object builders.
     */
    const BaseRenderDataVK* render_data = nullptr;
    /**
     * @brief Registry used to reuse compiled render objects by resource key.
     */
    Ref<RenderDatabase> render_database = MEM_NULL;
    /**
     * @brief Render pass used by the generated graphics pipeline.
     */
    Ref<RORenderPassVK> render_pass = MEM_NULL;
    /**
     * @brief Renderer used to execute the generated task's render graph.
     */
    RendererVK* renderer = nullptr;
    /**
     * @brief Number of color attachments declared by the render pass. The
     *        compiled pipeline's color blend state will provide this many
     *        default color attachment states. Defaults to 1.
     */
    uint32_t color_attachment_count = 1;
};

/**
 * @class GraphicsPipelineCompilerVK
 * @brief Compiles a graphics pipeline resource unit into render graph objects.
 *
 * The compiler reuses registered shaders, descriptor set layouts, pipeline
 * layouts, and graphics pipelines when matching entries already exist in the
 * render database.
 */
class GraphicsPipelineCompilerVK {
public:
    /**
     * @brief Construct a compiler with the given runtime context.
     *
     * @param p_context Runtime compilation context.
     */
    explicit GraphicsPipelineCompilerVK(const GraphicsPipelineCompilerVKContext& p_context);

    /**
     * @brief Compile a graphics pipeline resource into a render task
     * descriptor, appending the generated pipeline render object to
     * the supplied pipeline descriptor's resource list.
     *
     * The caller is responsible for pushing the returned task descriptor
     * into the pipeline descriptor's task list, and for adding any
     * additional resources (framebuffer, attachment images, etc.)
     * referenced by the task to the descriptor.
     *
     * @param p_resource_key Key of the graphics pipeline resource.
     * @param p_pipeline_resource Loaded graphics pipeline resource unit.
     * @param p_descriptor Descriptor to append the pipeline render object resource to.
     * @param p_target_framebuffer_res_id Render resource id of the framebuffer
     * the task renders into.
     * @param p_clear_values Clear values for each attachment of the
     * framebuffer's render pass.
     * @param p_vertex_buffer_res_id Optional vertex buffer resource id. Pass
     * nullopt for pipelines with no vertex input (e.g. fullscreen
     * passes).
     * @param p_index_buffer_res_id Optional index buffer resource id. Pass
     * nullopt when not drawing indexed.
     * @param p_prev_tasks Tasks the returned task depends on.
     * @param p_uniform_bindings Caller-provided uniform field bindings.
     * @return Render task descriptor for the compiled graphics pipeline.
     */
    std::tuple<Ref<ROGraphicsPipelineVK>, std::vector<Ref<RODescriptorSetVK>>>
        compile(const std::string& p_resource_key,
                RefRes<RUGraphicsPipeline> p_pipeline_resource,
                std::span<const RenderUniformBinding> p_uniform_bindings = {});

private:
    struct CompiledPipelineObjects {
        Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
        std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
    };

    GraphicsPipelineCompilerVKContext context;

    CompiledPipelineObjects compile_pipeline_objects(const std::string& p_resource_key,
        RefRes<RUGraphicsPipeline> p_pipeline_resource,
        std::span<const RenderUniformBinding> p_uniform_bindings);
    Ref<ROShaderVK> compile_shader(RefRes<RUShader> p_shader);
    std::vector<Ref<RODescriptorSetLayoutVK>> compile_descriptor_set_layouts(
        const std::string& p_resource_key, const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings);
    Ref<ROPipelineLayoutVK> compile_pipeline_layout(
        const std::string& p_resource_key, const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts);
    Ref<ROGraphicsPipelineVK> compile_graphics_pipeline(const std::string& p_resource_key,
        const RUGraphicsPipeline& p_pipeline_resource,
        Ref<ROShaderVK> p_vertex_shader,
        Ref<ROShaderVK> p_fragment_shader,
        Ref<ROPipelineLayoutVK> p_pipeline_layout);
    std::vector<Ref<RODescriptorSetVK>> compile_descriptor_sets(const std::string& p_resource_key,
        const RUGraphicsPipeline& p_pipeline_resource,
        const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        std::span<const RenderUniformBinding> p_uniform_bindings);
    Ref<RODescriptorSetVK> compile_material_descriptor_set(const std::string& p_resource_key,
        RefRes<RUMaterial> p_material,
        Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index);
    Ref<ROImageViewVK> compile_image_view(const std::string& p_resource_key, RefRes<RUImage> p_image_resource);
    Ref<ROImageVK> compile_image(const std::string& p_resource_key, RefRes<RUImage> p_image_resource, VkFormat p_image_format);
    void write_uniform_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index,
        std::span<const RenderUniformBinding> p_uniform_bindings) const;
    RefRes<RUMaterial> find_material_for_pipeline(const RUGraphicsPipeline& p_pipeline_resource) const;
};

} // namespace WhiteBirdEngine

#endif
