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
#include "function/render/shader_macro_value.hh"
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
#include <vector>

namespace WhiteBirdEngine {

namespace {

const std::vector<ShaderMacroDeclaration> DECLARATIONS = {
    ShaderMacroDeclaration{.macro = "SCALE", .options = {"1", "2"}, .optional = false},
    ShaderMacroDeclaration{.macro = "EXTRA", .options = {"1"}, .optional = true},
    ShaderMacroDeclaration{.macro = "FIXED", .options = {"7"}, .optional = false},
};

struct TestVariant {
    HashCode key = 0;
    int value = 0;
};

bool has_field(const ShaderMetadataParameter& p_parameter, std::string_view p_name) {
    return std::ranges::any_of(p_parameter.type.fields, [p_name](const ShaderMetadataTypeField& p_field) {
        return p_field.name == p_name;
    });
}

} // namespace

TEST(WBEShaderVariantResolverTest, KeysFollowTheChainedHashOfSortedMacros) {
    const ShaderVariantResolver resolver;
    HashCode expected = dynam_hash(std::string_view("A"));
    expected = dynam_hash_append(expected, "=");
    expected = dynam_hash_append(expected, "1");
    expected = dynam_hash_append(expected, ",");
    expected = dynam_hash_append(expected, "B");
    expected = dynam_hash_append(expected, "=");
    expected = dynam_hash_append(expected, "2");

    EXPECT_EQ(resolver.make_key({{.macro = "B", .value = "2"}, {.macro = "A", .value = "1"}}), expected);
    EXPECT_EQ(resolver.make_key({{.macro = "A", .value = "1"}, {.macro = "B", .value = "2"}}), expected);
    EXPECT_EQ(resolver.make_key({}), dynam_hash(std::string_view()));
}

TEST(WBEShaderVariantResolverTest, FillsOptionalAndSingleOptionDefaults) {
    const ShaderVariantResolver resolver;

    EXPECT_EQ(resolver.resolve_key(DECLARATIONS, {{.macro = "SCALE", .value = "2"}}),
        resolver.make_key({{.macro = "SCALE", .value = "2"}, {.macro = "FIXED", .value = "7"}}));
    EXPECT_EQ(resolver.resolve_key(DECLARATIONS, {{.macro = "EXTRA", .value = "1"}, {.macro = "SCALE", .value = "1"}}),
        resolver.make_key({{.macro = "SCALE", .value = "1"}, {.macro = "EXTRA", .value = "1"}, {.macro = "FIXED", .value = "7"}}));
    EXPECT_EQ(resolver.resolve_key({}, {}), resolver.make_key({}));
}

TEST(WBEShaderVariantResolverTest, RejectsInvalidSelections) {
    const ShaderVariantResolver resolver;

    // SCALE has several options, so it must be selected.
    EXPECT_THROW((void)resolver.resolve_key(DECLARATIONS, {}), std::runtime_error);
    EXPECT_THROW((void)resolver.resolve_key(DECLARATIONS, {{.macro = "SCALE", .value = "1"}, {.macro = "UNKNOWN", .value = "1"}}),
        std::runtime_error);
    EXPECT_THROW((void)resolver.resolve_key(DECLARATIONS, {{.macro = "SCALE", .value = "3"}}), std::runtime_error);
    EXPECT_THROW((void)resolver.resolve_key(DECLARATIONS, {{.macro = "SCALE", .value = "1"}, {.macro = "SCALE", .value = "2"}}),
        std::runtime_error);
}

TEST(WBEShaderVariantResolverTest, FindsKeysByBinarySearch) {
    const ShaderVariantResolver resolver;
    const std::vector<TestVariant> variants = {{.key = 3, .value = 30}, {.key = 5, .value = 50}, {.key = 9, .value = 90}};

    EXPECT_EQ(resolver.find(std::span<const TestVariant>(variants), 5, "test").value, 50);
    EXPECT_EQ(resolver.find(std::span<const TestVariant>(variants), 9, "test").value, 90);
    EXPECT_THROW((void)resolver.find(std::span<const TestVariant>(variants), 4, "test"), std::runtime_error);
    EXPECT_THROW((void)resolver.find(std::span<const TestVariant>(variants), 10, "test"), std::runtime_error);
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

TEST_F(WBEShaderVariantResourceTest, LoadsEveryMacroCombinationSortedByKey) {
    ASSERT_NE(shader, nullptr);
    // SCALE in {1, 2} times EXTRA in {undefined, 1}.
    ASSERT_EQ(shader->variant_count, 4);
    std::span<const RUShaderVariant> variants = shader->get_variants();
    for (size_t i = 1; i < variants.size(); ++i) {
        EXPECT_LT(variants[i - 1].key, variants[i].key);
    }
    for (const RUShaderVariant& variant : variants) {
        EXPECT_NE(variant.data, nullptr);
        EXPECT_GT(variant.size, 0);
    }
    const std::vector<ShaderMacroDeclaration> declarations = shader->get_macro_declarations();
    ASSERT_EQ(declarations.size(), 2);
    EXPECT_EQ(declarations[0].macro, "WBE_SHADER_VARIANT_SCALE");
    EXPECT_EQ(declarations[0].options, (std::vector<std::string>{"1", "2"}));
    EXPECT_FALSE(declarations[0].optional);
    EXPECT_EQ(declarations[1].macro, "WBE_SHADER_VARIANT_EXTRA");
    EXPECT_TRUE(declarations[1].optional);
}

TEST_F(WBEShaderVariantResourceTest, SelectsVariantsByMacroValues) {
    ASSERT_NE(shader, nullptr);
    const RUShaderVariant& scale_1 = shader->get_variant({{.macro = "WBE_SHADER_VARIANT_SCALE", .value = "1"}});
    const RUShaderVariant& scale_2 = shader->get_variant({{.macro = "WBE_SHADER_VARIANT_SCALE", .value = "2"}});
    const RUShaderVariant& scale_2_extra = shader->get_variant(
        {{.macro = "WBE_SHADER_VARIANT_EXTRA", .value = "1"}, {.macro = "WBE_SHADER_VARIANT_SCALE", .value = "2"}});

    EXPECT_NE(&scale_1, &scale_2);
    EXPECT_NE(&scale_2, &scale_2_extra);
    EXPECT_EQ(&scale_2_extra,
        &shader->get_variant({{.macro = "WBE_SHADER_VARIANT_SCALE", .value = "2"}, {.macro = "WBE_SHADER_VARIANT_EXTRA", .value = "1"}}));
    // The scale has two options, so the default selection names no variant.
    EXPECT_THROW((void)shader->get_variant(), std::runtime_error);
    EXPECT_THROW((void)shader->get_variant({{.macro = "WBE_SHADER_VARIANT_SCALE", .value = "3"}}), std::runtime_error);
}

TEST_F(WBEShaderVariantResourceTest, LoadsMetadataOfTheSelectedVariant) {
    ASSERT_NE(shader, nullptr);
    const ShaderMetadataQuery query;
    const ShaderMetadata plain = ShaderMetadataLoader().load(shader, {{.macro = "WBE_SHADER_VARIANT_SCALE", .value = "1"}});
    const ShaderMetadata extra = ShaderMetadataLoader().load(
        shader, {{.macro = "WBE_SHADER_VARIANT_SCALE", .value = "1"}, {.macro = "WBE_SHADER_VARIANT_EXTRA", .value = "1"}});

    const ShaderMetadataParameter* plain_indices = query.find_parameter(plain, "shader_variant_indices");
    const ShaderMetadataParameter* extra_indices = query.find_parameter(extra, "shader_variant_indices");
    ASSERT_NE(plain_indices, nullptr);
    ASSERT_NE(extra_indices, nullptr);
    EXPECT_TRUE(has_field(*plain_indices, "output_buffer"));
    EXPECT_FALSE(has_field(*plain_indices, "extra_buffer"));
    EXPECT_TRUE(has_field(*extra_indices, "extra_buffer"));
}

} // namespace WhiteBirdEngine
