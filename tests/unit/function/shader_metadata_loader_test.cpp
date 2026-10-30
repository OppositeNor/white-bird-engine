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
#include "core/allocator/stack_allocator.hh"
#include "function/render/render_resource_unit.hh"
#include "function/render/shader_metadata.hh"
#include "function/render/shader_metadata_loader.hh"
#include "function/render/shader_metadata_query.hh"
#include "global/global.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "test_global.hh"
#include "utils/defs.hh"
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace WhiteBirdEngine {

namespace {

RefRes<RUShader> make_test_shader(StackAllocator& p_allocator, std::string_view p_metadata_path) {
    MemID shader_memory = p_allocator.allocate(sizeof(RUShader));
    std::memset(p_allocator.get(shader_memory), 0, sizeof(RUShader));
    RefRes<RUShader> shader(shader_memory, &p_allocator);
    if (!p_metadata_path.empty()) {
        MemID path_memory = p_allocator.allocate(p_metadata_path.size() + 1);
        auto* path_storage = static_cast<char*>(p_allocator.get(path_memory));
        std::memcpy(path_storage, p_metadata_path.data(), p_metadata_path.size());
        path_storage[p_metadata_path.size()] = '\0';
        shader->metadata_path = RefRes<char>(path_memory, &p_allocator);
    }
    return shader;
}

} // namespace

class WBEShaderMetadataLoaderTest : public ::testing::Test {
protected:
    virtual void SetUp() override {
        global = make_test_global();
    }

    virtual void TearDown() override {
        global.reset();
    }

    std::unique_ptr<Global> global;
    StackAllocator storage{WBE_KI_B(4)};
};

TEST_F(WBEShaderMetadataLoaderTest, LoadsMetadataFromResourceRelativePath) {
    const ShaderMetadataLoader loader;
    RefRes<RUShader> shader =
        make_test_shader(storage, "assets/shaders/descriptor_heap_reflection/descriptor_heap_reflection.descriptor_heap_reflection_shader.json");

    const ShaderMetadata metadata = loader.load(shader);

    const ShaderMetadataQuery query;
    const ShaderMetadataParameter* push_data = query.find_parameter(metadata, "descriptor_indices");
    ASSERT_NE(push_data, nullptr);
    EXPECT_EQ(push_data->binding.kind, "pushConstantBuffer");
    EXPECT_NE(query.find_parameter(metadata, "acceleration_structure"), nullptr);
}

TEST_F(WBEShaderMetadataLoaderTest, RejectsNullShader) {
    const ShaderMetadataLoader loader;

    EXPECT_THROW((void)loader.load(RefRes<RUShader>()), std::runtime_error);
}

TEST_F(WBEShaderMetadataLoaderTest, RejectsShaderWithoutMetadataPath) {
    const ShaderMetadataLoader loader;
    RefRes<RUShader> shader = make_test_shader(storage, "");

    EXPECT_THROW((void)loader.load(shader), std::runtime_error);
}

TEST_F(WBEShaderMetadataLoaderTest, RejectsMissingMetadataFile) {
    const ShaderMetadataLoader loader;
    RefRes<RUShader> shader = make_test_shader(storage, "assets/shaders/does_not_exist/does_not_exist.json");

    EXPECT_THROW((void)loader.load(shader), std::runtime_error);
}

} // namespace WhiteBirdEngine
