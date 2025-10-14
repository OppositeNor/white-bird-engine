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
#ifndef __WBE_SERIALIZER_JSON_TEST_HH__
#define __WBE_SERIALIZER_JSON_TEST_HH__

#include "core/serializer/serializer_json.hh"
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

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

TEST(SerializerJSONTest, ConstCharPointer) {
    WBE::SerializerJSON serializer;
    
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
    
    // Use nlohmann json to verify the content structure
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    // Verify all keys are present
    ASSERT_TRUE(json_obj.contains("message"));
    ASSERT_TRUE(json_obj.contains("empty"));
    ASSERT_TRUE(json_obj.contains("special"));
    ASSERT_TRUE(json_obj.contains("null_term"));
    
    // Verify values are correct
    ASSERT_EQ(json_obj["message"].get<std::string>(), "Hello, World!");
    ASSERT_EQ(json_obj["empty"].get<std::string>(), "");
    ASSERT_EQ(json_obj["special"].get<std::string>(), "Line 1\nLine 2\tTabbed \"Quoted\"");
    ASSERT_EQ(json_obj["null_term"].get<std::string>(), "Test");
    
    // Verify key count
    ASSERT_EQ(json_obj.size(), 4);
}

TEST(SerializerJSONTest, ConstCharPointerInContext) {
    WBE::SerializerJSON serializer;
    
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
    
    // Use nlohmann json to verify nested structure
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    // Verify outer context exists
    ASSERT_TRUE(json_obj.contains("outer"));
    auto outer_data = json_obj["outer"];
    
    // Verify outer message
    ASSERT_TRUE(outer_data.contains("msg"));
    ASSERT_EQ(outer_data["msg"].get<std::string>(), "Outer message");
    
    // Verify inner context exists
    ASSERT_TRUE(outer_data.contains("inner"));
    auto inner_data = outer_data["inner"];
    
    // Verify inner message
    ASSERT_TRUE(inner_data.contains("msg"));
    ASSERT_EQ(inner_data["msg"].get<std::string>(), "Inner message");
}

TEST(SerializerJSONTest, BufferBoundsChecking) {
    WBE::SerializerJSON serializer;
    
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
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_EQ(json_obj["small_buf"].get<std::string>(), "Hello");
    ASSERT_EQ(json_obj["medium_buf"].get<std::string>(), "Hello");
    ASSERT_EQ(json_obj["large_buf"].get<std::string>(), "Hello");
}

TEST(SerializerJSONTest, BufferMaxCapacity) {
    WBE::SerializerJSON serializer;
    
    // Test buffer at maximum capacity (15 chars + null terminator for size 16)
    WBE::Buffer<16> buffer;
    const char* max_str = "123456789012345"; // 15 characters
    strcpy(buffer.buffer, max_str);
    
    serializer.register_serialize("max_capacity", buffer);
    std::string result = serializer.dump();
    
    // Parse and verify
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_EQ(json_obj["max_capacity"].get<std::string>(), "123456789012345");
}

TEST(SerializerJSONTest, BufferWithSpecialCharacters) {
    WBE::SerializerJSON serializer;
    
    WBE::Buffer<64> buffer;
    const char* special_str = "Line1\nLine2\tTab\"Quote'";
    strcpy(buffer.buffer, special_str);
    
    serializer.register_serialize("special_chars", buffer);
    std::string result = serializer.dump();
    
    // Parse and verify
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_EQ(json_obj["special_chars"].get<std::string>(), "Line1\nLine2\tTab\"Quote'");
}

TEST(SerializerJSONTest, BufferEmptyString) {
    WBE::SerializerJSON serializer;
    
    WBE::Buffer<32> buffer;
    buffer.buffer[0] = '\0'; // Empty string
    
    serializer.register_serialize("empty_buffer", buffer);
    std::string result = serializer.dump();
    
    // Parse and verify
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_EQ(json_obj["empty_buffer"].get<std::string>(), "");
}

