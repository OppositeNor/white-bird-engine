// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#ifndef __WBE_SERIALIZER_YAML_TEST_HH__
#define __WBE_SERIALIZER_YAML_TEST_HH__

#include "core/serializer/serializer_yaml.hh"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(SerializerYAMLTest, General) {
    WBE::SerializerYAML serializer;
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
    std::string expected = 
        "val1: 3\n"
        "val2: 3.1415\n"
        "val3: test string alt\n"
        "test_context1:\n"
        "  context_val1: -10\n"
        "  context_val2: -42\n"
        "  context_val3: test context string\n"
        "  test_context2:\n"
        "    context2_val1: 310\n"
        "    context2_val2: 4\n"
        "    context2_val3: \"\"";
    ASSERT_EQ(result, expected);
}

TEST(SerializerYAMLTest, Edge) {
    WBE::SerializerYAML serializer;
    ASSERT_EQ(serializer.get_current_context(), "base");
    ASSERT_EQ(serializer.get_context_depth(), 0);
    ASSERT_EQ(serializer.dump(), "");
}

#endif
