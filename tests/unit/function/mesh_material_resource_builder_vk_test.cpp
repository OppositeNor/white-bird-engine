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
#include "core/alloc/i_alloc.hh"
#include "core/alloc/stack_alloc.hh"
#include "core/core_utils.hh"
#include "function/render/material_texture_contract.hh"
#include "function/render/rend_resource_unit.hh"
#include "function/render/shader_role.hh"
#include "function/rend_vk/mesh_material_resource_builder_vk.hh"
#include "resource/resource_unit/resource_unit.hh"
#include "utils/defs.hh"
#include "utils/utils.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace WhiteBirdEngine {
namespace {

template <typename TValue>
RefRes<TValue> push_test_object(StackAlloc& p_alloc, const TValue& p_value) {
    MemID memory = p_alloc.allocate(sizeof(TValue));
    auto* object = static_cast<TValue*>(p_alloc.get(memory));
    std::construct_at(object, p_value);
    return RefRes<TValue>(memory, &p_alloc);
}

template <typename TValue>
RefRes<TValue> push_test_array(StackAlloc& p_alloc, std::span<const TValue> p_values) {
    if (p_values.empty()) {
        return MEM_NULL;
    }
    MemID memory = p_alloc.allocate(sizeof(TValue) * p_values.size());
    auto* array = static_cast<TValue*>(p_alloc.get(memory));
    for (size_t i = 0; i < p_values.size(); ++i) {
        std::construct_at(array + i, p_values[i]);
    }
    return RefRes<TValue>(memory, &p_alloc);
}

RUShaderRole make_test_shader_role(StackAlloc& p_alloc, std::string_view p_name) {
    MemID memory = p_alloc.allocate(p_name.size() + 1);
    auto* name_storage = static_cast<char*>(p_alloc.get(memory));
    std::memcpy(name_storage, p_name.data(), p_name.size());
    name_storage[p_name.size()] = '\0';
    return RUShaderRole(RefRes<char>(memory, &p_alloc), dynam_hash(p_name));
}

RefRes<RUTexture> make_test_texture_ref(StackAlloc& p_alloc) {
    MemID memory = p_alloc.allocate(sizeof(RUTexture));
    std::memset(p_alloc.get(memory), 0, sizeof(RUTexture));
    return RefRes<RUTexture>(memory, &p_alloc);
}

RefRes<RUMaterial> make_test_material(
    StackAlloc& p_alloc, std::string_view p_resource_key, std::span<const RUMaterial::TextureSlot> p_texture_slots) {
    RUMaterial material;
    material.resource_key = nullptr;
    if (!p_resource_key.empty()) {
        MemID key_memory = p_alloc.allocate(p_resource_key.size() + 1);
        auto* key_storage = static_cast<char*>(p_alloc.get(key_memory));
        std::memcpy(key_storage, p_resource_key.data(), p_resource_key.size());
        key_storage[p_resource_key.size()] = '\0';
        material.resource_key = key_storage;
    }
    material.textures = push_test_array(p_alloc, p_texture_slots);
    material.texture_count = p_texture_slots.size();
    return push_test_object(p_alloc, material);
}

MaterialTextureContract make_material_contract(std::string_view p_second_role = "normal") {
    MaterialTextureContract result;
    result.table_offset = 16;
    result.table_stride = 16;
    result.table_size = 16 + 16 * 4;
    result.material_count = 4;
    result.texture_fields.push_back({.role = ShaderRole("albedo"), .offset = 0});
    result.texture_fields.push_back({.role = ShaderRole(p_second_role), .offset = 4});
    result.texture_fields.push_back({.role = ShaderRole("rma"), .offset = 8});
    return result;
}

int32_t read_int32(const std::vector<uint8_t>& p_buffer, size_t p_offset) {
    int32_t result = -1;
    std::memcpy(&result, p_buffer.data() + p_offset, sizeof(result));
    return result;
}

} // namespace

