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
#ifndef __WBE_PROFILER_TEST_HH__
#define __WBE_PROFILER_TEST_HH__

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>

#include "core/profiling/profiler.hh"
#include "platform/file_system/directory.hh"
#include "global/global.hh"

namespace WBE = WhiteBirdEngine;
namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_TEST_PROFILING_CHANNEL, WBE_CHANNEL)
}

TEST(WBEProfilerTest, Profiling) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    uint32_t line_num = 0;
    {
        WBE_START_PROFILE(WBE::WBE_TEST_PROFILING_CHANNEL, "Test profile"); line_num = __LINE__;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    const auto& profile_data = global->engine_core->profiling_manager->get_profile_data(WBE::WBE_TEST_PROFILING_CHANNEL);
    ASSERT_EQ(profile_data.size(), 1);
    ASSERT_GT(profile_data[0].delta, 0.4999);
    ASSERT_NE(profile_data[0].file.find("profiler_test.hh"), std::string::npos);
    ASSERT_EQ(profile_data[0].line, line_num);
    uint32_t line_num_1 = 0;
    {
        WBE_START_PROFILE(WBE::WBE_TEST_PROFILING_CHANNEL, "Test profile"); line_num_1 = __LINE__;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_EQ(profile_data.size(), 2);
    ASSERT_GT(profile_data[1].delta, 0.099);
    ASSERT_NE(profile_data[1].file.find("profiler_test.hh"), std::string::npos);
    ASSERT_EQ(profile_data[1].line, line_num_1);
    ASSERT_GT(profile_data[0].delta, 0.4999);
    ASSERT_NE(profile_data[0].file.find("profiler_test.hh"), std::string::npos);
    ASSERT_EQ(profile_data[0].line, line_num);
}

#endif
