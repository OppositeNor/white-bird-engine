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
#ifndef __WBE_PARSER_JSON_TEST_HH__
#define __WBE_PARSER_JSON_TEST_HH__

#include <gtest/gtest.h>
#include "core/parser/parser_json.hh"
#include "parser_test_general.hh"

namespace WBE = WhiteBirdEngine;

TEST(ParserJSONTest, ParseFromBuffer) {
    WBE::ParserJSON parser;
    test_parser_general(parser,
                        R"({ "fruit" : "apple", "vegetable" : "lettice", "number" : 123 })",
                        R"({ "vegetable" : "lettice", "numbers" : [123, -200, 5, 60, -123] })",
                        R"({ "test_pair" : {
                                "test_key1" : "test_val1",
                                "test_key2" : 123,
                                "test_key3" : 3.1415926
                            }, "numbers" : [123, -200, 5, 60, -123] })");
}

// Test edge cases

TEST(ParserJSONTest, ParseFromBufferEdgeCases) {
    WBE::ParserJSON parser;
    // Empty JSON
    parser.parse_from_buffer(R"({})");
    ASSERT_TRUE(parser.get_data().get_all_keys().empty());

    // Nested empty objects
    parser.parse_from_buffer(R"({ "empty_obj": {} })");
    ASSERT_TRUE(parser.get_data().template get_value<WBE::ParserJSON::DataType>("empty_obj").get_all_keys().empty());

    // Deeply nested structures
    parser.parse_from_buffer(R"({ "level1": { "level2": { "level3": { "level4": "deep_value" } } } })");
    ASSERT_STREQ(parser.get_data()
                     .template get_value<WBE::ParserJSON::DataType>("level1")
                     .template get_value<WBE::ParserJSON::DataType>("level2")
                     .template get_value<WBE::ParserJSON::DataType>("level3")
                     .template get_value<std::string>("level4")
                     .c_str(),
                 "deep_value");

    // Null values
    parser.parse_from_buffer(R"({ "key_with_null": null })");
    ASSERT_TRUE(parser.get_data().template get_value<std::nullptr_t>("key_with_null") == nullptr);
}

TEST(ParserJSONTest, ParseListOfObjects) {
    WBE::ParserJSON parser;
    
    // Test parsing an array of simple objects
    const std::string json_simple_objects = R"({
        "users": [
            {
                "id": 1,
                "name": "Alice",
                "active": true
            },
            {
                "id": 2,
                "name": "Bob",
                "active": false
            },
            {
                "id": 3,
                "name": "Charlie",
                "active": true
            }
        ]
    })";
    
    parser.parse_from_buffer(json_simple_objects);
    
    // Get the list of objects
    auto users = parser.get_value<std::vector<WBE::JSONData>>("users");
    ASSERT_EQ(users.size(), 3);
    
    // Verify first user
    ASSERT_EQ(users[0].get_value<int>("id"), 1);
    ASSERT_EQ(users[0].get_value<std::string>("name"), "Alice");
    ASSERT_EQ(users[0].get_value<bool>("active"), true);
    
    // Verify second user
    ASSERT_EQ(users[1].get_value<int>("id"), 2);
    ASSERT_EQ(users[1].get_value<std::string>("name"), "Bob");
    ASSERT_EQ(users[1].get_value<bool>("active"), false);
    
    // Verify third user
    ASSERT_EQ(users[2].get_value<int>("id"), 3);
    ASSERT_EQ(users[2].get_value<std::string>("name"), "Charlie");
    ASSERT_EQ(users[2].get_value<bool>("active"), true);
}

