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
#include "core/allocator/i_allocator.hh"
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/application/application_game.hh"
#include "function/ecs/components.hh"
#include "function/function_utils.hh"
#include "function/input_manager/input_manager.hh"
#include "function/input_manager/key_codes.hh"
#include "function/render/render_defs.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/render_graph_vk.hh"
#include "function/render_vk/render_vk_defs.hh"
#include "function/render_vk/renderer_vk.hh"
#include "function/render_vk/ro_buffer_vk.hh"
#include "function/render_vk/ro_image_view_vk.hh"
#include "function/render_vk/ro_image_vk.hh"
#include "function/render_vk/rt_run_static_set_vk.hh"
#include "generated/type_uuid.gen.hh"
#include "global/global.hh"
#include "platform/file_system/path.hh"
#include "render_test.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "resource/resource_unit/resource_unit.hh"
#include <algorithm>
#include <cstdlib>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

inline constexpr std::string_view REND_RES_TEST_DRAW_SPONZA_INDIRECT_DEPTH_IMAGE_VIEW = "sponza_depth";
static constexpr std::string_view REND_RES_TEST_DRAW_SPONZA_INDIRECT_CAMERA_INFO_BUFFER = "camera_info";
inline constexpr VkFormat DRAW_SPONZA_INDIRECT_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

class ApplicationVulkanDrawSponzaIndirectTest final : public ApplicationGame {
public:
    virtual void logic_update(double p_delta) override;
    virtual void render_update(double p_delta) override;

    void setup_draw_task();

private:
    void create_depth_resources();
    void write_camera_descriptor();

    RendererVK* renderer_vk = nullptr;
    const BaseRenderDataVK* base_render_data = nullptr;
    RenderAllocator* render_allocator = nullptr;
    RefRes<RUStaticOpaqueSet> sponza_set;
    Ref<ROImageVK> depth_image;
    Ref<ROImageViewVK> depth_view;
    Ref<ROBufferVK> camera_info_buffer;
    ComponentCamera camera_component{};
    glm::vec3 camera_position = TEST_DEFAULT_CAMERA_POSITION;
    float camera_yaw = default_test_camera_yaw();
    float camera_pitch = default_test_camera_pitch();
};

void ApplicationVulkanDrawSponzaIndirectTest::logic_update(double p_delta) {
    InputManager* input_manager = InputManager::get_singleton();
    if (input_manager == nullptr) {
        return;
    }
    float delta = static_cast<float>(p_delta);
    if (input_manager->get_input_status(KEYCODE_LEFT).is_down()) {
        camera_yaw -= TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(KEYCODE_RIGHT).is_down()) {
        camera_yaw += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(KEYCODE_UP).is_down()) {
        camera_pitch += TEST_CAMERA_TURN_SPEED * delta;
    }
    if (input_manager->get_input_status(KEYCODE_DOWN).is_down()) {
        camera_pitch -= TEST_CAMERA_TURN_SPEED * delta;
    }
    camera_pitch = std::clamp(camera_pitch, -TEST_CAMERA_PITCH_LIMIT, TEST_CAMERA_PITCH_LIMIT);

    glm::vec3 forward = test_camera_forward(camera_yaw, camera_pitch);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));
    float move_delta = TEST_CAMERA_MOVE_SPEED * delta;
    if (input_manager->get_input_status(KEYCODE_W).is_down()) {
        camera_position += forward * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_S).is_down()) {
        camera_position -= forward * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_D).is_down()) {
        camera_position += right * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_A).is_down()) {
        camera_position -= right * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_E).is_down()) {
        camera_position += up * move_delta;
    }
    if (input_manager->get_input_status(KEYCODE_Q).is_down()) {
        camera_position -= up * move_delta;
    }

    camera_component.view = test_camera_view(camera_position, camera_yaw, camera_pitch);
    if (camera_info_buffer == MEM_NULL) {
        return;
    }
    CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    ROBufferVK* camera_buffer = camera_info_buffer.get();
    add_render_job([camera_info, camera_buffer]() {
        camera_buffer->update_buffer(&camera_info, sizeof(camera_info));
    });
}

void ApplicationVulkanDrawSponzaIndirectTest::render_update(double /*p_delta*/) {
    WBE::on_exec_test_render(*renderer_vk, base_render_data->current_frame);
}

