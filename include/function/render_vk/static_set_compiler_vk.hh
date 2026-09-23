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
#ifndef WBE_FILE_STATIC_SET_COMPILER_VK_HH
#define WBE_FILE_STATIC_SET_COMPILER_VK_HH

#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_raw.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/i_render_task.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_list.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/i_render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/ro_sampler_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "function/render_vk/static_set_render_resources_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendererVK;
struct RendTaskDescVK;

/**
 * @class CompiledStaticSetVK
 * @brief Reusable Vulkan resources and draw plan compiled from one static geometry set.
 *
 * This object owns no render-task behavior. It can configure a task descriptor with its task-private resources and create a
 * generic graphics task for a caller-selected attachment pass. It is neither copyable nor movable; share it through Ref.
 */
class CompiledStaticSetVK final {
public:
    /**
     * @class PassConfig
     * @brief Attachment and load configuration for one execution of a compiled static set.
     */
    struct PassConfig {
        explicit PassConfig(RenderAllocator* p_allocator);
        ~PassConfig() = default;
        PassConfig(const PassConfig&) = default;
        PassConfig(PassConfig&&) noexcept = default;
        PassConfig& operator=(const PassConfig&) = default;
        PassConfig& operator=(PassConfig&&) noexcept = default;

        /** Render area used by dynamic rendering. */
        VkExtent2D render_extent{};
        /** Color attachment graph-resource ids in pipeline order. */
        Vector<std::string, RenderAllocator> color_attachment_ids;
        /** Optional depth attachment graph-resource id. */
        std::optional<std::string> depth_attachment_id;
        /** Color clear values followed by the optional depth clear value. */
        Vector<VkClearValue, RenderAllocator> clear_values;
        /** Load operation shared by color attachments. */
        VkAttachmentLoadOp color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        /** Load operation used by the depth attachment. */
        VkAttachmentLoadOp depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    };

    CompiledStaticSetVK() = delete;
    ~CompiledStaticSetVK() = default;
    CompiledStaticSetVK(const CompiledStaticSetVK&) = delete;
    CompiledStaticSetVK(CompiledStaticSetVK&&) noexcept = delete;
    CompiledStaticSetVK& operator=(const CompiledStaticSetVK&) = delete;
    CompiledStaticSetVK& operator=(CompiledStaticSetVK&&) noexcept = delete;

    /** @brief Construct empty allocator-owned compiled-set storage. */
    explicit CompiledStaticSetVK(RenderAllocator* p_allocator);

    /**
     * @brief Add every compiled resource to one task descriptor and declare aggregate texture reads.
     * @param p_task_desc Descriptor that will own the task-private graph bindings.
     * @return The supplied descriptor.
     */
    RendTaskDescVK& configure_task(RendTaskDescVK& p_task_desc) const;

    /**
     * @brief Create a generic graphics task that renders this compiled set.
     * @param p_render_data Renderer state used by the generic task.
     * @param p_config Caller-selected attachment pass.
     * @return Generic graphics task ready to add to the configured descriptor.
     */
    Unique<IRenderTaskVK> create_render_task(const BaseRenderDataVK* p_render_data, const PassConfig& p_config) const;

    /** @brief Acquire the compiled material texture aggregate. */
    Ref<RenderResourceList> acquire_texture_list() const;

    /**
     * @brief Acquire the storage-capable indexed-indirect command buffer.
     * @return Draw-command buffer, or null for an empty static set.
     */
    Ref<ROBufferVK> acquire_draw_batches_buffer() const;

    /** @brief Acquire the pipeline-independent resources used by this compiled draw plan. */
    Ref<StaticSetRenderResourcesVK> acquire_render_resources() const;

private:
    friend class StaticSetCompilerVK;

    RenderAllocator* allocator;
    Ref<ROGraphicsPipelineVK> graphics_pipeline;
    Ref<StaticSetRenderResourcesVK> render_resources;
    Vector<RTRunGraphicsPipelineIndirectVK::PipelineBatch, RenderAllocator> pipeline_batches;
};

/**
 * @class StaticSetCompilerVK
 * @brief Compiles a static geometry set into reusable graph resources and a generic graphics draw plan.
 */
class StaticSetCompilerVK final {
public:
    /**
     * @class Config
     * @brief Pipeline compatibility and material-contract configuration used during compilation.
     */
    struct Config {
        Config() = default;
        ~Config() = default;
        Config(const Config&) = default;
        Config(Config&&) noexcept = default;
        Config& operator=(const Config&) = default;
        Config& operator=(Config&&) noexcept = default;

        /** Default material resource used to fill missing shader texture roles. */
        std::string default_material_resource_key = "default_material";
        /** Reflected material texture-table base name. */
        std::string material_textures_variable_name = "material_textures";
        /** Reflected material parameter-buffer base name. */
        std::string material_params_variable_name = "material_params";
        /** Ordered color formats used to compile the graphics pipeline. */
        std::vector<VkFormat> color_attachment_formats;
        /** Optional depth attachment format. */
        VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
        /** Optional stencil attachment format. */
        VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;
    };

    StaticSetCompilerVK() = delete;
    ~StaticSetCompilerVK() = default;
    StaticSetCompilerVK(const StaticSetCompilerVK&) = delete;
    StaticSetCompilerVK(StaticSetCompilerVK&&) noexcept = delete;
    StaticSetCompilerVK& operator=(const StaticSetCompilerVK&) = delete;
    StaticSetCompilerVK& operator=(StaticSetCompilerVK&&) noexcept = delete;

    /** @brief Construct a compiler using renderer-owned Vulkan services. */
    explicit StaticSetCompilerVK(RendererVK* p_renderer);

    /**
     * @brief Compile one static set and graphics pipeline.
     * @throws std::runtime_error If inputs or reflected contracts are invalid or GPU resource creation fails.
     */
    Ref<CompiledStaticSetVK> compile(
        RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_graphics_pipeline, const Config& p_config) const;

    /**
     * @brief Compile one pipeline-independent resource set containing opaque and masked draw batches.
     *
     * Vertex metadata supplies only layout and static-node reflection contracts; no render pipeline is compiled or retained.
     * Null opaque or masked sets are allowed, but at least one non-empty set is required.
     *
     * @throws std::runtime_error If resource data or reflected contracts are invalid or incompatible.
     */
    Ref<StaticSetRenderResourcesVK> compile_resources(RefRes<RUStaticOpaqueSet> p_opaque_set,
        RefRes<RUStaticMaskedSet> p_masked_set,
        const ShaderMetadata& p_opaque_vertex_metadata,
        std::string_view p_opaque_vertex_entry,
        const ShaderMetadata& p_masked_vertex_metadata,
        std::string_view p_masked_vertex_entry,
        const std::optional<MaterialTextureContract>& p_material_contract,
        const Config& p_config) const;

private:
    RendererVK* renderer;
};

} // namespace WhiteBirdEngine

#endif
