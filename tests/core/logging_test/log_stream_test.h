#ifndef __WBE_LOG_STREAM_TEST_H__
#define __WBE_LOG_STREAM_TEST_H__

#include "core/logging/log_stream.h"
#include "utils/defs.h"

#include <gtest/gtest.h>
#include <sstream>

namespace WBE = WhiteBirdEngine;

TEST(WBELogStreamTest, General) {
    std::stringstream ss;
    WBE::LogStream log_stream(ss);
    log_stream.message("Test message");
    ASSERT_EQ(ss.str(), std::string("[WBE_CHANNEL_UNKNOWN] Message: Test message\n"));
    log_stream.warning("Test warning");
    ASSERT_EQ(ss.str(), std::string("[WBE_CHANNEL_UNKNOWN] Message: Test message\n"
                                    "[WBE_CHANNEL_UNKNOWN] Warning: Test warning\n"));
    log_stream.error("Test error");
    ASSERT_EQ(ss.str(), std::string("[WBE_CHANNEL_UNKNOWN] Message: Test message\n"
                                    "[WBE_CHANNEL_UNKNOWN] Warning: Test warning\n"
                                    "[WBE_CHANNEL_UNKNOWN] Error: Test error\n"));
}

namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(WBE_TEST_LABEL)
}

TEST(WBELogStreamTest, UserDefinedLabel) {
    std::stringstream ss;
    WBE::LogStream<WBE::WBE_TEST_LABEL> log_stream(ss);
    log_stream.message("Test message");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL] Message: Test message\n"));
    log_stream.warning("Test warning");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL] Message: Test message\n"
                                    "[WBE_TEST_LABEL] Warning: Test warning\n"));
    log_stream.error("Test error");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL] Message: Test message\n"
                                    "[WBE_TEST_LABEL] Warning: Test warning\n"
                                    "[WBE_TEST_LABEL] Error: Test error\n"));
}

#define TEST_LABEL_NAME_MACRO WBE_TEST_LABEL_MACRO

namespace WhiteBirdEngine {
WBE_DEFINE_LABEL(TEST_LABEL_NAME_MACRO)
}

TEST(WBELogStreamTest, UserDefinedLabelMacroExpand) {
    std::stringstream ss;
    WBE::LogStream<WBE::WBE_TEST_LABEL_MACRO> log_stream(ss);
    log_stream.message("Test message");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL_MACRO] Message: Test message\n"));
    log_stream.warning("Test warning");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL_MACRO] Message: Test message\n"
                                    "[WBE_TEST_LABEL_MACRO] Warning: Test warning\n"));
    log_stream.error("Test error");
    ASSERT_EQ(ss.str(), std::string("[WBE_TEST_LABEL_MACRO] Message: Test message\n"
                                    "[WBE_TEST_LABEL_MACRO] Warning: Test warning\n"
                                    "[WBE_TEST_LABEL_MACRO] Error: Test error\n"));

}


#endif
