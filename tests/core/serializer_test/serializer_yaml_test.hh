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

    serializer.push_context("test_context1");
    ASSERT_EQ(serializer.get_current_context(), "test_context1");
    ASSERT_EQ(serializer.get_context_depth(), 1);
    serializer.pop_context();
    ASSERT_EQ(serializer.get_current_context(), "base");
    ASSERT_EQ(serializer.get_context_depth(), 0);
    ASSERT_EQ(serializer.dump(), "test_context1: ~");
}

#endif
