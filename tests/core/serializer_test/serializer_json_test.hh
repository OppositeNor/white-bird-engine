#ifndef __WBE_SERIALIZER_JSON_TEST_H__
#define __WBE_SERIALIZER_JSON_TEST_H__

#include "core/serializer/serializer_json.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(SerializerJSONTest, General) {
    WBE::SerializerJSON serializer;
    serializer.register_serialize("val1", 3);
    serializer.register_serialize("val2", 3.1415);
    serializer.register_serialize("val3", "test string");
    serializer.register_serialize("val3", "test string alt");
    ASSERT_EQ(serializer.get_current_context(), "base");
    ASSERT_EQ(serializer.get_context_depth(), 0);
    serializer.push_context("test_context1");
    serializer.register_serialize("context_val1", -10);
    serializer.register_serialize("context_val2", -42);
    serializer.register_serialize("context_val3", "test context string");
    ASSERT_EQ(serializer.get_current_context(), "test_context1");
    ASSERT_EQ(serializer.get_context_depth(), 1);
    serializer.push_context("test_context2");
    serializer.register_serialize("context2_val1", 310);
    serializer.register_serialize("context2_val2", 4);
    serializer.register_serialize("context2_val3", "");
    ASSERT_THROW(serializer.dump(), std::runtime_error);
    ASSERT_EQ(serializer.get_current_context(), "test_context2");
    ASSERT_EQ(serializer.get_context_depth(), 2);
    serializer.pop_context();
    serializer.pop_context();
    std::string result = serializer.dump();
    std::string expected = "{\"test_context1\":"
        "{\"context_val1\":-10,\"context_val2\":-42,\"context_val3\":\"test context string\",\"test_context2\":"
        "{\"context2_val1\":310,\"context2_val2\":4,\"context2_val3\":\"\"}},"
        "\"val1\":3,\"val2\":3.1415,\"val3\":\"test string alt\"}";
    ASSERT_EQ(result, expected);
}

TEST(SerializerJSONTest, Edge) {
    WBE::SerializerJSON serializer;
    ASSERT_EQ(serializer.get_current_context(), "base");
    ASSERT_EQ(serializer.get_context_depth(), 0);
    ASSERT_EQ(serializer.dump(), "null");
}

#endif