TEST(WBEMeshMaterialResourceBuilderVKTest, BuildsPackedRowsAndDeduplicatesTextures) {
    StackAlloc alloc(WBE_KI_B(16));
    RefRes<RUTexture> default_albedo = make_test_texture_ref(alloc);
    RefRes<RUTexture> default_normal = make_test_texture_ref(alloc);
    RefRes<RUTexture> default_rma = make_test_texture_ref(alloc);
    RefRes<RUTexture> bound_albedo = make_test_texture_ref(alloc);
    RefRes<RUTexture> bound_rma = make_test_texture_ref(alloc);

    std::array<RUMaterial::TextureSlot, 3> default_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = default_albedo},
        {.role = make_test_shader_role(alloc, "normal"), .texture = default_normal},
        {.role = make_test_shader_role(alloc, "rma"), .texture = default_rma},
    }};
    std::array<RUMaterial::TextureSlot, 2> first_material_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = bound_albedo},
        {.role = make_test_shader_role(alloc, "rma"), .texture = bound_rma},
    }};
    std::array<RUMaterial::TextureSlot, 1> second_material_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = bound_albedo},
    }};

    RefRes<RUMaterial> default_material = make_test_material(alloc, "default_material", default_slots);
    RefRes<RUMaterial> first_material = make_test_material(alloc, "first_material", first_material_slots);
    RefRes<RUMaterial> second_material = make_test_material(alloc, "second_material", second_material_slots);
    const std::array<RefRes<RUMaterial>, 2> ordered_materials{first_material, second_material};

    MeshMaterialResourcesVK resources = MeshMaterialResourceBuilderVK().build(
        make_material_contract(), std::span<const RefRes<RUMaterial>>(ordered_materials.data(), ordered_materials.size()), default_material, 8);

    ASSERT_EQ(resources.material_textures.size(), 5U);
    EXPECT_EQ(resources.material_textures[0], default_albedo);
    EXPECT_EQ(resources.material_textures[1], default_normal);
    EXPECT_EQ(resources.material_textures[2], default_rma);
    EXPECT_EQ(resources.material_textures[3], bound_albedo);
    EXPECT_EQ(resources.material_textures[4], bound_rma);
    EXPECT_EQ(resources.material_sampler, MEM_NULL);

    const std::vector<uint8_t>& packed_data = resources.material_params_data;
    ASSERT_EQ(packed_data.size(), 80U);
    EXPECT_EQ(read_int32(packed_data, 16), 3);
    EXPECT_EQ(read_int32(packed_data, 20), 1);
    EXPECT_EQ(read_int32(packed_data, 24), 4);
    EXPECT_EQ(read_int32(packed_data, 32), 3);
    EXPECT_EQ(read_int32(packed_data, 36), 1);
    EXPECT_EQ(read_int32(packed_data, 40), 2);
    EXPECT_EQ(read_int32(packed_data, 48), 0);
    EXPECT_EQ(read_int32(packed_data, 52), 1);
    EXPECT_EQ(read_int32(packed_data, 56), 2);

    MeshMaterialResourcesVK heap_resources = MeshMaterialResourceBuilderVK().build(make_material_contract(),
        std::span<const RefRes<RUMaterial>>(ordered_materials.data(), ordered_materials.size()),
        default_material,
        8,
        MEM_NULL,
        [&](RefRes<RUTexture> p_texture) {
            for (size_t i = 0; i < resources.material_textures.size(); ++i) {
                if (resources.material_textures[i] == p_texture) {
                    return static_cast<uint32_t>(100 + i);
                }
            }
            throw std::runtime_error("Unexpected texture in descriptor-index resolver test.");
        });
    EXPECT_EQ(heap_resources.material_textures.size(), 5U);
    EXPECT_EQ(read_int32(heap_resources.material_params_data, 16), 103);
    EXPECT_EQ(read_int32(heap_resources.material_params_data, 20), 101);
    EXPECT_EQ(read_int32(heap_resources.material_params_data, 24), 104);
    EXPECT_EQ(read_int32(heap_resources.material_params_data, 48), 100);
    EXPECT_EQ(read_int32(heap_resources.material_params_data, 52), 101);
    EXPECT_EQ(read_int32(heap_resources.material_params_data, 56), 102);
}

TEST(WBEMeshMaterialResourceBuilderVKTest, MatchesFallbackFieldRoleNames) {
    StackAlloc alloc(WBE_KI_B(8));
    RefRes<RUTexture> default_albedo = make_test_texture_ref(alloc);
    RefRes<RUTexture> default_detail = make_test_texture_ref(alloc);
    RefRes<RUTexture> default_rma = make_test_texture_ref(alloc);
    RefRes<RUTexture> bound_detail = make_test_texture_ref(alloc);

    std::array<RUMaterial::TextureSlot, 3> default_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = default_albedo},
        {.role = make_test_shader_role(alloc, "detail_index"), .texture = default_detail},
        {.role = make_test_shader_role(alloc, "rma"), .texture = default_rma},
    }};
    std::array<RUMaterial::TextureSlot, 1> material_slots{{
        {.role = make_test_shader_role(alloc, "detail_index"), .texture = bound_detail},
    }};

    RefRes<RUMaterial> default_material = make_test_material(alloc, "default_material", default_slots);
    RefRes<RUMaterial> material = make_test_material(alloc, "detail_material", material_slots);
    const std::array<RefRes<RUMaterial>, 1> ordered_materials{material};

    MeshMaterialResourcesVK resources = MeshMaterialResourceBuilderVK().build(make_material_contract("detail_index"),
        std::span<const RefRes<RUMaterial>>(ordered_materials.data(), ordered_materials.size()),
        default_material,
        8);

    EXPECT_EQ(read_int32(resources.material_params_data, 16), 0);
    EXPECT_EQ(read_int32(resources.material_params_data, 20), 3);
    EXPECT_EQ(read_int32(resources.material_params_data, 24), 2);
}

