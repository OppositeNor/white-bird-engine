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
#include "core/allocator/stack_allocator.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render_vk/shader_metadata_vk.hh"
#include "function/render_vk/shader_metadata_vk_utils.hh"
#include "function/render_vk/vertex_input_description_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <array>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace WhiteBirdEngine {

struct VertexAttributeTestSpec {
    std::string_view role;
    RUVertexAttributeFormat format;
    size_t offset;
    size_t size;
};

static RefRes<char> push_vertex_test_data(StackAllocator& p_allocator, size_t p_size, const void* p_data) {
    MemID memory = p_allocator.allocate(p_size);
    if (p_size > 0) {
        std::memcpy(p_allocator.get(memory), p_data, p_size);
    }
    return RefRes<char>(memory, &p_allocator);
}

static RUVertexView make_vertex_test_data(StackAllocator& p_allocator, std::span<const VertexAttributeTestSpec> p_specs, size_t p_stride) {
    std::vector<RUVertexView::Attribute> attributes;
    attributes.reserve(p_specs.size());
    for (const VertexAttributeTestSpec& spec : p_specs) {
        RefRes<char> role_name = push_vertex_test_data(p_allocator, spec.role.size() + 1, spec.role.data());
        role_name.get()[spec.role.size()] = '\0';
        RUShaderRole role;
        role.name = role_name;
        role.hash = dynam_hash(spec.role);
        attributes.push_back(RUVertexView::Attribute{.role = role, .format = spec.format, .offset = spec.offset, .size = spec.size});
    }
    RUVertexView result;
    if (!attributes.empty()) {
        result.attributes =
            push_vertex_test_data(p_allocator, attributes.size() * sizeof(attributes[0]), attributes.data()).force_cast<RUVertexView::Attribute>();
    }
    result.attribute_count = attributes.size();
    result.vertex_stride = p_stride;
    return result;
}

static ShaderMetadataTypeField make_vertex_shader_field(
    std::string_view p_name, std::string_view p_role, int32_t p_location, std::string_view p_kind, std::string_view p_scalar_type, int32_t p_component_count) {
    ShaderMetadataTypeField field;
    field.name = p_name;
    field.binding.kind = "varyingInput";
    field.binding.index = p_location;
    field.type.kind = p_kind;
    if (p_kind == "scalar") {
        field.type.scalar_type = p_scalar_type;
    } else {
        field.type.element_scalar_type = p_scalar_type;
        field.type.element_count = p_component_count;
    }
    ShaderMetadataUserAttribute attribute;
    attribute.name = "WBE_VERTEX_INPUT";
    if (!p_role.empty()) {
        ShaderMetadataUserAttributeArgument argument;
        argument.kind = "string";
        argument.string_value = p_role;
        attribute.arguments.push_back(argument);
    }
    field.user_attributes.push_back(attribute);
    return field;
}

static ShaderMetadata make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField> p_fields) {
    ShaderMetadata metadata;
    ShaderMetadataEntryPoint entry_point;
    entry_point.name = "vertex_main";
    entry_point.stage = "vertex";
    ShaderMetadataParameter parameter;
    parameter.name = "p_input";
    parameter.type.kind = "struct";
    parameter.type.fields.assign(p_fields.begin(), p_fields.end());
    entry_point.parameters.push_back(parameter);
    metadata.entry_points.push_back(entry_point);
    return metadata;
}

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

