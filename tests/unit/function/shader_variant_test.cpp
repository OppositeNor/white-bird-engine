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
#include "core/memory/reference_strong.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render/shader_metadata_query.hh"
#include "function/render/shader_variant_resolver.hh"
#include "function/rend_vk/rend_vk_resource_unit_loader.hh"
#include "global/global.hh"
#include "resource/resource_chunk/resource_chunk_load_file.hh"
#include "resource/resource_manager/resource_manager.hh"
#include "test_global.hh"
#include "utils/utils.hh"
#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {

namespace {

struct TestVariant {
    std::string_view name;
    int value = 0;
};

std::string_view get_test_variant_name(const TestVariant& p_variant) {
    return p_variant.name;
}

bool has_field(const ShaderMetadataParameter& p_parameter, std::string_view p_name) {
    return std::ranges::any_of(p_parameter.type.fields, [p_name](const ShaderMetadataTypeField& p_field) {
        return p_field.name == p_name;
    });
}

} // namespace

TEST(WBEShaderVariantResolverTest, FindsVariantsByNameAndEmptyNamesAsTheDefault) {
    const ShaderVariantResolver resolver;
    const std::vector<TestVariant> variants = {{.name = "a", .value = 1}, {.name = "b", .value = 2}, {.name = "c", .value = 3}};
    const std::span<const TestVariant> table(variants);

    EXPECT_EQ(resolver.find(table, 1, "a", get_test_variant_name, "test").value, 1);
    EXPECT_EQ(resolver.find(table, 1, "c", get_test_variant_name, "test").value, 3);
    EXPECT_EQ(resolver.find(table, 1, "", get_test_variant_name, "test").value, 2);
    EXPECT_THROW((void)resolver.find(table, 1, "d", get_test_variant_name, "test"), std::runtime_error);
    EXPECT_THROW((void)resolver.find(table, 1, "0", get_test_variant_name, "test"), std::runtime_error);
}

class WBEShaderVariantResourceTest : public ::testing::Test {
protected:
    virtual void SetUp() override {
        global = make_test_global();
        register_renderer_vk_resource_unit_loaders();
        ResourceManager* resource_manager = ResourceManager::get_singleton();
        resource_manager->register_chunk(
            make_ref<ResourceChunkLoadFile>(global_alloc(), "res_chunks/shader_variants.json", resource_manager->get_allocator()));
        shader = resource_manager->get_resource<RUShader>("shader_variants_shader");
    }

    virtual void TearDown() override {
        shader = RefRes<RUShader>();
        global.reset();
    }

    std::unique_ptr<Global> global;
    RefRes<RUShader> shader;
};

TEST_F(WBEShaderVariantResourceTest, LoadsEveryVariantSortedByName) {
    ASSERT_NE(shader, nullptr);
    ASSERT_EQ(shader->variant_count, 4);
    std::span<const RUShaderVariant> variants = shader->get_variants();
    EXPECT_STREQ(variants[0].name.get(), "scale_1");
    EXPECT_STREQ(variants[1].name.get(), "scale_1_extra");
    EXPECT_STREQ(variants[2].name.get(), "scale_2");
    EXPECT_STREQ(variants[3].name.get(), "scale_2_extra");
    for (const RUShaderVariant& variant : variants) {
        EXPECT_NE(variant.data, nullptr);
        EXPECT_GT(variant.size, 0);
    }
}

TEST_F(WBEShaderVariantResourceTest, SelectsVariantsByName) {
    ASSERT_NE(shader, nullptr);
    const RUShaderVariant& scale_1 = shader->get_variant("scale_1");
    const RUShaderVariant& scale_2 = shader->get_variant("scale_2");

    EXPECT_STREQ(scale_1.name.get(), "scale_1");
    EXPECT_STREQ(scale_2.name.get(), "scale_2");
    // scale_2 carries the default tag, though it is not the first variant.
    EXPECT_EQ(&shader->get_variant(), &scale_2);
    EXPECT_THROW((void)shader->get_variant("scale_3"), std::runtime_error);
}

TEST_F(WBEShaderVariantResourceTest, LoadsMetadataOfTheSelectedVariant) {
    ASSERT_NE(shader, nullptr);
    const ShaderMetadataQuery query;
    const ShaderMetadata plain = ShaderMetadataLoader().load(shader, "scale_1");
    const ShaderMetadata extra = ShaderMetadataLoader().load(shader, "scale_1_extra");

    const ShaderMetadataParameter* plain_indices = query.find_parameter(plain, "shader_variant_indices");
    const ShaderMetadataParameter* extra_indices = query.find_parameter(extra, "shader_variant_indices");
    ASSERT_NE(plain_indices, nullptr);
    ASSERT_NE(extra_indices, nullptr);
    EXPECT_TRUE(has_field(*plain_indices, "output_buffer"));
    EXPECT_FALSE(has_field(*plain_indices, "extra_buffer"));
    EXPECT_TRUE(has_field(*extra_indices, "extra_buffer"));
}

} // namespace WhiteBirdEngine
