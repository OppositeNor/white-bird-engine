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
#include "function/render_vk/geometry_draw_data_vk.hh"
#include "utils/defs.hh"
#include <array>
#include <glm/ext/matrix_transform.hpp>
#include <gtest/gtest.h>
#include <stdexcept>

namespace WhiteBirdEngine {

TEST(WBEGeometryDrawDataVKTest, VisibilityChangesPreserveGeometryAndInstanceLinkAddressing) {
    RenderAllocator allocator(WBE_KI_B(64));
    std::array<VkDrawIndexedIndirectCommand, 2> geometry{{
        {.indexCount = 36, .instanceCount = 2, .firstIndex = 7, .vertexOffset = 4, .firstInstance = 0},
        {.indexCount = 12, .instanceCount = 2, .firstIndex = 43, .vertexOffset = 9, .firstInstance = 2},
    }};
    GeometryDrawDataVK data(&allocator, 2, geometry);
    std::array<glm::mat4, 2> transforms{
        glm::scale(glm::mat4(1.0F), glm::vec3(2.0F, 3.0F, 4.0F)),
        glm::translate(glm::mat4(1.0F), glm::vec3(5.0F, 6.0F, 7.0F)),
    };
    for (size_t count : {0U, 1U, 2U, 1U, 0U}) {
        data.update({transforms.data(), count});
        for (size_t i = 0; i < geometry.size(); ++i) {
            EXPECT_EQ(data.get_commands()[i].instanceCount, count);
            EXPECT_EQ(data.get_commands()[i].firstInstance, geometry[i].firstInstance);
            EXPECT_EQ(data.get_commands()[i].firstIndex, geometry[i].firstIndex);
            EXPECT_EQ(data.get_commands()[i].indexCount, geometry[i].indexCount);
            EXPECT_EQ(data.get_commands()[i].vertexOffset, geometry[i].vertexOffset);
        }
        for (size_t i = 0; i < 2; ++i) {
            EXPECT_EQ(data.get_nodes()[i].transform, i < count ? transforms[i] : glm::mat4(1.0F));
        }
        if (count > 0) {
            EXPECT_FLOAT_EQ(data.get_nodes()[0].normal_transform[0][0], 0.5F);
            EXPECT_FLOAT_EQ(data.get_nodes()[0].normal_transform[1][1], 1.0F / 3.0F);
            EXPECT_FLOAT_EQ(data.get_nodes()[0].normal_transform[2][2], 0.25F);
        }
    }
}

TEST(WBEGeometryDrawDataVKTest, RejectsCapacityOverflowWithoutChangingPublishedState) {
    RenderAllocator allocator(WBE_KI_B(64));
    const std::array<VkDrawIndexedIndirectCommand, 1> commands{{{.indexCount = 36, .instanceCount = 1}}};
    GeometryDrawDataVK data(&allocator, 1, commands);
    const std::array<glm::mat4, 2> transforms{glm::mat4(1.0F), glm::mat4(1.0F)};
    data.update({transforms.data(), 1});
    EXPECT_THROW(data.update(transforms), std::runtime_error);
    EXPECT_EQ(data.get_commands()[0].instanceCount, 1U);
    const std::array<glm::mat4, 1> singular{glm::mat4(0.0F)};
    EXPECT_THROW(data.update(singular), std::runtime_error);
    EXPECT_EQ(data.get_nodes()[0].transform, glm::mat4(1.0F));
    EXPECT_THROW(GeometryDrawDataVK(&allocator, 0, commands), std::runtime_error);
    EXPECT_THROW(GeometryDrawDataVK(&allocator, 2, commands), std::runtime_error);
}

} // namespace WhiteBirdEngine
