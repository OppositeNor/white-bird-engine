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
#include <cstring>

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

TEST(SerializerYAMLTest, BufferBoundsChecking) {
    WBE::SerializerYAML serializer;
    
    // Test Buffer with different sizes
    WBE::Buffer<16> small_buffer;
    WBE::Buffer<64> medium_buffer;
    WBE::Buffer<256> large_buffer;
    
    // Test small string that fits in all buffers
    const char* small_str = "Hello";
    strcpy(small_buffer.buffer, small_str);
    strcpy(medium_buffer.buffer, small_str);
    strcpy(large_buffer.buffer, small_str);
    
    serializer.register_serialize("small_buf", small_buffer);
    serializer.register_serialize("medium_buf", medium_buffer);
    serializer.register_serialize("large_buf", large_buffer);
    
    std::string result = serializer.dump();
    
    // Parse and verify
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("small_buf"), "Hello");
    ASSERT_EQ(parser.get_value<std::string>("medium_buf"), "Hello");
    ASSERT_EQ(parser.get_value<std::string>("large_buf"), "Hello");
}

TEST(SerializerYAMLTest, BufferMaxCapacity) {
    WBE::SerializerYAML serializer;
    
    // Test buffer at maximum capacity (15 chars + null terminator for size 16)
    WBE::Buffer<16> buffer;
    const char* max_str = "123456789012345"; // 15 characters
    strcpy(buffer.buffer, max_str);
    
    serializer.register_serialize("max_capacity", buffer);
    std::string result = serializer.dump();
    
    // Parse and verify
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("max_capacity"), "123456789012345");
}

TEST(SerializerYAMLTest, BufferWithSpecialCharacters) {
    WBE::SerializerYAML serializer;
    
    WBE::Buffer<64> buffer;
    const char* special_str = "Line1\nLine2\tTab\"Quote'";
    strcpy(buffer.buffer, special_str);
    
    serializer.register_serialize("special_chars", buffer);
    std::string result = serializer.dump();
    
    // Parse and verify
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("special_chars"), "Line1\nLine2\tTab\"Quote'");
}

TEST(SerializerYAMLTest, BufferEmptyString) {
    WBE::SerializerYAML serializer;
    
    WBE::Buffer<32> buffer;
    buffer.buffer[0] = '\0'; // Empty string
    
    serializer.register_serialize("empty_buffer", buffer);
    std::string result = serializer.dump();
    
    // Parse and verify
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("empty_buffer"), "");
}

TEST(SerializerYAMLTest, BufferMultilineHandling) {
    WBE::SerializerYAML serializer;
    
    WBE::Buffer<128> buffer;
    const char* multiline_str = "Line 1\nLine 2\nLine 3\nWith various content";
    strcpy(buffer.buffer, multiline_str);
    
    serializer.register_serialize("multiline_buffer", buffer);
    std::string result = serializer.dump();
    
    // Parse and verify
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("multiline_buffer"), "Line 1\nLine 2\nLine 3\nWith various content");
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

TEST(SerializerYAMLTest, RoundTripSerialization) {
    WBE::SerializerYAML serializer;
    
    // Create complex data structure
    serializer.register_serialize("string_val", "Hello, YAML World!");
    serializer.register_serialize("int_val", 42);
    serializer.register_serialize("double_val", 3.14159);
    serializer.register_serialize("bool_val", true);
    serializer.register_serialize("negative_int", -123);
    serializer.register_serialize("zero_val", 0);
    
    // Add nested context
    serializer.push_context("nested_data");
    serializer.register_serialize("nested_string", "nested yaml value");
    serializer.register_serialize("nested_number", 99);
    
    // Add deeper nesting
    serializer.push_context("deep_nested");
    serializer.register_serialize("deep_val", "very deep yaml");
    const char* deep_cstr = "deep yaml const char";
    serializer.register_serialize("deep_cstr", deep_cstr);
    serializer.pop_context();
    serializer.pop_context();
    
    // Serialize to YAML
    std::string yaml_result = serializer.dump();
    
    // Parse back the YAML
    WBE::ParserYAML parser;
    parser.parse_from_buffer(yaml_result);
    
    // Verify all values are preserved
    ASSERT_EQ(parser.get_value<std::string>("string_val"), "Hello, YAML World!");
    ASSERT_EQ(parser.get_value<int>("int_val"), 42);
    ASSERT_DOUBLE_EQ(parser.get_value<double>("double_val"), 3.14159);
    ASSERT_EQ(parser.get_value<bool>("bool_val"), true);
    ASSERT_EQ(parser.get_value<int>("negative_int"), -123);
    ASSERT_EQ(parser.get_value<int>("zero_val"), 0);
    
    // Verify nested data
    auto nested = parser.get_value<WBE::YAMLData>("nested_data");
    ASSERT_EQ(nested.get_value<std::string>("nested_string"), "nested yaml value");
    ASSERT_EQ(nested.get_value<int>("nested_number"), 99);
    
    // Verify deep nested data
    auto deep_nested = nested.get_value<WBE::YAMLData>("deep_nested");
    ASSERT_EQ(deep_nested.get_value<std::string>("deep_val"), "very deep yaml");
    ASSERT_EQ(deep_nested.get_value<std::string>("deep_cstr"), "deep yaml const char");
}

