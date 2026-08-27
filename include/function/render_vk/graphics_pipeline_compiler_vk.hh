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
#include "function/render_vk/pipeline_compiler_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_descriptor_set_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/ro_material_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstdint>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
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
     * @brief Renderer used to execute the generated task's render graph.
     */
    RendererVK* renderer = nullptr;
    /**
     * @brief Optional valid image used for sampled-image descriptors that the caller leaves unset.
     */
    RefRes<RUImage> default_image_resource = MEM_NULL;
    /**
     * @brief Dynamic-rendering color attachment formats.
     */
    std::vector<VkFormat> color_attachment_formats;
    /**
     * @brief Dynamic-rendering depth attachment format.
     */
    VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
    /**
     * @brief Dynamic-rendering stencil attachment format.
     */
    VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;
};

/**
 * @class GraphicsPipelineCompilerVK
 * @brief Compiles a graphics pipeline resource unit into render graph objects.
 *
 * The compiler reuses registered shaders, descriptor set layouts, pipeline
 * layouts, and graphics pipelines when matching entries already exist in the
 * render database.
 */
class GraphicsPipelineCompilerVK : public PipelineCompilerVK {
public:
    using SupportedUniformDataTypes = std::tuple<float, int32_t, uint32_t, glm::vec2, glm::vec3, glm::vec4, glm::mat3, glm::mat4>;
    using DescriptorSetBinding = PipelineCompilerVK::DescriptorSetBinding;
    using DescriptorSetBindings = PipelineCompilerVK::DescriptorSetBindings;

    struct CompileResult {
        Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
        std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
        DescriptorSetBindings descriptor_set_bindings;
    };

    /**
     * @brief Construct a compiler with the given runtime context.
     *
     * @param p_context Runtime compilation context.
     */
    explicit GraphicsPipelineCompilerVK(const GraphicsPipelineCompilerVKContext& p_context);

    /**
     * @brief Compile a graphics pipeline resource into Vulkan render objects and reflected descriptor binding locations.
     *
     * @param p_resource_key Key of the graphics pipeline resource.
     * @param p_pipeline_resource Loaded graphics pipeline resource unit.
     * @param p_bindpoint_declarations Caller-provided shader bindpoint data.
     * @return Compiled pipeline objects and a descriptor-scope name to set/binding map.
     */
    CompileResult compile(std::string_view p_resource_key,
        RefRes<RUGraphicsPipeline> p_pipeline_resource,
        const RenderBindpointDeclarations& p_bindpoint_declarations = {});

private:
    struct CompiledPipelineObjects {
        Ref<ROGraphicsPipelineVK> graphics_pipeline = MEM_NULL;
        std::vector<Ref<RODescriptorSetVK>> descriptor_sets;
        DescriptorSetBindings descriptor_set_bindings;
    };

    GraphicsPipelineCompilerVKContext context;

    CompiledPipelineObjects compile_pipeline_objects(std::string_view p_resource_key,
        RefRes<RUGraphicsPipeline> p_pipeline_resource,
        const RenderBindpointDeclarations& p_bindpoint_declarations);
    Ref<ROGraphicsPipelineVK> compile_graphics_pipeline(std::string_view p_resource_key,
        const RUGraphicsPipeline& p_pipeline_resource,
        Ref<ROShaderVK> p_vertex_shader,
        Ref<ROShaderVK> p_geometry_shader,
        Ref<ROShaderVK> p_fragment_shader,
        std::string_view p_vertex_stage_entry,
        std::string_view p_geometry_stage_entry,
        std::string_view p_fragment_stage_entry,
        Ref<ROPipelineLayoutVK> p_pipeline_layout);
    std::vector<Ref<RODescriptorSetVK>> compile_descriptor_sets(std::string_view p_resource_key,
        const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        const RenderBindpointDeclarations& p_bindpoint_declarations);
    Ref<RODescriptorSetVK> compile_texture_descriptor_set(std::string_view p_resource_key,
        Ref<RODescriptorSetLayoutVK> p_descriptor_set_layout,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index,
        const RenderBindpointDeclarations& p_bindpoint_declarations);
    void add_direct_texture_bindpoints(
        std::string_view p_resource_key, ROMaterialVK::Builder& p_builder, const RenderBindpointDeclarations& p_bindpoint_declarations);
    void add_direct_sampler_bindpoints(
        std::string_view p_resource_key, ROMaterialVK::Builder& p_builder, const RenderBindpointDeclarations& p_bindpoint_declarations);
    Ref<ROImageViewVK> compile_image_view(std::string_view p_resource_key, RefRes<RUImage> p_image_resource);
    Ref<ROImageVK> compile_image(std::string_view p_resource_key, RefRes<RUImage> p_image_resource, VkFormat p_image_format);
    void write_uniform_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index,
        const RenderBindpointDeclarations& p_bindpoint_declarations) const;
};

} // namespace WhiteBirdEngine

#endif