TEST(WBEShaderMetadataVKUtilsTest, MapsReadWriteTextureResourcesToStorageImageDescriptors) {
    ShaderMetadata metadata;
    ShaderMetadataParameter output_image;
    output_image.name = "output_image";
    output_image.binding.kind = "descriptorTableSlot";
    output_image.binding.index = 0;
    output_image.type.kind = "resource";
    output_image.type.scalar_type = "texture2D";
    output_image.type.access = "readWrite";
    metadata.parameters.push_back(output_image);

    ShaderMetadataEntryPoint entry_point;
    entry_point.name = "compute_main";
    entry_point.stage = "compute";
    metadata.entry_points.push_back(entry_point);

    std::vector<ShaderDescriptorBindingVK> bindings = collect_shader_descriptor_bindings(metadata, "compute", "compute_main");
    ASSERT_EQ(bindings.size(), 1U);
    EXPECT_EQ(bindings[0].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    EXPECT_EQ(bindings[0].layout_binding.stageFlags, VK_SHADER_STAGE_COMPUTE_BIT);
}

TEST(WBEShaderMetadataVKUtilsTest, MapsStorageBuffersToStorageBufferDescriptors) {
    ShaderMetadataParameter instance_buffer;
    instance_buffer.name = "static_instances";
    instance_buffer.binding.kind = "descriptorTableSlot";
    instance_buffer.binding.index = 0;
    instance_buffer.type.kind = "structuredBuffer";

    ShaderMetadataParameter draw_info_block;
    draw_info_block.name = "static_draw_info";
    draw_info_block.binding.kind = "subElementRegisterSpace";
    draw_info_block.binding.index = 1;
    draw_info_block.type.kind = "parameterBlock";

    ShaderMetadataTypeField draw_infos;
    draw_infos.name = "draws";
    draw_infos.binding.kind = "descriptorTableSlot";
    draw_infos.binding.index = 2;
    draw_infos.type.kind = "array";
    draw_infos.type.element_count = 1;
    draw_infos.type.element_scalar_type = "structuredBuffer";
    draw_info_block.type.fields.push_back(draw_infos);

    ShaderMetadata metadata;
    metadata.parameters.push_back(instance_buffer);
    metadata.parameters.push_back(draw_info_block);
    ShaderMetadataEntryPoint entry_point;
    entry_point.name = "vertex_main";
    entry_point.stage = "vertex";
    metadata.entry_points.push_back(entry_point);

    std::vector<ShaderDescriptorBindingVK> bindings = collect_shader_descriptor_bindings(metadata, "vertex", "vertex_main");

    ASSERT_EQ(bindings.size(), 2U);
    EXPECT_EQ(bindings[0].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    EXPECT_EQ(bindings[0].layout_binding.stageFlags, VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_EQ(bindings[1].layout_binding.descriptorType, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    EXPECT_EQ(bindings[1].set_index, 1U);
    EXPECT_EQ(bindings[1].layout_binding.binding, 2U);
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

TEST(WBEShaderMetadataVKUtilsTest, BuildsEverySupportedDynamicVertexInputFormat) {
    struct FormatCase {
        RUVertexAttributeFormat resource_format;
        std::string_view kind;
        std::string_view scalar_type;
        int32_t component_count;
        size_t size;
        VkFormat vk_format;
    };
    const std::array<FormatCase, 12> cases{{
        {.resource_format = RUVertexAttributeFormat::FLOAT, .kind = "scalar", .scalar_type = "float32", .component_count = 1, .size = 4, .vk_format = VK_FORMAT_R32_SFLOAT},
        {.resource_format = RUVertexAttributeFormat::FLOAT2, .kind = "vector", .scalar_type = "float32", .component_count = 2, .size = 8, .vk_format = VK_FORMAT_R32G32_SFLOAT},
        {.resource_format = RUVertexAttributeFormat::FLOAT3, .kind = "vector", .scalar_type = "float32", .component_count = 3, .size = 12, .vk_format = VK_FORMAT_R32G32B32_SFLOAT},
        {.resource_format = RUVertexAttributeFormat::FLOAT4, .kind = "vector", .scalar_type = "float32", .component_count = 4, .size = 16, .vk_format = VK_FORMAT_R32G32B32A32_SFLOAT},
        {.resource_format = RUVertexAttributeFormat::INT, .kind = "scalar", .scalar_type = "int32", .component_count = 1, .size = 4, .vk_format = VK_FORMAT_R32_SINT},
        {.resource_format = RUVertexAttributeFormat::INT2, .kind = "vector", .scalar_type = "int32", .component_count = 2, .size = 8, .vk_format = VK_FORMAT_R32G32_SINT},
        {.resource_format = RUVertexAttributeFormat::INT3, .kind = "vector", .scalar_type = "int32", .component_count = 3, .size = 12, .vk_format = VK_FORMAT_R32G32B32_SINT},
        {.resource_format = RUVertexAttributeFormat::INT4, .kind = "vector", .scalar_type = "int32", .component_count = 4, .size = 16, .vk_format = VK_FORMAT_R32G32B32A32_SINT},
        {.resource_format = RUVertexAttributeFormat::UINT, .kind = "scalar", .scalar_type = "uint32", .component_count = 1, .size = 4, .vk_format = VK_FORMAT_R32_UINT},
        {.resource_format = RUVertexAttributeFormat::UINT2, .kind = "vector", .scalar_type = "uint32", .component_count = 2, .size = 8, .vk_format = VK_FORMAT_R32G32_UINT},
        {.resource_format = RUVertexAttributeFormat::UINT3, .kind = "vector", .scalar_type = "uint32", .component_count = 3, .size = 12, .vk_format = VK_FORMAT_R32G32B32_UINT},
        {.resource_format = RUVertexAttributeFormat::UINT4, .kind = "vector", .scalar_type = "uint32", .component_count = 4, .size = 16, .vk_format = VK_FORMAT_R32G32B32A32_UINT},
    }};
    for (const FormatCase& format_case : cases) {
        StackAllocator allocator(WBE_KI_B(8));
        const VertexAttributeTestSpec spec{
            .role = "value", .format = format_case.resource_format, .offset = 0, .size = format_case.size};
        RUVertexView vertex_data =
            make_vertex_test_data(allocator, std::span<const VertexAttributeTestSpec>(&spec, 1), format_case.size);
        const ShaderMetadataTypeField field =
            make_vertex_shader_field("value", "", 3, format_case.kind, format_case.scalar_type, format_case.component_count);
        ShaderMetadata metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&field, 1));

        VertexInputDescriptionVK description = build_vertex_input_description_vk(metadata, "vertex_main", vertex_data);
        ASSERT_EQ(description.bindings.size(), 1U);
        ASSERT_EQ(description.attributes.size(), 1U);
        EXPECT_EQ(description.bindings[0].stride, format_case.size);
        EXPECT_EQ(description.attributes[0].location, 3U);
        EXPECT_EQ(description.attributes[0].format, format_case.vk_format);
    }
}

TEST(WBEShaderMetadataVKUtilsTest, MatchesShaderSubsetAndAllowsExtraResourceAttributes) {
    StackAllocator allocator(WBE_KI_B(8));
    const std::array<VertexAttributeTestSpec, 2> specs{{
        {.role = "position", .format = RUVertexAttributeFormat::FLOAT3, .offset = 0, .size = 12},
        {.role = "uv", .format = RUVertexAttributeFormat::FLOAT2, .offset = 12, .size = 8},
    }};
    RUVertexView vertex_data = make_vertex_test_data(allocator, specs, 20);
    const ShaderMetadataTypeField field = make_vertex_shader_field("input_position", "position", 5, "vector", "float32", 3);
    ShaderMetadata metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&field, 1));

    VertexInputDescriptionVK description = build_vertex_input_description_vk(metadata, "vertex_main", vertex_data);
    ASSERT_EQ(description.bindings.size(), 1U);
    ASSERT_EQ(description.attributes.size(), 1U);
    EXPECT_EQ(description.bindings[0].stride, 20U);
    EXPECT_EQ(description.attributes[0].location, 5U);
    EXPECT_EQ(description.attributes[0].offset, 0U);
}

TEST(WBEShaderMetadataVKUtilsTest, FindsAnnotatedFieldsInNestedVertexStructs) {
    StackAllocator allocator(WBE_KI_B(8));
    const VertexAttributeTestSpec spec{.role = "position", .format = RUVertexAttributeFormat::FLOAT3, .offset = 0, .size = 12};
    RUVertexView vertex_data = make_vertex_test_data(allocator, std::span<const VertexAttributeTestSpec>(&spec, 1), 12);
    ShaderMetadataTypeField position = make_vertex_shader_field("position", "", 2, "vector", "float32", 3);
    ShaderMetadataTypeField nested;
    nested.name = "nested";
    nested.type.kind = "struct";
    nested.fields.push_back(position);
    ShaderMetadata metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&nested, 1));

    VertexInputDescriptionVK description = build_vertex_input_description_vk(metadata, "vertex_main", vertex_data);

    ASSERT_EQ(description.attributes.size(), 1U);
    EXPECT_EQ(description.attributes[0].location, 2U);
    EXPECT_EQ(description.attributes[0].format, VK_FORMAT_R32G32B32_SFLOAT);
}

