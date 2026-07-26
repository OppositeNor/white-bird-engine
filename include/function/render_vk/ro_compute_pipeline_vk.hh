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
#ifndef WBE_FILE_RO_COMPUTE_PIPELINE_VK_HH
#define WBE_FILE_RO_COMPUTE_PIPELINE_VK_HH
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "core/reflection/reflection_defs.hh"
#include "function/render_vk/render_object_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_pipeline_layout_vk.hh"
#include "function/render_vk/ro_shader_vk.hh"
#include "utils/utils.hh"
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>
namespace WhiteBirdEngine {

/**
 * @class ROComputePipelineVK
 * @brief Vulkan compute pipeline render object.
 */
class WBE_META(WBE_RENDER_OBJECT) ROComputePipelineVK final : public RenderObjectVK {
public:
    /**
     * @class Builder
     * @brief Builder for compute pipeline render objects.
     */
    struct Builder {
        /**
         * @class ShaderStage
         * @brief Compute shader stage configuration.
         */
        struct ShaderStage {
            /** The entry function name of the shader. */
            std::string entry_function = "main";
            /** Shader instance. */
            Ref<ROShaderVK> shader;
        };

        // Compute shader stage.
        Required<ShaderStage> shader_stage;
        // Pipeline layout.
        Ref<ROPipelineLayoutVK> pipeline_layout;

        /**
         * @brief Set the compute shader stage.
         *
         * @param p_shader The shader instance.
         * @param p_entry_function The shader entry function.
         * @return *this.
         */
        Builder& set_shader_stage(Ref<ROShaderVK> p_shader, std::string_view p_entry_function = "main") {
            shader_stage = ShaderStage{.entry_function = std::string(p_entry_function), .shader = p_shader};
            return *this;
        }

        /**
         * @brief Set the pipeline layout.
         *
         * @param p_pipeline_layout The pipeline layout instance.
         * @return *this.
         */
        Builder& set_pipeline_layout(Ref<ROPipelineLayoutVK> p_pipeline_layout) {
            pipeline_layout = p_pipeline_layout;
            return *this;
        }

        /**
         * @brief Create a reference-counted compute pipeline object.
         *
         * @param p_render_data Base render data.
         * @return Reference-counted compute pipeline object.
         */
        Ref<ROComputePipelineVK> create_ref(const BaseRenderDataVK* p_render_data) const {
            return make_ref<ROComputePipelineVK>(p_render_data->allocator, p_render_data, *this);
        }

        /**
         * @brief Create a unique-ownership compute pipeline object.
         *
         * @param p_render_data Base render data.
         * @return Unique-ownership compute pipeline object.
         */
        Unique<ROComputePipelineVK> create_unique(const BaseRenderDataVK* p_render_data) const {
            return make_unique<ROComputePipelineVK>(p_render_data->allocator, p_render_data, *this);
        }
    };

    virtual ~ROComputePipelineVK();
    ROComputePipelineVK(const ROComputePipelineVK&) = delete;
    ROComputePipelineVK(ROComputePipelineVK&&) = delete;
    ROComputePipelineVK& operator=(const ROComputePipelineVK&) = delete;
    ROComputePipelineVK& operator=(ROComputePipelineVK&&) = delete;

    /**
     * @brief Construct a compute pipeline render object.
     *
     * @param p_render_data Base render data.
     * @param p_builder Builder containing the compute pipeline configuration.
     */
    ROComputePipelineVK(const BaseRenderDataVK* p_render_data, const Builder& p_builder);

    /**
     * @brief Get the Vulkan compute pipeline handle.
     *
     * @return The Vulkan compute pipeline handle.
     */
    VkPipeline get_compute_pipeline() const {
        return compute_pipeline;
    }

    /**
     * @class ExposedFields
     * @brief Exposed fields of ROComputePipelineVK.
     */
    struct ExposedFields {
        /**
         * @brief Compute shader used by this pipeline.
         */
        Ref<const ROShaderVK> compute_shader;
        /**
         * @brief Pipeline layout used for descriptor set binding.
         */
        Ref<const ROPipelineLayoutVK> pipeline_layout;
    };

    const ExposedFields& fields() const {
        return exposed_fields;
    }

private:
    ExposedFields exposed_fields;
    VkPipeline compute_pipeline = VK_NULL_HANDLE;
};

} // namespace WhiteBirdEngine

#endif
