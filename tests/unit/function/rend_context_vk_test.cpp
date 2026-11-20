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
#include "core/core_utils.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_buffer_vk.hh"
#include "function/rend_vk/ro_compute_pipeline_vk.hh"
#include "function/rend_vk/ro_graphics_pipeline_vk.hh"
#include "function/rend_vk/ro_image_vk.hh"
#include "function/rend_vk/ro_image_view_vk.hh"
#include "function/rend_vk/ro_sampler_vk.hh"
#include "function/rend_vk/ro_shader_vk.hh"
#include "function/rend_vk/ro_staging_buffer_vk.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <type_traits>

namespace WhiteBirdEngine {

static_assert(std::is_same_v<ROBufferVK::Builder, BufferDescVK>);
static_assert(std::is_same_v<ROBufferVK::AliasBuilder, BufferAliasDescVK>);
static_assert(std::is_same_v<ROImageVK::Builder, ImageDescVK>);
static_assert(std::is_same_v<ROImageVK::AliasBuilder, ImageAliasDescVK>);
static_assert(std::is_same_v<ROImageViewVK::Builder, ImageViewDescVK>);
static_assert(std::is_same_v<ROSamplerVK::Builder, SamplerDescVK>);
static_assert(std::is_same_v<ROStagingBufferVK::Builder, StagingBufferDescVK>);
static_assert(std::is_same_v<ROShaderVK::Builder, ShaderDescVK>);
static_assert(std::is_same_v<ROComputePipelineVK::Builder, ComputePipelineDescVK>);
static_assert(std::is_same_v<ROGraphicsPipelineVK::Builder, GraphicsPipelineDescVK>);

TEST(WBERendContextVKTest, RequiresAlloc) {
    EXPECT_THROW(RendContextVK(nullptr), std::runtime_error);
}

TEST(WBERendContextVKTest, DoesNotOwnAlloc) {
    RendAlloc alloc(WBE_KI_B(64));
    {
        const RendContextVK context(&alloc);

        EXPECT_EQ(context.alloc, &alloc);
    }

    EXPECT_TRUE(alloc.is_empty());
    const MemID memory = alloc.allocate(64);
    alloc.deallocate(memory);
    EXPECT_TRUE(alloc.is_empty());
}

TEST(WBERendContextVKTest, CannotBeDefaultConstructedCopiedOrMoved) {
    EXPECT_FALSE(std::is_default_constructible_v<RendContextVK>);
    EXPECT_FALSE(std::is_copy_constructible_v<RendContextVK>);
    EXPECT_FALSE(std::is_copy_assignable_v<RendContextVK>);
    EXPECT_FALSE(std::is_move_constructible_v<RendContextVK>);
    EXPECT_FALSE(std::is_move_assignable_v<RendContextVK>);
}

TEST(WBERendContextVKTest, CreationRejectsMissingRequiredPropertiesWithoutDevice) {
    RendAlloc alloc(WBE_KI_B(64));
    {
        const RendContextVK context(&alloc);

        EXPECT_THROW(context.create_buffer(BufferDescVK{}), std::runtime_error);
        EXPECT_THROW(context.create_image(ImageDescVK{}), std::runtime_error);
        EXPECT_THROW(context.create_image_view(ImageViewDescVK{}), std::runtime_error);
        EXPECT_THROW(context.create_staging_buffer(StagingBufferDescVK{}), std::runtime_error);
        EXPECT_THROW(context.create_shader(ShaderDescVK{}), std::runtime_error);
        EXPECT_THROW(context.create_compute_pipeline(ComputePipelineDescVK{}), std::runtime_error);
        EXPECT_THROW(context.create_graphics_pipeline(GraphicsPipelineDescVK{}), std::runtime_error);
    }
    // Failed placement construction leaves its storage in the pool; all constructed objects have now been destroyed.
    alloc.clear();
}

TEST(WBERendContextVKTest, AliasingCreationRequiresDeviceMemoryWithoutDevice) {
    RendAlloc alloc(WBE_KI_B(64));
    {
        const RendContextVK context(&alloc);

        EXPECT_THROW(context.create_buffer(BufferAliasDescVK{}), std::runtime_error);
        EXPECT_THROW(context.create_image(ImageAliasDescVK{}), std::runtime_error);
    }
    // Failed placement construction leaves its storage in the pool; all constructed objects have now been destroyed.
    alloc.clear();
}

} // namespace WhiteBirdEngine