TEST(WBEMeshMaterialResourceBuilderVKTest, RejectsCapacityAndDescLimitOverflow) {
    StackAlloc alloc(WBE_KI_B(16));
    RefRes<RUTexture> default_albedo = make_test_texture_ref(alloc);
    RefRes<RUTexture> default_normal = make_test_texture_ref(alloc);
    RefRes<RUTexture> default_rma = make_test_texture_ref(alloc);
    RefRes<RUTexture> bound_albedo = make_test_texture_ref(alloc);
    RefRes<RUTexture> bound_normal = make_test_texture_ref(alloc);
    RefRes<RUTexture> bound_rma = make_test_texture_ref(alloc);

    std::array<RUMaterial::TextureSlot, 3> default_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = default_albedo},
        {.role = make_test_shader_role(alloc, "normal"), .texture = default_normal},
        {.role = make_test_shader_role(alloc, "rma"), .texture = default_rma},
    }};
    std::array<RUMaterial::TextureSlot, 3> material_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = bound_albedo},
        {.role = make_test_shader_role(alloc, "normal"), .texture = bound_normal},
        {.role = make_test_shader_role(alloc, "rma"), .texture = bound_rma},
    }};

    RefRes<RUMaterial> default_material = make_test_material(alloc, "default_material", default_slots);
    RefRes<RUMaterial> material = make_test_material(alloc, "overflow_material", material_slots);
    const std::array<RefRes<RUMaterial>, 5> too_many_materials{material, material, material, material, material};
    EXPECT_THROW(MeshMaterialResourceBuilderVK().build(make_material_contract(),
                     std::span<const RefRes<RUMaterial>>(too_many_materials.data(), too_many_materials.size()),
                     default_material,
                     8),
        std::runtime_error);

    const std::array<RefRes<RUMaterial>, 1> one_material{material};
    EXPECT_THROW(MeshMaterialResourceBuilderVK().build(
                     make_material_contract(), std::span<const RefRes<RUMaterial>>(one_material.data(), one_material.size()), default_material, 4),
        std::runtime_error);
}

TEST(WBEMeshMaterialResourceBuilderVKTest, RejectsMissingDefaultRolesAndDuplicateMaterialRoles) {
    StackAlloc alloc(WBE_KI_B(16));
    RefRes<RUTexture> default_albedo = make_test_texture_ref(alloc);
    RefRes<RUTexture> default_normal = make_test_texture_ref(alloc);
    RefRes<RUTexture> duplicate_normal = make_test_texture_ref(alloc);

    std::array<RUMaterial::TextureSlot, 2> incomplete_default_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = default_albedo},
        {.role = make_test_shader_role(alloc, "normal"), .texture = default_normal},
    }};
    std::array<RUMaterial::TextureSlot, 3> duplicate_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = default_albedo},
        {.role = make_test_shader_role(alloc, "normal"), .texture = default_normal},
        {.role = make_test_shader_role(alloc, "normal"), .texture = duplicate_normal},
    }};

    RefRes<RUMaterial> incomplete_default = make_test_material(alloc, "default_material", incomplete_default_slots);
    RefRes<RUMaterial> duplicate_material = make_test_material(alloc, "duplicate_material", duplicate_slots);
    const std::array<RefRes<RUMaterial>, 1> ordered_materials{duplicate_material};

    EXPECT_THROW(MeshMaterialResourceBuilderVK().build(make_material_contract(),
                     std::span<const RefRes<RUMaterial>>(ordered_materials.data(), ordered_materials.size()),
                     incomplete_default,
                     8),
        std::runtime_error);

    std::array<RUMaterial::TextureSlot, 3> complete_default_slots{{
        {.role = make_test_shader_role(alloc, "albedo"), .texture = default_albedo},
        {.role = make_test_shader_role(alloc, "normal"), .texture = default_normal},
        {.role = make_test_shader_role(alloc, "rma"), .texture = make_test_texture_ref(alloc)},
    }};
    RefRes<RUMaterial> complete_default = make_test_material(alloc, "default_material_complete", complete_default_slots);
    EXPECT_THROW(MeshMaterialResourceBuilderVK().build(make_material_contract(),
                     std::span<const RefRes<RUMaterial>>(ordered_materials.data(), ordered_materials.size()),
                     complete_default,
                     8),
        std::runtime_error);
}

} // namespace WhiteBirdEngine