TEST(ParserJSONTest, ParseListOfNestedObjects) {
    WBE::ParserJSON parser;
    
    // Test parsing an array of objects with nested structures
    const std::string json_nested_objects = R"({
        "products": [
            {
                "id": 101,
                "name": "Laptop",
                "details": {
                    "brand": "TechCorp",
                    "model": "X1000",
                    "specs": {
                        "ram": "16GB",
                        "storage": "512GB SSD"
                    }
                },
                "tags": ["electronics", "computers", "portable"]
            },
            {
                "id": 102,
                "name": "Mouse",
                "details": {
                    "brand": "PeripheralCo",
                    "model": "M200",
                    "specs": {
                        "dpi": "1600",
                        "buttons": "5"
                    }
                },
                "tags": ["electronics", "accessories"]
            }
        ]
    })";
    
    parser.parse_from_buffer(json_nested_objects);
    
    // Get the list of products
    auto products = parser.get_value<std::vector<WBE::JSONData>>("products");
    ASSERT_EQ(products.size(), 2);
    
    // Verify first product
    ASSERT_EQ(products[0].get_value<int>("id"), 101);
    ASSERT_EQ(products[0].get_value<std::string>("name"), "Laptop");
    
    // Verify nested details of first product
    auto details1 = products[0].get_value<WBE::JSONData>("details");
    ASSERT_EQ(details1.get_value<std::string>("brand"), "TechCorp");
    ASSERT_EQ(details1.get_value<std::string>("model"), "X1000");
    
    // Verify deeply nested specs
    auto specs1 = details1.get_value<WBE::JSONData>("specs");
    ASSERT_EQ(specs1.get_value<std::string>("ram"), "16GB");
    ASSERT_EQ(specs1.get_value<std::string>("storage"), "512GB SSD");
    
    // Verify tags array within the object
    auto tags1 = products[0].get_value<std::vector<std::string>>("tags");
    ASSERT_EQ(tags1.size(), 3);
    ASSERT_EQ(tags1[0], "electronics");
    ASSERT_EQ(tags1[1], "computers");
    ASSERT_EQ(tags1[2], "portable");
    
    // Verify second product
    ASSERT_EQ(products[1].get_value<int>("id"), 102);
    ASSERT_EQ(products[1].get_value<std::string>("name"), "Mouse");
    
    auto details2 = products[1].get_value<WBE::JSONData>("details");
    auto specs2 = details2.get_value<WBE::JSONData>("specs");
    ASSERT_EQ(specs2.get_value<std::string>("dpi"), "1600");
    ASSERT_EQ(specs2.get_value<std::string>("buttons"), "5");
    
    auto tags2 = products[1].get_value<std::vector<std::string>>("tags");
    ASSERT_EQ(tags2.size(), 2);
    ASSERT_EQ(tags2[0], "electronics");
    ASSERT_EQ(tags2[1], "accessories");
}

TEST(ParserJSONTest, ParseEmptyAndMixedLists) {
    WBE::ParserJSON parser;
    
    // Test empty array
    const std::string json_empty_array = R"({
        "empty_list": [],
        "mixed_content": [
            { "type": "object", "value": 42 },
            { "type": "string", "value": "hello" },
            { "type": "boolean", "value": true }
        ]
    })";
    
    parser.parse_from_buffer(json_empty_array);
    
    // Verify empty list
    auto empty_list = parser.get_value<std::vector<WBE::JSONData>>("empty_list");
    ASSERT_EQ(empty_list.size(), 0);
    
    // Verify mixed content list
    auto mixed_content = parser.get_value<std::vector<WBE::JSONData>>("mixed_content");
    ASSERT_EQ(mixed_content.size(), 3);
    
    // Verify each mixed item
    ASSERT_EQ(mixed_content[0].get_value<std::string>("type"), "object");
    ASSERT_EQ(mixed_content[0].get_value<int>("value"), 42);
    
    ASSERT_EQ(mixed_content[1].get_value<std::string>("type"), "string");
    ASSERT_EQ(mixed_content[1].get_value<std::string>("value"), "hello");
    
    ASSERT_EQ(mixed_content[2].get_value<std::string>("type"), "boolean");
    ASSERT_EQ(mixed_content[2].get_value<bool>("value"), true);
}

#endif