TEST(SerializerJSONTest, PushList) {
    WBE::SerializerJSON main_serializer;
    
    // Create first item for the list
    WBE::SerializerJSON item1;
    item1.register_serialize("id", 1);
    item1.register_serialize("name", "First Item");
    item1.register_serialize("active", true);
    
    // Create second item for the list
    WBE::SerializerJSON item2;
    item2.register_serialize("id", 2);
    item2.register_serialize("name", "Second Item");
    item2.register_serialize("active", false);
    
    // Create third item with nested context
    WBE::SerializerJSON item3;
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
    
    // Use nlohmann json to deeply verify the structure
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    // Verify top-level keys
    ASSERT_TRUE(json_obj.contains("items"));
    ASSERT_TRUE(json_obj.contains("total_count"));
    ASSERT_TRUE(json_obj.contains("list_name"));
    
    // Verify top-level values
    ASSERT_EQ(json_obj["total_count"].get<int>(), 3);
    ASSERT_EQ(json_obj["list_name"].get<std::string>(), "Test Items");
    
    // Verify items array structure
    auto items = json_obj["items"];
    ASSERT_TRUE(items.is_array());
    ASSERT_EQ(items.size(), 3);

    // Verify first item
    ASSERT_TRUE(items[0].contains("id"));
    ASSERT_TRUE(items[0].contains("name"));
    ASSERT_TRUE(items[0].contains("active"));
    ASSERT_EQ(items[0]["id"].get<int>(), 1);
    ASSERT_EQ(items[0]["name"].get<std::string>(), "First Item");
    ASSERT_EQ(items[0]["active"].get<bool>(), true);
    
    // Verify second item
    ASSERT_EQ(items[1]["id"].get<int>(), 2);
    ASSERT_EQ(items[1]["name"].get<std::string>(), "Second Item");
    ASSERT_EQ(items[1]["active"].get<bool>(), false);
    
    // Verify third item with nested context
    ASSERT_EQ(items[2]["id"].get<int>(), 3);
    ASSERT_TRUE(items[2].contains("details"));
    auto details = items[2]["details"];
    ASSERT_EQ(details["description"].get<std::string>(), "Third item with details");
    ASSERT_EQ(details["priority"].get<std::string>(), "high");
}

TEST(SerializerJSONTest, PushListWithConstChar) {
    WBE::SerializerJSON main_serializer;
    
    // Create items with const char* data
    WBE::SerializerJSON item1;
    const char* name1 = "Item One";
    const char* desc1 = "Description for item one";
    item1.register_serialize("name", name1);
    item1.register_serialize("description", desc1);
    
    WBE::SerializerJSON item2;
    const char* name2 = "Item Two";
    const char* desc2 = "";  // Empty string
    item2.register_serialize("name", name2);
    item2.register_serialize("description", desc2);
    
    // Push to list
    main_serializer.push_list("string_items", item1);
    main_serializer.push_list("string_items", item2);
    
    std::string result = main_serializer.dump();
    
    // Use nlohmann json to verify const char* handling in lists
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_TRUE(json_obj.contains("string_items"));
    auto items = json_obj["string_items"];
    ASSERT_TRUE(items.is_array());
    ASSERT_EQ(items.size(), 2);
    
    // Verify first item
    ASSERT_TRUE(items[0].contains("name"));
    ASSERT_TRUE(items[0].contains("description"));
    ASSERT_EQ(items[0]["name"].get<std::string>(), "Item One");
    ASSERT_EQ(items[0]["description"].get<std::string>(), "Description for item one");
    
    // Verify second item with empty string
    ASSERT_EQ(items[1]["name"].get<std::string>(), "Item Two");
    ASSERT_EQ(items[1]["description"].get<std::string>(), "");
}

TEST(SerializerJSONTest, PushListEmpty) {
    WBE::SerializerJSON main_serializer;
    
    // Create empty serializer
    WBE::SerializerJSON empty_item;
    
    main_serializer.push_list("empty_list", empty_item);
    main_serializer.register_serialize("has_items", false);
    
    std::string result = main_serializer.dump();
    
    // Use nlohmann json to verify empty list handling
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_TRUE(json_obj.contains("empty_list"));
    ASSERT_TRUE(json_obj.contains("has_items"));
    ASSERT_EQ(json_obj["has_items"].get<bool>(), false);
    
    // Verify empty list structure
    auto empty_list = json_obj["empty_list"];
    ASSERT_TRUE(empty_list.is_array());
    ASSERT_EQ(empty_list.size(), 1);  // Should contain one empty item
}

