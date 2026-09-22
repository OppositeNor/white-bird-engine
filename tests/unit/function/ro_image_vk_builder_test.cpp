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
#include "function/render_vk/ro_image_vk.hh"
#include "function/function_utils.hh"
#include <gtest/gtest.h>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TEST(WBEROImageVKBuilderTest, SetProperties2DConfiguresRequiredProperties) {
    constexpr VkExtent2D IMAGE_EXTENT{.width = 640, .height = 480};
    constexpr VkImageUsageFlags IMAGE_USAGE = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ROImageVK::Builder builder;

    ROImageVK::Builder& result = builder.set_properties_2d(VK_FORMAT_R8G8B8A8_UNORM, IMAGE_EXTENT, IMAGE_USAGE);

    EXPECT_EQ(&result, &builder);
    EXPECT_EQ(WBE_REQUIRED(builder.image_type), VK_IMAGE_TYPE_2D);
    EXPECT_EQ(WBE_REQUIRED(builder.format), VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(WBE_REQUIRED(builder.extent).width, IMAGE_EXTENT.width);
    EXPECT_EQ(WBE_REQUIRED(builder.extent).height, IMAGE_EXTENT.height);
    EXPECT_EQ(WBE_REQUIRED(builder.extent).depth, 1U);
    EXPECT_EQ(WBE_REQUIRED(builder.usage), IMAGE_USAGE);
}

} // namespace WhiteBirdEngine
