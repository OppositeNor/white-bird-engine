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
#include "function/render/render_defs.hh"
#include "function/render_vk/rt_run_compute_pipelines_sequential_vk.hh"
#include "function/render_vk/rt_run_graphics_pipelines_sequential_vk.hh"
#include "utils/defs.hh"
#include <gtest/gtest.h>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TEST(WBERunPipelinesSequentialVKTest, ComputeDispatchAddsPipelineAndGroupCount) {
    RenderAllocator allocator(WBE_KI_B(64));
    RTRunComputePipelinesSequentialVK::Dispatch dispatch(&allocator);
    Ref<ROComputePipelineVK> pipeline = MEM_NULL;
    const VkExtent3D group_count{.width = 4, .height = 3, .depth = 2};

    RTRunComputePipelinesSequentialVK::Dispatch& result = dispatch.add_entry(pipeline, group_count);

    EXPECT_EQ(&result, &dispatch);
    ASSERT_EQ(dispatch.entries.size(), 1U);
    EXPECT_EQ(dispatch.entries[0].pipeline, pipeline);
    EXPECT_EQ(dispatch.entries[0].group_count.width, group_count.width);
    EXPECT_EQ(dispatch.entries[0].group_count.height, group_count.height);
    EXPECT_EQ(dispatch.entries[0].group_count.depth, group_count.depth);
}

TEST(WBERunPipelinesSequentialVKTest, GraphicsDispatchAddsEntry) {
    RenderAllocator allocator(WBE_KI_B(64));
    RTRunGraphicsPipelinesSequentialVK::Dispatch dispatch(&allocator);
    RTRunGraphicsPipelinesSequentialVK::Entry entry(&allocator);
    entry.render_extent = {.width = 1280, .height = 720};
    entry.color_attachment_ids.push_back("color");

    RTRunGraphicsPipelinesSequentialVK::Dispatch& result = dispatch.add_entry(std::move(entry));

    EXPECT_EQ(&result, &dispatch);
    ASSERT_EQ(dispatch.entries.size(), 1U);
    EXPECT_EQ(dispatch.entries[0].render_extent.width, 1280U);
    EXPECT_EQ(dispatch.entries[0].render_extent.height, 720U);
    ASSERT_EQ(dispatch.entries[0].color_attachment_ids.size(), 1U);
    EXPECT_EQ(dispatch.entries[0].color_attachment_ids[0], "color");
}

} // namespace WhiteBirdEngine