TEST(SerializerJSONTest, Edge) {
    WBE::SerializerJSON serializer;
    ASSERT_EQ(serializer.get_current_context(), "base");
    ASSERT_EQ(serializer.get_context_depth(), 0);
    ASSERT_EQ(serializer.dump(), "null");

    serializer.push_context("test_context1");
    ASSERT_EQ(serializer.get_current_context(), "test_context1");
    ASSERT_EQ(serializer.get_context_depth(), 1);
    serializer.pop_context();
    ASSERT_EQ(serializer.get_current_context(), "base");
    ASSERT_EQ(serializer.get_context_depth(), 0);
    ASSERT_EQ(serializer.dump(), "{\"test_context1\":null}");

    ASSERT_THROW(serializer.pop_context(), std::runtime_error);
}

TEST(SerializerJSONTest, RoundTripSerialization) {
    WBE::SerializerJSON serializer;
    
    // Create complex data structure
    serializer.register_serialize("string_val", "Hello, World!");
    serializer.register_serialize("int_val", 42);
    serializer.register_serialize("double_val", 3.14159);
    serializer.register_serialize("bool_val", true);
    serializer.register_serialize("negative_int", -123);
    serializer.register_serialize("zero_val", 0);
    
    // Add nested context
    serializer.push_context("nested_data");
    serializer.register_serialize("nested_string", "nested value");
    serializer.register_serialize("nested_number", 99);
    
    // Add deeper nesting
    serializer.push_context("deep_nested");
    serializer.register_serialize("deep_val", "very deep");
    const char* deep_cstr = "deep const char";
    serializer.register_serialize("deep_cstr", deep_cstr);
    serializer.pop_context();
    serializer.pop_context();
    
    // Serialize to JSON
    std::string json_result = serializer.dump();
    
    // Parse back the JSON
    nlohmann::json json_obj = nlohmann::json::parse(json_result);
    
    // Verify all values are preserved
    ASSERT_EQ(json_obj["string_val"].get<std::string>(), "Hello, World!");
    ASSERT_EQ(json_obj["int_val"].get<int>(), 42);
    ASSERT_DOUBLE_EQ(json_obj["double_val"].get<double>(), 3.14159);
    ASSERT_EQ(json_obj["bool_val"].get<bool>(), true);
    ASSERT_EQ(json_obj["negative_int"].get<int>(), -123);
    ASSERT_EQ(json_obj["zero_val"].get<int>(), 0);
    
    // Verify nested data
    auto nested = json_obj["nested_data"];
    ASSERT_EQ(nested["nested_string"].get<std::string>(), "nested value");
    ASSERT_EQ(nested["nested_number"].get<int>(), 99);
    
    // Verify deep nested data
    auto deep_nested = nested["deep_nested"];
    ASSERT_EQ(deep_nested["deep_val"].get<std::string>(), "very deep");
    ASSERT_EQ(deep_nested["deep_cstr"].get<std::string>(), "deep const char");
}

TEST(SerializerJSONTest, SpecialCharactersAndEdgeCases) {
    WBE::SerializerJSON serializer;
    
    // Test various special characters and edge cases
    const char* unicode_str = "Unicode: αβγ δεζ 中文 🚀";
    const char* json_special = "JSON special: \"quotes\" and \\backslashes\\ and /slashes/";
    const char* whitespace_str = "Whitespace:\n\t\r  test";
    const char* empty_str = "";
    const char* single_char = "a";
    
    serializer.register_serialize("unicode", unicode_str);
    serializer.register_serialize("json_special", json_special);
    serializer.register_serialize("whitespace", whitespace_str);
    serializer.register_serialize("empty", empty_str);
    serializer.register_serialize("single_char", single_char);
    
    // Test numeric edge cases
    serializer.register_serialize("max_int", std::numeric_limits<int>::max());
    serializer.register_serialize("min_int", std::numeric_limits<int>::min());
    serializer.register_serialize("max_double", std::numeric_limits<double>::max());
    serializer.register_serialize("min_double", std::numeric_limits<double>::lowest());
    serializer.register_serialize("infinity", std::numeric_limits<double>::infinity());
    
    std::string result = serializer.dump();
    
    // Parse back and verify
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_EQ(json_obj["unicode"].get<std::string>(), unicode_str);
    ASSERT_EQ(json_obj["json_special"].get<std::string>(), json_special);
    ASSERT_EQ(json_obj["whitespace"].get<std::string>(), whitespace_str);
    ASSERT_EQ(json_obj["empty"].get<std::string>(), "");
    ASSERT_EQ(json_obj["single_char"].get<std::string>(), "a");
    
    ASSERT_EQ(json_obj["max_int"].get<int>(), std::numeric_limits<int>::max());
    ASSERT_EQ(json_obj["min_int"].get<int>(), std::numeric_limits<int>::min());
    ASSERT_DOUBLE_EQ(json_obj["max_double"].get<double>(), std::numeric_limits<double>::max());
    ASSERT_DOUBLE_EQ(json_obj["min_double"].get<double>(), std::numeric_limits<double>::lowest());
}

