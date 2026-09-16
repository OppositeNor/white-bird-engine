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
#ifndef WBE_FILE_RT_RUN_STATIC_SET_VK_HH
#define WBE_FILE_RT_RUN_STATIC_SET_VK_HH

#include "core/allocator/i_allocator.hh"
#include "core/allocator/stl_allocator.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/graphics_pipeline_compiler_vk.hh"
#include "function/render_vk/render_task_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/ro_graphics_pipeline_vk.hh"
#include "function/render_vk/rt_run_graphics_pipeline_indirect_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

class RendererVK;

inline constexpr std::string_view RT_RUN_STATIC_SET = "run_static_set";

/**
 * @class RTRunStaticSetVK
 * @brief Vulkan render task for an explicit static geometry set and graphics pipeline.
 *
 * The caller supplies the source geometry set and the graphics pipeline explicitly. Empty sets record a no-draw
 * rendering pass that still honors attachment load and clear operations. Non-empty sets compile one graphics pipeline,
 * retain descriptor bindings, and group indirect indexed draws by compatible vertex input. Groups follow first appearance,
 * preserving source order within each group; submesh and material identity remains in the original instance tables.
 */
class RTRunStaticSetVK final : public RenderTaskVK {
public:
    /**
     * @class Config
     * @brief Attachment configuration and compatibility material-contract settings for the task.
     */
    class Config {
    public:
        Config() = default;
        ~Config() = default;
        Config(const Config&) = default;
        Config(Config&&) noexcept = default;
        Config& operator=(const Config&) = default;
        Config& operator=(Config&&) noexcept = default;

        Config& set_default_material_resource_key(std::string_view p_resource_key);
        Config& set_material_textures_variable_name(std::string_view p_variable_name);
        Config& set_material_params_variable_name(std::string_view p_variable_name);
        Config& set_render_extent(VkExtent2D p_render_extent);
        Config& add_color_attachment(std::string_view p_resource_id, VkFormat p_format);
        Config& set_depth_attachment(std::string_view p_resource_id, VkFormat p_format);
        Config& set_stencil_attachment_format(VkFormat p_format);
        Config& add_clear_value(VkClearValue p_clear_value);
        Config& set_color_attachment_load_op(VkAttachmentLoadOp p_load_op);
        Config& set_depth_attachment_load_op(VkAttachmentLoadOp p_load_op);

        std::string_view get_default_material_resource_key() const;
        std::string_view get_material_textures_variable_name() const;
        std::string_view get_material_params_variable_name() const;
        VkExtent2D get_render_extent() const;
        size_t get_color_attachment_count() const;
        std::string_view get_color_attachment_id(size_t p_index) const;
        VkFormat get_color_attachment_format(size_t p_index) const;
        bool has_depth_attachment() const;
        std::string_view get_depth_attachment_id() const;
        VkFormat get_depth_attachment_format() const;
        VkFormat get_stencil_attachment_format() const;
        size_t get_clear_value_count() const;
        VkClearValue get_clear_value(size_t p_index) const;
        VkAttachmentLoadOp get_color_attachment_load_op() const;
        VkAttachmentLoadOp get_depth_attachment_load_op() const;

    private:
        std::string default_material_resource_key = "default_material";
        std::string material_textures_variable_name = "material_textures";
        std::string material_params_variable_name = "material_params";
        VkExtent2D render_extent{};
        std::vector<std::string> color_attachment_ids;
        std::optional<std::string> depth_attachment_id;
        std::vector<VkFormat> color_attachment_formats;
        VkFormat depth_attachment_format = VK_FORMAT_UNDEFINED;
        VkFormat stencil_attachment_format = VK_FORMAT_UNDEFINED;
        std::vector<VkClearValue> clear_values;
        VkAttachmentLoadOp color_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentLoadOp depth_attachment_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    };

    RTRunStaticSetVK() = delete;
    virtual ~RTRunStaticSetVK() override = default;
    RTRunStaticSetVK(const RTRunStaticSetVK&) = delete;
    RTRunStaticSetVK(RTRunStaticSetVK&&) noexcept = delete;
    RTRunStaticSetVK& operator=(const RTRunStaticSetVK&) = delete;
    RTRunStaticSetVK& operator=(RTRunStaticSetVK&&) noexcept = delete;

    /**
     * @brief Construct an explicit static-set task.
     *
     * The task retains the source static-geometry set and the selected graphics-pipeline resource. For non-empty sets it
     * builds one compiled pipeline and one indirect draw task. Callers that need multiple pipelines for one logical pass
     * must compose multiple render-graph tasks explicitly.
     *
     * @param p_renderer Renderer providing Vulkan state.
     * @param p_set Static geometry set resource to render.
     * @param p_graphics_pipeline Graphics pipeline resource selected by the caller for this task instance.
     * @param p_config Attachment configuration and compatibility material-contract settings.
     */
    RTRunStaticSetVK(
        RendererVK* p_renderer, RefRes<RUStaticGeometrySet> p_set, RefRes<RUGraphicsPipeline> p_graphics_pipeline, const Config& p_config);

    virtual QueueFamilyVK queue_family() const override;
    virtual void bind_resources(const RendResViewVK& p_resources) override;
    virtual Vector<ResAccessVK, RenderAllocator> read_resources() const override;
    virtual Vector<ResAccessVK, RenderAllocator> write_resources() const override;
    virtual void record(VkCommandBuffer p_command_buffer, double p_delta) override;

    /**
     * @brief Replace one reflected descriptor-heap binding on the compiled pipeline.
     */
    void set_descriptor_binding(std::string_view p_field_path, Ref<IRenderObject> p_resource);
    /**
     * @brief Replace one reflected binding with a context-resolved resource.
     */
    void set_descriptor_binding(std::string_view p_field_path, Ref<RenderObjectContainerVK> p_resource_container);

private:
    const BaseRenderDataVK* render_data = nullptr;
    RefRes<RUStaticGeometrySet> geometry_set = MEM_NULL;
    RefRes<RUGraphicsPipeline> graphics_pipeline = MEM_NULL;
    Config config;
    Unique<RTRunGraphicsPipelineIndirectVK> indirect_task = MEM_NULL;
    Ref<ROGraphicsPipelineVK> compiled_graphics_pipeline = MEM_NULL;
    Vector<Ref<RenderObjectContainerVK>, RenderAllocator> color_attachment_containers;
    Ref<RenderObjectContainerVK> depth_attachment_container;
};

} // namespace WhiteBirdEngine

#endif
