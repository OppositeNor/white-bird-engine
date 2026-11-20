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
#include "function/render/i_renderer.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/desc_set_plugin_vk.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TEST(WBEDescSetPluginVKTest, EnablesOnlyWhenRequested) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    DescSetPluginVK plugin(&rend_data);

    RendererCapabilities capabilities{};
    EXPECT_FALSE(plugin.try_enable(capabilities, VK_NULL_HANDLE, nullptr, 0));
    EXPECT_FALSE(plugin.is_enabled());

    capabilities.desc_set = true;
    EXPECT_TRUE(plugin.try_enable(capabilities, VK_NULL_HANDLE, nullptr, 0));
    EXPECT_TRUE(plugin.is_enabled());
    EXPECT_TRUE(capabilities.desc_set);
    EXPECT_TRUE(plugin.get_required_instance_extensions().empty());
    EXPECT_TRUE(plugin.get_required_device_extensions().empty());
}

TEST(WBEDescSetPluginVKTest, RegistersSingleton) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    EXPECT_EQ(DescSetPluginVK::get_singleton(), nullptr);
    {
        DescSetPluginVK plugin(&rend_data);
        EXPECT_EQ(DescSetPluginVK::get_singleton(), &plugin);
    }
    EXPECT_EQ(DescSetPluginVK::get_singleton(), nullptr);
}

TEST(WBEDescSetPluginVKTest, ThrowsBeforeVulkanObjectsExist) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    DescSetPluginVK plugin(&rend_data);

    EXPECT_THROW(static_cast<void>(plugin.get_instance()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(plugin.get_desc_pool()), std::runtime_error);
    EXPECT_THROW(static_cast<void>(plugin.allocate_desc_set(VK_NULL_HANDLE)), std::runtime_error);
    EXPECT_THROW(plugin.free_desc_set(VK_NULL_HANDLE), std::runtime_error);

    auto* instance = reinterpret_cast<VkInstance>(uintptr_t{1}); // NOLINT
    plugin.on_after_instance_created(instance);
    EXPECT_EQ(plugin.get_instance(), instance);
}

} // namespace WhiteBirdEngine