TEST(SerializerJSONTest, LargeDataSets) {
    WBE::SerializerJSON main_serializer;
    
    // Test with many items
    const int num_items = 1000;
    
    for (int i = 0; i < num_items; ++i) {
        WBE::SerializerJSON item;
        item.register_serialize("id", i);
        item.register_serialize("name", std::string("Item_") + std::to_string(i));
        item.register_serialize("active", i % 2 == 0);
        item.register_serialize("value", static_cast<double>(i) * 3.14);
        
        main_serializer.push_list("items", item);
    }
    
    main_serializer.register_serialize("total_count", num_items);
    main_serializer.register_serialize("test_name", "Large Dataset Test");
    
    std::string result = main_serializer.dump();
    ASSERT_FALSE(result.empty());
    
    // Parse back and verify structure
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_EQ(json_obj["total_count"].get<int>(), num_items);
    ASSERT_EQ(json_obj["test_name"].get<std::string>(), "Large Dataset Test");
    
    auto items = json_obj["items"];
    ASSERT_TRUE(items.is_array());
    ASSERT_EQ(items.size(), num_items);
    
    // Spot check a few items
    ASSERT_EQ(items[0]["id"].get<int>(), 0);
    ASSERT_EQ(items[0]["name"].get<std::string>(), "Item_0");
    ASSERT_EQ(items[0]["active"].get<bool>(), true);
    
    ASSERT_EQ(items[num_items-1]["id"].get<int>(), num_items-1);
    ASSERT_EQ(items[num_items-1]["active"].get<bool>(), (num_items-1) % 2 == 0);
}

TEST(SerializerJSONTest, ErrorHandling) {
    WBE::SerializerJSON serializer;
    
    // Test context stack underflow
    ASSERT_THROW(serializer.pop_context(), std::runtime_error);
    
    // Test dumping with unbalanced contexts
    serializer.push_context("test1");
    serializer.push_context("test2");
    ASSERT_THROW(serializer.dump(), std::runtime_error);
    
    // Clean up contexts
    serializer.pop_context();
    serializer.pop_context();
    
    // Test clear functionality
    serializer.register_serialize("before_clear", "value");
    serializer.clear();
    
    std::string after_clear = serializer.dump();
    ASSERT_EQ(after_clear, "null");
    
    // Verify serializer is usable after clear
    serializer.register_serialize("after_clear", "new_value");
    std::string after_use = serializer.dump();
    ASSERT_NE(after_use.find("after_clear"), std::string::npos);
    ASSERT_NE(after_use.find("new_value"), std::string::npos);
}

TEST(SerializerJSONTest, ContextManagement) {
    WBE::SerializerJSON serializer;
    
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
    serializer.register_serialize("deep_data", "level3_value");
    serializer.pop_context();
    
    serializer.register_serialize("mid_data", "level2_value");
    serializer.pop_context();
    
    serializer.register_serialize("shallow_data", "level1_value");
    serializer.pop_context();
    
    serializer.register_serialize("root_data", "root_value");
    
    std::string result = serializer.dump();
    
    // Parse and verify nested structure
    nlohmann::json json_obj = nlohmann::json::parse(result);
    
    ASSERT_EQ(json_obj["root_data"].get<std::string>(), "root_value");
    
    auto level1 = json_obj["level1"];
    ASSERT_EQ(level1["shallow_data"].get<std::string>(), "level1_value");
    
    auto level2 = level1["level2"];
    ASSERT_EQ(level2["mid_data"].get<std::string>(), "level2_value");
    
    auto level3 = level2["level3"];
    ASSERT_EQ(level3["deep_data"].get<std::string>(), "level3_value");
}

#endif
