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
#include "core/alloc/stl_alloc.hh"
#include "core/memory/reference_strong.hh"
#include "function/render/rend_defs.hh"
#include "function/rend_vk/rend_context_vk.hh"
#include "function/rend_vk/ro_compute_pipeline_vk.hh"
#include "function/rend_vk/rt_max_downsample_vk.hh"
#include "utils/defs.hh"
#include <array>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace WhiteBirdEngine {

TEST(WBERTMaxDownsampleVKTest, RejectsMissingRendData) {
    const std::array<std::string, 2> mip_view_ids{"mip_0", "mip_1"};

    EXPECT_THROW(RTMaxDownsampleVK(nullptr, MEM_NULL, mip_view_ids), std::runtime_error);
}

TEST(WBERTMaxDownsampleVKTest, RejectsFewerThanTwoMipViews) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    const std::array<std::string, 1> mip_view_ids{"mip_0"};

    EXPECT_THROW(RTMaxDownsampleVK(&rend_data, MEM_NULL, mip_view_ids), std::runtime_error);
}

TEST(WBERTMaxDownsampleVKTest, RejectsEmptyMipViewId) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    const std::array<std::string, 3> mip_view_ids{"mip_0", "", "mip_2"};

    EXPECT_THROW(RTMaxDownsampleVK(&rend_data, MEM_NULL, mip_view_ids), std::runtime_error);
}

TEST(WBERTMaxDownsampleVKTest, RejectsRepeatedMipViewId) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    const std::array<std::string, 3> mip_view_ids{"mip_0", "mip_1", "mip_0"};

    EXPECT_THROW(RTMaxDownsampleVK(&rend_data, MEM_NULL, mip_view_ids), std::runtime_error);
}

TEST(WBERTMaxDownsampleVKTest, RejectsNullPipeline) {
    RendAlloc alloc(WBE_KI_B(64));
    RendContextVK rend_data(&alloc);
    const std::array<std::string, 2> mip_view_ids{"mip_0", "mip_1"};

    EXPECT_THROW(RTMaxDownsampleVK(&rend_data, MEM_NULL, mip_view_ids), std::runtime_error);
}

} // namespace WhiteBirdEngine
