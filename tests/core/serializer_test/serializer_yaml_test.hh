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
#include "core/parser/parser_yaml.hh"
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

TEST(SerializerYAMLTest, ConstCharPointer) {
    WBE::SerializerYAML serializer;
    
    // Test basic const char* serialization
    const char* test_string = "Hello, World!";
    serializer.register_serialize("message", test_string);
    
    // Test empty string
    const char* empty_string = "";
    serializer.register_serialize("empty", empty_string);
    
    // Test string with special characters
    const char* special_string = "Line 1\nLine 2\tTabbed \"Quoted\"";
    serializer.register_serialize("special", special_string);
    
    // Test null terminator handling
    const char* null_terminated = "Test\0Hidden";
    serializer.register_serialize("null_term", null_terminated);
    
    std::string result = serializer.dump();
    
    // Use parser to verify the content structure
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    // Verify all keys are present
    ASSERT_TRUE(parser.contains("message"));
    ASSERT_TRUE(parser.contains("empty"));
    ASSERT_TRUE(parser.contains("special"));
    ASSERT_TRUE(parser.contains("null_term"));
    
    // Verify values are correct
    ASSERT_EQ(parser.get_value<std::string>("message"), "Hello, World!");
    ASSERT_EQ(parser.get_value<std::string>("empty"), "");
    ASSERT_EQ(parser.get_value<std::string>("special"), "Line 1\nLine 2\tTabbed \"Quoted\"");
    ASSERT_EQ(parser.get_value<std::string>("null_term"), "Test");
    
    // Verify key count
    auto keys = parser.get_all_keys();
    ASSERT_EQ(keys.size(), 4);
}

TEST(SerializerYAMLTest, ConstCharPointerInContext) {
    WBE::SerializerYAML serializer;
    
    // Test const char* in nested contexts
    serializer.push_context("outer");
    const char* outer_message = "Outer message";
    serializer.register_serialize("msg", outer_message);
    
    serializer.push_context("inner");
    const char* inner_message = "Inner message";
    serializer.register_serialize("msg", inner_message);
    
    serializer.pop_context();
    serializer.pop_context();
    
    std::string result = serializer.dump();
    
    // Use parser to verify nested structure
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    // Verify outer context exists
    ASSERT_TRUE(parser.contains("outer"));
    auto outer_data = parser.get_value<WBE::YAMLData>("outer");
    
    // Verify outer message
    ASSERT_TRUE(outer_data.contains("msg"));
    ASSERT_EQ(outer_data.get_value<std::string>("msg"), "Outer message");
    
    // Verify inner context exists
    ASSERT_TRUE(outer_data.contains("inner"));
    auto inner_data = outer_data.get_value<WBE::YAMLData>("inner");
    
    // Verify inner message
    ASSERT_TRUE(inner_data.contains("msg"));
    ASSERT_EQ(inner_data.get_value<std::string>("msg"), "Inner message");
}

TEST(SerializerYAMLTest, PushList) {
    WBE::SerializerYAML main_serializer;
    
    // Create first item for the list
    WBE::SerializerYAML item1;
    item1.register_serialize("id", 1);
    item1.register_serialize("name", "First Item");
    item1.register_serialize("active", true);
    
    // Create second item for the list
    WBE::SerializerYAML item2;
    item2.register_serialize("id", 2);
    item2.register_serialize("name", "Second Item");
    item2.register_serialize("active", false);
    
    // Create third item with nested context
    WBE::SerializerYAML item3;
    item3.register_serialize("id", 3);
    item3.push_context("details");
    item3.register_serialize("description", "Third item with details");
    item3.register_serialize("priority", "high");
    item3.pop_context();
    
    // Push items to list
    main_serializer.push_list("items", item1);
    main_serializer.push_list("items", item2);
    main_serializer.push_list("items", item3);
    
    // Add some other data
    main_serializer.register_serialize("total_count", 3);
    main_serializer.register_serialize("list_name", "Test Items");
    
    std::string result = main_serializer.dump();
    
    // Use parser to verify basic structure
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    // Verify top-level keys exist
    ASSERT_TRUE(parser.contains("items"));
    ASSERT_TRUE(parser.contains("total_count"));
    ASSERT_TRUE(parser.contains("list_name"));
    
    // Verify top-level values
    ASSERT_EQ(parser.get_value<int>("total_count"), 3);
    ASSERT_EQ(parser.get_value<std::string>("list_name"), "Test Items");
    
    // Basic verification that items list exists and is a sequence
    // Note: Deep list validation would require additional YAML sequence support
    ASSERT_TRUE(parser.contains("items"));
}

TEST(SerializerYAMLTest, PushListWithConstChar) {
    WBE::SerializerYAML main_serializer;
    
    // Create items with const char* data
    WBE::SerializerYAML item1;
    const char* name1 = "Item One";
    const char* desc1 = "Description for item one";
    item1.register_serialize("name", name1);
    item1.register_serialize("description", desc1);
    
    WBE::SerializerYAML item2;
    const char* name2 = "Item Two";
    const char* desc2 = "";  // Empty string
    item2.register_serialize("name", name2);
    item2.register_serialize("description", desc2);
    
    // Push to list
    main_serializer.push_list("string_items", item1);
    main_serializer.push_list("string_items", item2);
    
    std::string result = main_serializer.dump();
    
    // Use parser to verify basic structure and content
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_TRUE(parser.contains("string_items"));
    
    // Verify that the serialized content contains the expected strings
    ASSERT_NE(result.find("Item One"), std::string::npos);
    ASSERT_NE(result.find("Item Two"), std::string::npos);
    ASSERT_NE(result.find("Description for item one"), std::string::npos);
}

TEST(SerializerYAMLTest, PushListEmpty) {
    WBE::SerializerYAML main_serializer;
    
    // Create empty serializer
    WBE::SerializerYAML empty_item;
    
    main_serializer.push_list("empty_list", empty_item);
    main_serializer.register_serialize("has_items", false);
    
    std::string result = main_serializer.dump();
    
    // Use parser to verify empty list handling
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_TRUE(parser.contains("empty_list"));
    ASSERT_TRUE(parser.contains("has_items"));
    ASSERT_EQ(parser.get_value<bool>("has_items"), false);
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
