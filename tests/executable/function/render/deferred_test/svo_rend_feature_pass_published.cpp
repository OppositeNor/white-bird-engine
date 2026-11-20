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
#include "svo_rend_feature_pass_published.hh"
#include "../rend_test.hh"
#include "core/memory/reference_strong.hh"
#include "core/memory/unique.hh"
#include "function/rend_vk/i_rend_task_vk.hh"
#include "function/rend_vk/mesh_resources_vk.hh"
#include "function/rend_vk/rend_graph_context_vk.hh"
#include "function/rend_vk/rend_graph_desc_vk.hh"
#include "function/rend_vk/renderer_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "function/rend_vk/ro_compute_pipeline_vk.hh"
#include "function/rend_vk/rt_run_compute_pipeline_vk.hh"
#include "function/rend_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "i_svo_rend_feature.hh"
#include <atomic>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

SVORendFeaturePassPublished::SVORendFeaturePassPublished(RendererVK* p_renderer,
    Ref<MeshResourcesVK> p_static_mesh_resources,
    const RUAABB3D& p_scene_bounds,
    bool p_rebuild_static_every_frame)
    : ISVORendFeature(p_renderer, std::move(p_static_mesh_resources), p_scene_bounds, "svo_2", "svo_2_dynamic", p_rebuild_static_every_frame) {
}

std::string_view SVORendFeaturePassPublished::get_name() const {
    return "svo_2 (pass publication)";
}

uint32_t SVORendFeaturePassPublished::get_reset_group_count() const {
    return 1;
}

SVORecipeVariant SVORendFeaturePassPublished::get_recipe_variant() const {
    return SVORecipeVariant::PASS_PUBLISHED;
}

} // namespace WhiteBirdEngine
