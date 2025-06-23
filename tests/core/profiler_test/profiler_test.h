#ifndef __WBE_PROFILER_TEST_H__
#define __WBE_PROFILER_TEST_H__

#include <gtest/gtest.h>
#include <memory>
#include <unistd.h>

#include "core/profiling/profiler.h"
#include "platform/file_system/directory.h"
#include "global/global.h"

namespace WBE = WhiteBirdEngine;
namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_TEST_PROFILING_CHANNEL)
}

TEST(WBEProfilerTest, Profiling) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"build"}));
    uint32_t line_num = 0;
    {
        WBE_START_PROFILE(WBE::WBE_TEST_PROFILING_CHANNEL); line_num = __LINE__;
        sleep(3);
    }
    const auto& profile_data = global->profiling_manager->get_profile_data(WBE::WBE_TEST_PROFILING_CHANNEL);
    ASSERT_EQ(profile_data.size(), 1);
    ASSERT_GT(profile_data[0].delta, 2999);
    ASSERT_NE(profile_data[0].file.find("profiler_test.h"), std::string::npos);
    ASSERT_EQ(profile_data[0].line, line_num);
    {
        WBE_START_PROFILE(WBE::WBE_TEST_PROFILING_CHANNEL); line_num = __LINE__;
        sleep(2);
    }
    ASSERT_EQ(profile_data.size(), 2);
    ASSERT_GT(profile_data[1].delta, 1999);
    ASSERT_NE(profile_data[1].file.find("profiler_test.h"), std::string::npos);
    ASSERT_EQ(profile_data[1].line, line_num);
}

#endif
