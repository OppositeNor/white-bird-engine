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
#ifndef WBE_FILE_PIPELINE_COMPILER_VK_HH
#define WBE_FILE_PIPELINE_COMPILER_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/render_database.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_descriptor_set_layout_vk.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace WhiteBirdEngine {

class RODescriptorSetVK;

struct PipelineCompilerVKContext {
    RenderAllocator* allocator = nullptr;
    const BaseRenderDataVK* render_data = nullptr;
    Ref<RenderDatabase> render_database = MEM_NULL;
};

class PipelineCompilerVK {
public:
    using DescriptorSetBinding = std::pair<uint32_t, uint32_t>;
    using DescriptorSetBindings = std::unordered_map<std::string, DescriptorSetBinding>;

protected:
    using UniformDataBuilder = std::vector<uint8_t> (*)(const ShaderDescriptorBindingVK&, const RenderBindpointDeclarations&);

    explicit PipelineCompilerVK(const PipelineCompilerVKContext& p_context, std::string_view p_compiler_name);

    Ref<ROShaderVK> compile_shader(RefRes<RUShader> p_shader);
    std::vector<Ref<RODescriptorSetLayoutVK>> compile_descriptor_set_layouts(
        std::string_view p_resource_key, const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings);
    Ref<ROPipelineLayoutVK> compile_pipeline_layout(
        std::string_view p_resource_key, const std::vector<Ref<RODescriptorSetLayoutVK>>& p_descriptor_set_layouts);
    static DescriptorSetBindings build_descriptor_set_bindings(const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings);
    /**
     * @brief Write reflected storage-buffer bindings into one descriptor set.
     *
     * @param p_descriptor_set Descriptor set receiving the writes.
     * @param p_descriptor_bindings Reflected bindings for the pipeline.
     * @param p_set_index Set index represented by the descriptor set.
     * @param p_bindpoint_declarations Caller-provided storage-buffer render objects and ranges.
     */
    void write_storage_buffer_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index,
        const RenderBindpointDeclarations& p_bindpoint_declarations) const;
    /**
     * @brief Write reflected direct image and sampler bindings into one descriptor set.
     *
     * @param p_descriptor_set Descriptor set receiving the writes.
     * @param p_descriptor_bindings Reflected bindings for the pipeline.
     * @param p_set_index Set index represented by the descriptor set.
     * @param p_bindpoint_declarations Caller-provided image views and samplers.
     */
    void write_image_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index,
        const RenderBindpointDeclarations& p_bindpoint_declarations) const;
    /**
     * @brief Pack and write reflected uniform-buffer bindings into one descriptor set.
     *
     * @param p_descriptor_set Descriptor set receiving the writes.
     * @param p_descriptor_bindings Reflected bindings for the pipeline.
     * @param p_set_index Set index represented by the descriptor set.
     * @param p_bindpoint_declarations Caller-provided uniform data.
     * @param p_uniform_data_builder Pipeline-specific reflected-data packer.
     */
    void write_uniform_descriptors(Ref<RODescriptorSetVK> p_descriptor_set,
        const std::vector<ShaderDescriptorBindingVK>& p_descriptor_bindings,
        uint32_t p_set_index,
        const RenderBindpointDeclarations& p_bindpoint_declarations,
        UniformDataBuilder p_uniform_data_builder) const;

    template <typename RenderObjectType>
    static Ref<RenderObjectType> get_registered_render_object(
        Ref<RenderDatabase> p_render_database, std::string_view p_resource_key, HashCode p_render_object_type_id) {
        auto render_object = p_render_database->get_compiled_render_resource(p_resource_key, p_render_object_type_id);
        if (render_object == MEM_NULL) {
            return MEM_NULL;
        }
        return render_object.dynamic_cast_ref<RenderObjectType>();
    }

    PipelineCompilerVKContext base_context;
};

} // namespace WhiteBirdEngine

#endif
