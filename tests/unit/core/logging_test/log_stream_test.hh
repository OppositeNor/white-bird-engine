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
#ifndef __WBE_LOG_STREAM_TEST_HH__
#define __WBE_LOG_STREAM_TEST_HH__

#include "global/global.hh"
#include "core/logging/log_stream.hh"
#include "platform/file_system/directory.hh"

#include <gtest/gtest.h>
#include <sstream>

namespace WBE = WhiteBirdEngine;

TEST(WBELogStreamTest, General) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::stringstream ss;
    WBE::LogStream log_stream(ss);
    log_stream.message("Test message");
    ASSERT_EQ(ss.str(), std::string("[WBE_CHANNEL_UNKNOWN] <Message>: Test message\n"));
    log_stream.warning("Test warning");
    ASSERT_EQ(ss.str(), std::string("[WBE_CHANNEL_UNKNOWN] <Message>: Test message\n"
                                    "[WBE_CHANNEL_UNKNOWN] <Warning>: Test warning\n"));
    log_stream.error("Test error");
    ASSERT_EQ(ss.str(), std::string("[WBE_CHANNEL_UNKNOWN] <Message>: Test message\n"
                                    "[WBE_CHANNEL_UNKNOWN] <Warning>: Test warning\n"
                                    "[WBE_CHANNEL_UNKNOWN] <Error>: Test error\n"));
}

namespace WhiteBirdEngine {
WBE_LABEL(WBE_TEST_CHANNEL, WBE_CHANNEL)
}

TEST(WBELogStreamTest, UserDefinedLabel) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::stringstream ss;
    WBE::LogStream log_stream(WBE::WBE_TEST_CHANNEL, ss);
    log_stream.message("Test message");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_CHANNEL] <Message>: Test message\n"));
    log_stream.warning("Test warning");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_CHANNEL] <Message>: Test message\n"
                                    "[WBE_TEST_CHANNEL] <Warning>: Test warning\n"));
    log_stream.error("Test error");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_CHANNEL] <Message>: Test message\n"
                                    "[WBE_TEST_CHANNEL] <Warning>: Test warning\n"
                                    "[WBE_TEST_CHANNEL] <Error>: Test error\n"));
}

#define TEST_LABEL_NAME_MACRO WBE_TEST_LABEL_MACRO

namespace WhiteBirdEngine {
WBE_LABEL(TEST_LABEL_NAME_MACRO, WBE_CHANNEL)
}

TEST(WBELogStreamTest, UserDefinedLabelMacroExpand) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::stringstream ss;
    WBE::LogStream log_stream(WBE::WBE_TEST_LABEL_MACRO, ss);
    log_stream.message("Test message");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL_MACRO] <Message>: Test message\n"));
    log_stream.warning("Test warning");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL_MACRO] <Message>: Test message\n"
                                    "[WBE_TEST_LABEL_MACRO] <Warning>: Test warning\n"));
    log_stream.error("Test error");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL_MACRO] <Message>: Test message\n"
                                    "[WBE_TEST_LABEL_MACRO] <Warning>: Test warning\n"
                                    "[WBE_TEST_LABEL_MACRO] <Error>: Test error\n"));
}



#endif