void ApplicationVulkanDrawSponzaIndirectTest::setup_draw_task() {
    renderer_vk = RendererVK::get_singleton();
    base_render_data = renderer_vk->get_base_render_data();
    render_allocator = get_renderer_allocator().get();

    ResourceManager* resource_manager = ResourceManager::get_singleton();
    auto resource_chunk = make_ref<ResourceChunkLoadFile>(
        global_allocator(), "res_chunks/rendering_test_resources.json", resource_manager->get_allocator());
    resource_manager->register_chunk(resource_chunk);
    sponza_set = resource_manager->get_resource<RUStaticOpaqueSet>("sponza.static_opaque_set");
    if (sponza_set == nullptr) {
        throw std::runtime_error("Failed to setup indirect Sponza test: static opaque set resource was not found.");
    }
    RefRes<RUGraphicsPipeline> graphics_pipeline = resource_manager->get_resource<RUGraphicsPipeline>("forward_cook_torrance_pbr_static");
    if (graphics_pipeline == nullptr) {
        throw std::runtime_error("Failed to setup indirect Sponza test: static graphics pipeline resource was not found.");
    }
    camera_component = make_default_test_camera_component(base_render_data->swap_chain_aspect_ratio, 100.0F);
    create_depth_resources();

    RTRunStaticSetVK::Config task_config;
    task_config.add_color_attachment(REND_RES_SWAP_CHAIN, base_render_data->swap_chain_format)
        .set_depth_attachment(REND_RES_TEST_DRAW_SPONZA_INDIRECT_DEPTH_IMAGE_VIEW, DRAW_SPONZA_INDIRECT_DEPTH_FORMAT);
    auto task = make_unique<RTRunStaticSetVK>(render_allocator, renderer_vk, sponza_set, graphics_pipeline, task_config);
    write_camera_descriptor();

    RendGraphDescVK graph_desc(render_allocator);
    graph_desc.add_resource(REND_RES_SWAP_CHAIN, renderer_vk->acquire_swap_chain_image_view_container())
        .add_resource(REND_RES_TEST_DRAW_SPONZA_INDIRECT_DEPTH_IMAGE_VIEW, depth_view)
        .add_resource(REND_RES_TEST_DRAW_SPONZA_INDIRECT_CAMERA_INFO_BUFFER, camera_info_buffer);
    graph_desc.tasks.emplace_back(render_allocator, RT_RUN_STATIC_SET, std::move(task));
    Ref<RenderGraphVK> graph = renderer_vk->register_render_graph("draw_sponza_indirect_graph", std::move(graph_desc));
    write_descriptor_debug_output("draw_sponza_indirect_graph_descriptors", graph->list_descriptors());
    renderer_vk->activate_render_graph("draw_sponza_indirect_graph");
}

void ApplicationVulkanDrawSponzaIndirectTest::create_depth_resources() {
    depth_image = ROImageVK::Builder()
                      .set_properties_2d(DRAW_SPONZA_INDIRECT_DEPTH_FORMAT, base_render_data->swap_chain_extent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                      .create_ref(base_render_data);
    depth_view =
        ROImageViewVK::Builder()
            .set_image(depth_image)
            .set_view_type(VK_IMAGE_VIEW_TYPE_2D)
            .set_format(DRAW_SPONZA_INDIRECT_DEPTH_FORMAT)
            .set_subresource_range(VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1})
            .create_ref(base_render_data);
}

void ApplicationVulkanDrawSponzaIndirectTest::write_camera_descriptor() {
    CameraInfoVKUBO camera_info = make_camera_info_ubo(camera_component);
    camera_info_buffer =
        ROBufferVK::Builder().set_usage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT).initial_data(&camera_info, sizeof(camera_info)).create_ref(base_render_data);
}

} // namespace WhiteBirdEngine

int main(int p_argc, char* p_argv[]) {
    WBE::on_exec_test_start();
    std::unique_ptr<WhiteBirdEngine::Global> global =
        std::make_unique<WhiteBirdEngine::Global>(p_argc, p_argv, WhiteBirdEngine::Path("test_env"));
    WhiteBirdEngine::ApplicationVulkanDrawSponzaIndirectTest application;
    application.setup_draw_task();
    application.run();
    return EXIT_SUCCESS;
}
