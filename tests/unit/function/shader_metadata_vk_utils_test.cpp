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
#include "function/render_vk/shader_metadata_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

TEST(WBEShaderMetadataVKUtilsTest, MarksRuntimeSampledImageArrayAsVariableDescriptorBinding) {
    ShaderMetadataParameter material_textures;
    material_textures.name = "material_textures";
    material_textures.binding.kind = "subElementRegisterSpace";
    material_textures.binding.index = 2;
    material_textures.type.kind = "parameterBlock";

    ShaderMetadataTypeField sampler;
    sampler.name = "material_sampler";
    sampler.type.kind = "samplerState";
    sampler.binding.kind = "descriptorTableSlot";
    sampler.binding.index = 0;
    material_textures.type.fields.push_back(sampler);

    ShaderMetadataTypeField textures;
    textures.name = "textures";
    textures.type.kind = "array";
    textures.type.element_count = 0;
    textures.type.element_scalar_type = "texture2D";
    textures.binding.kind = "descriptorTableSlot";
    textures.binding.index = 1;
    material_textures.type.fields.push_back(textures);

    ShaderMetadata metadata;
    metadata.parameters.push_back(material_textures);
    ShaderMetadataEntryPoint entry_point;
    entry_point.name = "fragment_main";
    entry_point.stage = "fragment";
    metadata.entry_points.push_back(entry_point);

    std::vector<ShaderDescriptorBindingVK> bindings = collect_shader_descriptor_bindings(metadata, "fragment", "fragment_main");
    ASSERT_EQ(bindings.size(), 2);
    EXPECT_EQ(bindings[0].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_SAMPLER);
    EXPECT_FALSE(bindings[0].variable_descriptor_count);
    EXPECT_EQ(bindings[1].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    EXPECT_EQ(bindings[1].layout_binding.descriptorCount, 0);
    EXPECT_TRUE(bindings[1].variable_descriptor_count);

    std::vector<ShaderDescriptorSetLayoutVK> set_layouts = build_graphics_pipeline_descriptor_set_layout_bindings(bindings);
    ASSERT_EQ(set_layouts.size(), 3);
    ASSERT_EQ(set_layouts[2].bindings.size(), 2);
    ASSERT_EQ(set_layouts[2].binding_flags.size(), 2);
    EXPECT_EQ(set_layouts[2].binding_flags[0], 0);
    EXPECT_EQ(set_layouts[2].binding_flags[1], VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

    set_variable_descriptor_count_upper_bounds(set_layouts, 10);
    EXPECT_EQ(set_layouts[2].bindings[1].descriptorCount, 9);
}

TEST(WBEShaderMetadataVKUtilsTest, SharesVariableDescriptorBudgetAcrossDescriptorSets) {
    ShaderDescriptorSetLayoutVK first_set;
    first_set.set_index = 0;
    first_set.bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 2,
    });
    first_set.binding_flags.push_back(0);
    first_set.bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    });
    first_set.binding_flags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

    ShaderDescriptorSetLayoutVK second_set;
    second_set.set_index = 1;
    second_set.bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    });
    second_set.binding_flags.push_back(0);
    second_set.bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    });
    second_set.binding_flags.push_back(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

    std::vector<ShaderDescriptorSetLayoutVK> set_layouts{first_set, second_set};
    set_variable_descriptor_count_upper_bounds(set_layouts, 10);
    EXPECT_EQ(set_layouts[0].bindings[1].descriptorCount, 3);
    EXPECT_EQ(set_layouts[1].bindings[1].descriptorCount, 3);

    EXPECT_THROW(set_variable_descriptor_count_upper_bounds(set_layouts, 4), std::runtime_error);
}

} // namespace WhiteBirdEngine
