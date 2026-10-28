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
#ifndef WBE_FILE_ENGINE_CONFIG_TEST_HH
#define WBE_FILE_ENGINE_CONFIG_TEST_HH

#include "core/engine_config/engine_config.hh"
#include "core/engine_config/engine_config_options.hh"
#include "platform/file_system/path.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace WBE = WhiteBirdEngine;

namespace {

// The test configuration that the build copies from res/config/test/.
const WBE::Path ENGINE_CONFIG_TEST_FILE = WBE::Path("test_env") / "res" / "config" / "engine_config.toml";

std::vector<char*> engine_config_test_arguments(std::vector<std::string>& p_arguments) {
    std::vector<char*> result;
    result.reserve(p_arguments.size());
    for (std::string& argument : p_arguments) {
        result.push_back(argument.data());
    }
    return result;
}

} // namespace

TEST(WBEEngineConfigTest, LoadsConfigFileWithoutArguments) {
    WBE::EngineConfig config(ENGINE_CONFIG_TEST_FILE, 0, nullptr);
    const WBE::EngineConfigOptions& options = config.get_config_options();

    EXPECT_EQ(options.per_tick_stack_size, 65536U);
    EXPECT_EQ(options.global_mem_small_pool_size, 1048576U);
    EXPECT_EQ(options.global_mem_medium_pool_size, 15728640U);
    EXPECT_EQ(options.global_mem_large_pool_size, 536870912U);
    EXPECT_EQ(options.thread_mem_pool_size, 16384U);
    EXPECT_EQ(options.resource_mem_pool_size, 6442450944U);
    // Fields the file omits keep their defaults.
    EXPECT_EQ(options.worker_thread_count, 0U);
    EXPECT_TRUE(options.utility_name.empty());
}

TEST(WBEEngineConfigTest, ArgumentsOverrideConfigFile) {
    std::vector<std::string> arguments{"engine_config_test", "--resource_mem_pool_size=4096", "--worker_thread_count=3"};
    std::vector<char*> argument_pointers = engine_config_test_arguments(arguments);
    WBE::EngineConfig config(
        ENGINE_CONFIG_TEST_FILE, static_cast<uint32_t>(argument_pointers.size()), argument_pointers.data());
    const WBE::EngineConfigOptions& options = config.get_config_options();

    EXPECT_EQ(options.resource_mem_pool_size, 4096U);
    EXPECT_EQ(options.worker_thread_count, 3U);
    EXPECT_EQ(options.utility_name, "engine_config_test");
    // Options not on the command line keep the file's values.
    EXPECT_EQ(options.thread_mem_pool_size, 16384U);
}

TEST(WBEEngineConfigTest, IgnoresUnregisteredArguments) {
    std::vector<std::string> arguments{"engine_config_test", "--gtest_filter=*", "--per_tick_stack_size=1024"};
    std::vector<char*> argument_pointers = engine_config_test_arguments(arguments);
    WBE::EngineConfig config(
        ENGINE_CONFIG_TEST_FILE, static_cast<uint32_t>(argument_pointers.size()), argument_pointers.data());

    EXPECT_EQ(config.get_config_options().per_tick_stack_size, 1024U);
}

TEST(WBEEngineConfigTest, RegistersAndClearsSingleton) {
    {
        WBE::EngineConfig config(ENGINE_CONFIG_TEST_FILE, 0, nullptr);
        EXPECT_EQ(WBE::EngineConfig::get_singleton(), &config);
    }
    EXPECT_EQ(WBE::EngineConfig::get_singleton(), nullptr);
}

TEST(WBEEngineConfigTest, ThrowsForMissingConfigFile) {
    const WBE::Path missing_path = std::filesystem::temp_directory_path() / "wbe_engine_config_missing.toml";
    std::filesystem::remove(missing_path);

    EXPECT_THROW(WBE::EngineConfig(missing_path, 0, nullptr), std::runtime_error);
}

#endif
