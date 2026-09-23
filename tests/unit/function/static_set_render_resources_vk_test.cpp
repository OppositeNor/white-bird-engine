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
#include "function/render/render_defs.hh"
#include "function/render_vk/render_graph_descriptor_vk.hh"
#include "function/render_vk/static_set_render_resources_vk.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>

namespace WhiteBirdEngine {

TEST(WBEStaticSetRenderResourcesVKTest, RegistersEmptyTextureAggregateAndExposesPublicResources) {
    RenderAllocator allocator(WBE_KI_B(64));
    StaticSetRenderResourcesVK resources(&allocator);
    RendTaskDescVK task_desc(&allocator);

    RendTaskDescVK& result = resources.register_resources(task_desc);

    EXPECT_EQ(&result, &task_desc);
    ASSERT_EQ(task_desc.resources.size(), 1U);
    EXPECT_EQ(task_desc.resources[0].res_id, REND_RES_STATIC_SET_TEXTURES);
    EXPECT_NE(task_desc.resources[0].resource_list, MEM_NULL);
    EXPECT_TRUE(task_desc.reads.empty());
    EXPECT_EQ(resources.acquire_vertices(), MEM_NULL);
    EXPECT_EQ(resources.acquire_indices(), MEM_NULL);
    EXPECT_EQ(resources.acquire_opaque_draw_batches(), MEM_NULL);
    EXPECT_EQ(resources.acquire_masked_draw_batches(), MEM_NULL);
    EXPECT_NE(resources.acquire_textures(), MEM_NULL);
}

} // namespace WhiteBirdEngine
