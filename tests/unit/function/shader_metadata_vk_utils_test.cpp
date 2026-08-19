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

TEST(WBEShaderMetadataVKUtilsTest, MergesGeometryStageDescriptorBindingsIntoGraphicsPipelineLayouts) {
    ShaderMetadata vertex_metadata;
    ShaderMetadataParameter vertex_uniform;
    vertex_uniform.name = "vertex_ubo";
    vertex_uniform.binding.kind = "descriptorTableSlot";
    vertex_uniform.binding.space = 0;
    vertex_uniform.binding.index = 0;
    vertex_uniform.binding.size = 64;
    vertex_uniform.type.kind = "constantBuffer";
    vertex_metadata.parameters.push_back(vertex_uniform);
    ShaderMetadataEntryPoint vertex_entry_point;
    vertex_entry_point.name = "vertex_main";
    vertex_entry_point.stage = "vertex";
    vertex_metadata.entry_points.push_back(vertex_entry_point);

    ShaderMetadata geometry_metadata;
    ShaderMetadataParameter geometry_texture;
    geometry_texture.name = "geometry_texture";
    geometry_texture.binding.kind = "descriptorTableSlot";
    geometry_texture.binding.space = 1;
    geometry_texture.binding.index = 2;
    geometry_texture.type.kind = "resource";
    geometry_texture.type.scalar_type = "texture2D";
    geometry_metadata.parameters.push_back(geometry_texture);
    ShaderMetadataEntryPoint geometry_entry_point;
    geometry_entry_point.name = "geometry_main";
    geometry_entry_point.stage = "geometry";
    geometry_metadata.entry_points.push_back(geometry_entry_point);

    ShaderMetadata fragment_metadata;
    ShaderMetadataParameter fragment_sampler;
    fragment_sampler.name = "fragment_sampler";
    fragment_sampler.binding.kind = "descriptorTableSlot";
    fragment_sampler.binding.space = 1;
    fragment_sampler.binding.index = 3;
    fragment_sampler.type.kind = "samplerState";
    fragment_metadata.parameters.push_back(fragment_sampler);
    ShaderMetadataEntryPoint fragment_entry_point;
    fragment_entry_point.name = "fragment_main";
    fragment_entry_point.stage = "fragment";
    fragment_metadata.entry_points.push_back(fragment_entry_point);

    std::vector<ShaderDescriptorBindingVK> bindings = collect_graphics_pipeline_descriptor_bindings(
        vertex_metadata, "vertex_main", &geometry_metadata, "geometry_main", fragment_metadata, "fragment_main");

    ASSERT_EQ(bindings.size(), 3U);
    EXPECT_EQ(bindings[0].parameter_name, "vertex_ubo");
    EXPECT_EQ(bindings[0].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    EXPECT_EQ(bindings[0].layout_binding.stageFlags, VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_EQ(bindings[1].parameter_name, "geometry_texture");
    EXPECT_EQ(bindings[1].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    EXPECT_EQ(bindings[1].layout_binding.stageFlags, VK_SHADER_STAGE_GEOMETRY_BIT);
    EXPECT_EQ(bindings[2].parameter_name, "fragment_sampler");
    EXPECT_EQ(bindings[2].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_SAMPLER);
    EXPECT_EQ(bindings[2].layout_binding.stageFlags, VK_SHADER_STAGE_FRAGMENT_BIT);
}

} // namespace WhiteBirdEngine