TEST(WBEShaderMetadataVKUtilsTest, RejectsInvalidVertexInputContracts) {
    StackAllocator allocator(WBE_KI_B(32));
    const std::array<VertexAttributeTestSpec, 2> specs{{
        {.role = "position", .format = RUVertexAttributeFormat::FLOAT3, .offset = 0, .size = 12},
        {.role = "uv", .format = RUVertexAttributeFormat::FLOAT2, .offset = 12, .size = 8},
    }};
    RUVertexView vertex_data = make_vertex_test_data(allocator, specs, 20);

    ShaderMetadataTypeField missing = make_vertex_shader_field("missing", "missing", 0, "vector", "float32", 3);
    ShaderMetadata missing_metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&missing, 1));
    EXPECT_THROW(build_vertex_input_description_vk(missing_metadata, "vertex_main", vertex_data), std::runtime_error);

    ShaderMetadataTypeField incompatible = make_vertex_shader_field("position", "", 0, "vector", "uint32", 3);
    ShaderMetadata incompatible_metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&incompatible, 1));
    EXPECT_THROW(build_vertex_input_description_vk(incompatible_metadata, "vertex_main", vertex_data), std::runtime_error);

    std::array<ShaderMetadataTypeField, 2> duplicate_role_fields{{
        make_vertex_shader_field("first", "position", 0, "vector", "float32", 3),
        make_vertex_shader_field("second", "position", 1, "vector", "float32", 3),
    }};
    ShaderMetadata duplicate_role_metadata = make_vertex_shader_metadata(duplicate_role_fields);
    EXPECT_THROW(build_vertex_input_description_vk(duplicate_role_metadata, "vertex_main", vertex_data), std::runtime_error);

    std::array<ShaderMetadataTypeField, 2> duplicate_location_fields{{
        make_vertex_shader_field("position", "", 0, "vector", "float32", 3),
        make_vertex_shader_field("uv", "", 0, "vector", "float32", 2),
    }};
    ShaderMetadata duplicate_location_metadata = make_vertex_shader_metadata(duplicate_location_fields);
    EXPECT_THROW(build_vertex_input_description_vk(duplicate_location_metadata, "vertex_main", vertex_data), std::runtime_error);

    ShaderMetadataTypeField malformed = make_vertex_shader_field("position", "", 0, "vector", "float32", 3);
    malformed.user_attributes[0].arguments.emplace_back();
    malformed.user_attributes[0].arguments[0].kind = "int";
    ShaderMetadata malformed_metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&malformed, 1));
    EXPECT_THROW(build_vertex_input_description_vk(malformed_metadata, "vertex_main", vertex_data), std::runtime_error);

    ShaderMetadataTypeField repeated = make_vertex_shader_field("position", "", 0, "vector", "float32", 3);
    repeated.user_attributes.push_back(repeated.user_attributes[0]);
    ShaderMetadata repeated_metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&repeated, 1));
    EXPECT_THROW(build_vertex_input_description_vk(repeated_metadata, "vertex_main", vertex_data), std::runtime_error);

    ShaderMetadataTypeField unsupported = make_vertex_shader_field("position", "", 0, "matrix", "float32", 3);
    ShaderMetadata unsupported_metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&unsupported, 1));
    EXPECT_THROW(build_vertex_input_description_vk(unsupported_metadata, "vertex_main", vertex_data), std::runtime_error);

    const std::array<VertexAttributeTestSpec, 2> duplicate_specs{{
        {.role = "position", .format = RUVertexAttributeFormat::FLOAT3, .offset = 0, .size = 12},
        {.role = "position", .format = RUVertexAttributeFormat::FLOAT3, .offset = 12, .size = 12},
    }};
    RUVertexView duplicate_resource = make_vertex_test_data(allocator, duplicate_specs, 24);
    ShaderMetadataTypeField position = make_vertex_shader_field("position", "", 0, "vector", "float32", 3);
    ShaderMetadata position_metadata = make_vertex_shader_metadata(std::span<const ShaderMetadataTypeField>(&position, 1));
    EXPECT_THROW(build_vertex_input_description_vk(position_metadata, "vertex_main", duplicate_resource), std::runtime_error);
}

TEST(WBEShaderMetadataVKUtilsTest, BuildsEmptyStateForVertexlessEntry) {
    StackAllocator allocator(WBE_KI_B(4));
    RUVertexView vertex_data = make_vertex_test_data(allocator, {}, 0);
    ShaderMetadata metadata = make_vertex_shader_metadata({});
    VertexInputDescriptionVK description = build_vertex_input_description_vk(metadata, "vertex_main", vertex_data);
    EXPECT_TRUE(description.bindings.empty());
    EXPECT_TRUE(description.attributes.empty());
}

} // namespace WhiteBirdEngine