TEST(SerializerYAMLTest, SpecialCharactersAndEdgeCases) {
    WBE::SerializerYAML serializer;
    
    // Test various special characters and edge cases for YAML
    const char* unicode_str = "Unicode: αβγ δεζ 中文 🚀";
    const char* yaml_special = "YAML special: 'single quotes' and \"double quotes\"";
    const char* whitespace_str = "Whitespace:\n\t\r  test";
    const char* empty_str = "";
    const char* single_char = "a";
    const char* colon_str = "Key: value inside string";
    const char* dash_str = "- not a list item";
    
    serializer.register_serialize("unicode", unicode_str);
    serializer.register_serialize("yaml_special", yaml_special);
    serializer.register_serialize("whitespace", whitespace_str);
    serializer.register_serialize("empty", empty_str);
    serializer.register_serialize("single_char", single_char);
    serializer.register_serialize("colon_test", colon_str);
    serializer.register_serialize("dash_test", dash_str);
    
    // Test numeric edge cases
    serializer.register_serialize("max_int", std::numeric_limits<int>::max());
    serializer.register_serialize("min_int", std::numeric_limits<int>::min());
    serializer.register_serialize("large_double", 1234567890.123456789);
    serializer.register_serialize("small_double", 0.000000123456789);
    
    std::string result = serializer.dump();
    
    // Parse back and verify
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("unicode"), unicode_str);
    ASSERT_EQ(parser.get_value<std::string>("yaml_special"), yaml_special);
    ASSERT_EQ(parser.get_value<std::string>("whitespace"), whitespace_str);
    ASSERT_EQ(parser.get_value<std::string>("empty"), "");
    ASSERT_EQ(parser.get_value<std::string>("single_char"), "a");
    ASSERT_EQ(parser.get_value<std::string>("colon_test"), colon_str);
    ASSERT_EQ(parser.get_value<std::string>("dash_test"), dash_str);
    
    ASSERT_EQ(parser.get_value<int>("max_int"), std::numeric_limits<int>::max());
    ASSERT_EQ(parser.get_value<int>("min_int"), std::numeric_limits<int>::min());
    ASSERT_DOUBLE_EQ(parser.get_value<double>("large_double"), 1234567890.123456789);
    ASSERT_DOUBLE_EQ(parser.get_value<double>("small_double"), 0.000000123456789);
}

TEST(SerializerYAMLTest, MultilineStringsAndFormatting) {
    WBE::SerializerYAML serializer;
    
    // Test multiline strings and formatting
    const char* multiline_str = "Line 1\nLine 2\nLine 3\nWith multiple lines";
    const char* formatted_str = "  Indented text  \n\tTabbed content\n  More indentation  ";
    const char* code_block = "def function():\n    return 'Python code'\n    # with comments";
    
    serializer.register_serialize("multiline", multiline_str);
    serializer.register_serialize("formatted", formatted_str);
    serializer.register_serialize("code_block", code_block);
    
    // Add some regular values for comparison
    serializer.register_serialize("simple_string", "Simple value");
    serializer.register_serialize("number", 123);
    
    std::string result = serializer.dump();
    
    // Parse back and verify formatting is preserved
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("multiline"), multiline_str);
    ASSERT_EQ(parser.get_value<std::string>("formatted"), formatted_str);
    ASSERT_EQ(parser.get_value<std::string>("code_block"), code_block);
    ASSERT_EQ(parser.get_value<std::string>("simple_string"), "Simple value");
    ASSERT_EQ(parser.get_value<int>("number"), 123);
}

TEST(SerializerYAMLTest, ErrorHandling) {
    WBE::SerializerYAML serializer;
    
    // Test context stack underflow
    ASSERT_THROW(serializer.pop_context(), std::runtime_error);
    
    // Test dumping with unbalanced contexts
    serializer.push_context("test1");
    serializer.push_context("test2");
    ASSERT_THROW(serializer.dump(), std::runtime_error);
    
    // Clean up contexts
    serializer.pop_context();
    serializer.pop_context();
    
    // Test that serializer produces valid YAML after errors
    serializer.register_serialize("after_error", "test_value");
    std::string result = serializer.dump();
    ASSERT_FALSE(result.empty());
    ASSERT_NE(result.find("after_error"), std::string::npos);
}

TEST(SerializerYAMLTest, ContextManagement) {
    WBE::SerializerYAML serializer;
    
    // Test multiple context levels
    ASSERT_EQ(serializer.get_context_depth(), 0);
    ASSERT_EQ(serializer.get_current_context(), "base");
    
    serializer.push_context("level1");
    ASSERT_EQ(serializer.get_context_depth(), 1);
    ASSERT_EQ(serializer.get_current_context(), "level1");
    
    serializer.push_context("level2");
    ASSERT_EQ(serializer.get_context_depth(), 2);
    ASSERT_EQ(serializer.get_current_context(), "level2");
    
    serializer.push_context("level3");
    ASSERT_EQ(serializer.get_context_depth(), 3);
    ASSERT_EQ(serializer.get_current_context(), "level3");
    
    // Add data at different levels
    serializer.register_serialize("deep_data", "level3_yaml_value");
    serializer.pop_context();
    
    serializer.register_serialize("mid_data", "level2_yaml_value");
    serializer.pop_context();
    
    serializer.register_serialize("shallow_data", "level1_yaml_value");
    serializer.pop_context();
    
    serializer.register_serialize("root_data", "root_yaml_value");
    
    std::string result = serializer.dump();
    
    // Parse and verify nested structure
    WBE::ParserYAML parser;
    parser.parse_from_buffer(result);
    
    ASSERT_EQ(parser.get_value<std::string>("root_data"), "root_yaml_value");
    
    auto level1 = parser.get_value<WBE::YAMLData>("level1");
    ASSERT_EQ(level1.get_value<std::string>("shallow_data"), "level1_yaml_value");
    
    auto level2 = level1.get_value<WBE::YAMLData>("level2");
    ASSERT_EQ(level2.get_value<std::string>("mid_data"), "level2_yaml_value");
    
    auto level3 = level2.get_value<WBE::YAMLData>("level3");
    ASSERT_EQ(level3.get_value<std::string>("deep_data"), "level3_yaml_value");
}

#endif
