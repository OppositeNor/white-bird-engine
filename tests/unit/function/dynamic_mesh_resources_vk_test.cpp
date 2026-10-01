/* Copyright 2026 OppositeNor

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
#include "function/render_vk/dynamic_mesh_resources_vk.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <stdexcept>

namespace WhiteBirdEngine {

TEST(WBEDynamicMeshResourcesVKTest, StartsWithIndependentEmptyResources) {
    RenderAllocator allocator(WBE_KI_B(64));
    DynamicMeshResourcesVK resources(&allocator);

    EXPECT_EQ(resources.acquire_vertices(), MEM_NULL);
    EXPECT_EQ(resources.acquire_indices(), MEM_NULL);
    EXPECT_EQ(resources.acquire_draw_batches(), MEM_NULL);
    EXPECT_EQ(resources.acquire_submeshes(), MEM_NULL);
    EXPECT_EQ(resources.acquire_nodes(), MEM_NULL);
    EXPECT_EQ(resources.acquire_instances(), MEM_NULL);
    EXPECT_EQ(resources.acquire_material_params(), MEM_NULL);
    EXPECT_EQ(resources.acquire_material_sampler(), MEM_NULL);
    EXPECT_NE(resources.acquire_textures(), MEM_NULL);
    EXPECT_EQ(resources.get_capacity(), 0U);
    EXPECT_EQ(resources.get_draw_batch_count(), 0U);
    EXPECT_THROW(resources.get_draw_batch(0), std::runtime_error);
}

TEST(WBEDynamicMeshResourcesVKTest, RejectsNullAllocator) {
    EXPECT_THROW(DynamicMeshResourcesVK(nullptr), std::runtime_error);
}

} // namespace WhiteBirdEngine
