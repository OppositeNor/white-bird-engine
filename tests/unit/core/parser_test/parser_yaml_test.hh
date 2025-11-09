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
#ifndef __WBE_PARSER_YAML_TEST_HH__
#define __WBE_PARSER_YAML_TEST_HH__

#include "core/parser/parser_yaml.hh"
#include "parser_test_general.hh"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

namespace WBE = WhiteBirdEngine;

inline std::string general_test_str1 = \
R"(
# this is a comment
fruit: apple # another comment
vegetable: "lettice"
number: 123
)";

inline std::string general_test_str2 = \
R"(
# this is a comment
vegetable: "lettice"
numbers: [123, -200, 5, 60, -123]
)";

inline std::string general_test_str2_ident = \
R"(
# this is a comment
vegetable: "lettice"
numbers: 
    - 123
    - -200
    - 5
    - 60
    - -123
)";

inline std::string general_test_str3 = \
R"(
test_pair:
    test_key1: test_val1
    test_key2: 123
    test_key3: 3.1415926
numbers:
    - 123
    - -200
    - 5
    - 60
    - -123
)";

TEST(ParserYAMLTest, ParseFromBuffer) {
    WBE::ParserYAML parser;
    test_parser_general(parser, general_test_str1, general_test_str2, general_test_str3);
    test_parser_general(parser, general_test_str1, general_test_str2_ident, general_test_str3);
}

// Test edge cases
TEST(ParserYAMLTest, ParseFromBufferEdgeCases) {
    WBE::ParserYAML parser;
    // Empty YAML
    parser.parse_from_buffer(R"({})");
    ASSERT_TRUE(parser.get_data().get_all_keys().empty());
    // Nested empty objects
    parser.parse_from_buffer(R"({ empty_obj: {} })");
    ASSERT_TRUE(parser.get_data().template get_value<WBE::ParserYAML::DataType>("empty_obj").get_all_keys().empty());
    // Deeply nested structures
    parser.parse_from_buffer(R"({ level1: { level2: { level3: { level4: deep_value } } } })");
    ASSERT_STREQ(parser.get_data()
                     .template get_value<WBE::ParserYAML::DataType>("level1")
                     .template get_value<WBE::ParserYAML::DataType>("level2")
                     .template get_value<WBE::ParserYAML::DataType>("level3")
                     .template get_value<std::string>("level4")
                     .c_str(),
                 "deep_value");
  // Null values
  parser.parse_from_buffer(R"({ key_with_null: null })");
  ASSERT_TRUE(parser.contains("key_with_null"));
  // Getting the key as YAMLData should produce an empty/null node (no keys)
  auto null_node = parser.get_value<WBE::YAMLData>("key_with_null");
  ASSERT_TRUE(null_node.get_all_keys().empty());
}

TEST(ParserYAMLTest, ParseListOfObjects) {
    WBE::ParserYAML parser;
    
    // Test parsing an array of simple objects
    const std::string yaml_simple_objects = R"(
users:
  - id: 1
    name: Alice
    active: true
  - id: 2
    name: Bob
    active: false
  - id: 3
    name: Charlie
    active: true
)";
    
  parser.parse_from_buffer(yaml_simple_objects);

  // Get the list of objects as vector<YAMLData>
  auto users = parser.get_value<std::vector<WBE::YAMLData>>("users");
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

TEST(ParserYAMLTest, ParseListOfNestedObjects) {
    WBE::ParserYAML parser;
    
    // Test parsing an array of objects with nested structures
    const std::string yaml_nested_objects = R"(
products:
  - id: 101
    name: Laptop
    details:
      brand: TechCorp
      model: X1000
      specs:
        ram: 16GB
        storage: 512GB SSD
    tags:
      - electronics
      - computers
      - portable
  - id: 102
    name: Mouse
    details:
      brand: PeripheralCo
      model: M200
      specs:
        dpi: "1600"
        buttons: "5"
    tags:
      - electronics
      - accessories
)";
    
  parser.parse_from_buffer(yaml_nested_objects);

  // Get the list of products
  auto products = parser.get_value<std::vector<WBE::YAMLData>>("products");
  ASSERT_EQ(products.size(), 2);

  // Verify first product
  ASSERT_EQ(products[0].get_value<int>("id"), 101);
  ASSERT_EQ(products[0].get_value<std::string>("name"), "Laptop");

  // Verify nested details of first product
  auto details1 = products[0].get_value<WBE::YAMLData>("details");
  ASSERT_EQ(details1.get_value<std::string>("brand"), "TechCorp");
  ASSERT_EQ(details1.get_value<std::string>("model"), "X1000");

  // Verify deeply nested specs
  auto specs1 = details1.get_value<WBE::YAMLData>("specs");
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

  auto details2 = products[1].get_value<WBE::YAMLData>("details");
  auto specs2 = details2.get_value<WBE::YAMLData>("specs");
  ASSERT_EQ(specs2.get_value<std::string>("dpi"), "1600");
  ASSERT_EQ(specs2.get_value<std::string>("buttons"), "5");

  auto tags2 = products[1].get_value<std::vector<std::string>>("tags");
  ASSERT_EQ(tags2.size(), 2);
  ASSERT_EQ(tags2[0], "electronics");
  ASSERT_EQ(tags2[1], "accessories");
}

TEST(ParserYAMLTest, ParseEmptyAndMixedLists) {
    WBE::ParserYAML parser;
    
    // Test empty array and mixed content
    const std::string yaml_empty_and_mixed = R"(
empty_list: []
mixed_content:
  - type: object
    value: 42
  - type: string
    value: hello
  - type: boolean
    value: true
simple_array: [1, 2, 3, 4, 5]
)";
    
    parser.parse_from_buffer(yaml_empty_and_mixed);
    
    // Verify empty list can be accessed
    ASSERT_TRUE(parser.contains("empty_list"));
    auto empty_list = parser.get_value<WBE::YAMLData>("empty_list");
    
    // Verify mixed content list can be accessed
    ASSERT_TRUE(parser.contains("mixed_content"));
    auto mixed_content = parser.get_value<WBE::YAMLData>("mixed_content");
    
    // Since we can't access the internal node directly, we verify that
    // the YAML parser can handle these structures without throwing exceptions
    // This tests the parser's ability to handle empty sequences and
    // sequences of mixed object types
    
    // Test simple array parsing (existing functionality)
    auto simple_array = parser.get_value<std::vector<int>>("simple_array");
    ASSERT_EQ(simple_array.size(), 5);
    ASSERT_EQ(simple_array[0], 1);
    ASSERT_EQ(simple_array[4], 5);
}

TEST(ParserYAMLTest, BufferRetrieval) {
    WBE::ParserYAML parser;
    
    const std::string yaml_data = R"(
short_text: "Hello"
medium_text: "This is a medium length text"
long_text: "This is a very long text that should test buffer capacity limits and bounds checking functionality"
empty_text: ""
special_chars: "Line1\nLine2\tTab\"Quote'"
)";
    
    parser.parse_from_buffer(yaml_data);
    
    // Test retrieving into different buffer sizes
    WBE::Buffer<16> small_buffer;
    WBE::Buffer<64> medium_buffer;
    WBE::Buffer<128> large_buffer;
    
    // Test short text fits in all buffers
    std::string key = "short_text";
    parser.get_data().get_value(key, small_buffer);
    ASSERT_STREQ(small_buffer.buffer, "Hello");
    
    parser.get_data().get_value(key, medium_buffer);
    ASSERT_STREQ(medium_buffer.buffer, "Hello");
    
    parser.get_data().get_value(key, large_buffer);
    ASSERT_STREQ(large_buffer.buffer, "Hello");
    
    // Test medium text fits in medium and large buffers
    key = "medium_text";
    parser.get_data().get_value(key, medium_buffer);
    ASSERT_STREQ(medium_buffer.buffer, "This is a medium length text");
    
    parser.get_data().get_value(key, large_buffer);
    ASSERT_STREQ(large_buffer.buffer, "This is a medium length text");
    
    // Test empty text
    key = "empty_text";
    parser.get_data().get_value(key, small_buffer);
    ASSERT_STREQ(small_buffer.buffer, "");
    
    // Test special characters
    key = "special_chars";
    parser.get_data().get_value(key, medium_buffer);
    ASSERT_STREQ(medium_buffer.buffer, "Line1\nLine2\tTab\"Quote'");
}

TEST(ParserYAMLTest, BufferBoundsChecking) {
    WBE::ParserYAML parser;
    
    const std::string yaml_data = R"(
long_text: "This is a very long text that exceeds small buffer capacity"
)";
    
    parser.parse_from_buffer(yaml_data);
    
    // Test that attempting to retrieve long text into small buffer throws exception
    WBE::Buffer<16> small_buffer;
    std::string key = "long_text";
    ASSERT_THROW(parser.get_data().get_value(key, small_buffer), std::runtime_error);
    
    // Test that it works with appropriately sized buffer
    WBE::Buffer<128> large_buffer;
    ASSERT_NO_THROW(parser.get_data().get_value(key, large_buffer));
    ASSERT_STREQ(large_buffer.buffer, "This is a very long text that exceeds small buffer capacity");
}

TEST(ParserYAMLTest, BufferMaxCapacityRetrieval) {
    WBE::ParserYAML parser;
    
    // Test string that exactly fits buffer capacity (15 chars + null terminator for size 16)
    const std::string yaml_data = R"(
max_text: "123456789012345"
)";
    
    parser.parse_from_buffer(yaml_data);
    
    WBE::Buffer<16> buffer;
    std::string key = "max_text";
    parser.get_data().get_value(key, buffer);
    ASSERT_STREQ(buffer.buffer, "123456789012345");
    
    // Test string that's one character too long
    const std::string yaml_data_too_long = R"(
too_long: "1234567890123456"
)";
    
    parser.parse_from_buffer(yaml_data_too_long);
    key = "too_long";
    ASSERT_THROW(parser.get_data().get_value(key, buffer), std::runtime_error);
}

TEST(ParserYAMLTest, BufferWithYAMLSpecificFeatures) {
    WBE::ParserYAML parser;
    
    const std::string yaml_data = R"(
multiline_literal: |
  This is a literal
  multiline string
  with preserved newlines
  
multiline_folded: >
  This is a folded
  multiline string
  that becomes a single line
)";
    
    parser.parse_from_buffer(yaml_data);
    
    // Test multiline literal retrieval
    WBE::Buffer<128> buffer;
    std::string key = "multiline_literal";
    parser.get_data().get_value(key, buffer);
    ASSERT_TRUE(strstr(buffer.buffer, "This is a literal\nmultiline string") != nullptr);
    
    // Test multiline folded retrieval
    key = "multiline_folded";
    parser.get_data().get_value(key, buffer);
    ASSERT_TRUE(strstr(buffer.buffer, "This is a folded multiline string") != nullptr);
}

TEST(ParserYAMLTest, ComplexNestedStructures) {
    WBE::ParserYAML parser;
    
    // Test deeply nested YAML structures
    const std::string complex_yaml = R"(
application:
  name: "Test App"
  version: "1.2.3"
  features:
    authentication:
      enabled: true
      methods:
        - oauth
        - basic
        - token
      config:
        timeout: 3600
        max_attempts: 3
        providers:
          google:
            client_id: "google_client_123"
            enabled: true
          facebook:
            app_id: "fb_app_456"
            enabled: false
    logging:
      level: info
      destinations:
        - console
        - file
        - remote
      rotation:
        max_size: "10MB"
        max_files: 5
        compress: true
  dependencies:
    - name: "yaml-cpp"
      version: "0.7.0"
      required: true
    - name: "nlohmann-json"
      version: "3.11.2"
      required: true
    - name: "optional-lib"
      version: "1.0.0"
      required: false
)";
    
    parser.parse_from_buffer(complex_yaml);
    
    // Test top-level access
    auto app = parser.get_value<WBE::YAMLData>("application");
    ASSERT_EQ(app.get_value<std::string>("name"), "Test App");
    ASSERT_EQ(app.get_value<std::string>("version"), "1.2.3");
    
    // Test nested features
    auto features = app.get_value<WBE::YAMLData>("features");
    auto auth = features.get_value<WBE::YAMLData>("authentication");
    ASSERT_EQ(auth.get_value<bool>("enabled"), true);
    
    // Test sequence within nested structure
    auto methods = auth.get_value<std::vector<std::string>>("methods");
    ASSERT_EQ(methods.size(), 3);
    ASSERT_EQ(methods[0], "oauth");
    ASSERT_EQ(methods[1], "basic");
    ASSERT_EQ(methods[2], "token");
    
    // Test deeply nested config
    auto config = auth.get_value<WBE::YAMLData>("config");
    ASSERT_EQ(config.get_value<int>("timeout"), 3600);
    ASSERT_EQ(config.get_value<int>("max_attempts"), 3);
    
    // Test providers mapping
    auto providers = config.get_value<WBE::YAMLData>("providers");
    auto google = providers.get_value<WBE::YAMLData>("google");
    ASSERT_EQ(google.get_value<std::string>("client_id"), "google_client_123");
    ASSERT_EQ(google.get_value<bool>("enabled"), true);
    
    auto facebook = providers.get_value<WBE::YAMLData>("facebook");
    ASSERT_EQ(facebook.get_value<std::string>("app_id"), "fb_app_456");
    ASSERT_EQ(facebook.get_value<bool>("enabled"), false);
    
    // Test other nested sections
    auto logging = features.get_value<WBE::YAMLData>("logging");
    ASSERT_EQ(logging.get_value<std::string>("level"), "info");
    
    auto destinations = logging.get_value<std::vector<std::string>>("destinations");
    ASSERT_EQ(destinations.size(), 3);
    ASSERT_EQ(destinations[0], "console");
    ASSERT_EQ(destinations[1], "file");
    ASSERT_EQ(destinations[2], "remote");
}

TEST(ParserYAMLTest, SequenceVariations) {
    WBE::ParserYAML parser;
    
    // Test various YAML sequence types and structures
    const std::string sequence_yaml = R"(
empty_sequence: []
string_sequence:
  - one
  - two
  - three
number_sequence: [1, 2, 3, 4, 5]
boolean_sequence:
  - true
  - false
  - true
  - true
  - false
inline_sequence: [quick, brown, fox]
nested_sequences:
  - [1, 2, 3]
  - [a, b, c]
  - [true, false]
sequence_of_objects:
  - id: 1
    data: [10, 20]
  - id: 2
    data: [30, 40]
  - id: 3
    data: [50, 60]
multiline_sequence:
  - |
    First multiline
    string entry
  - |
    Second multiline
    string entry
)";
    
    parser.parse_from_buffer(sequence_yaml);
    
    // Test empty sequence
    auto empty_seq = parser.get_value<std::vector<WBE::YAMLData>>("empty_sequence");
    ASSERT_EQ(empty_seq.size(), 0);
    
    // Test string sequence
    auto string_seq = parser.get_value<std::vector<std::string>>("string_sequence");
    ASSERT_EQ(string_seq.size(), 3);
    ASSERT_EQ(string_seq[1], "two");
    
    // Test number sequence
    auto number_seq = parser.get_value<std::vector<int>>("number_sequence");
    ASSERT_EQ(number_seq.size(), 5);
    ASSERT_EQ(number_seq[3], 4);
    
    // Test boolean sequence
    auto boolean_seq = parser.get_value<std::vector<bool>>("boolean_sequence");
    ASSERT_EQ(boolean_seq.size(), 5);
    ASSERT_EQ(boolean_seq[0], true);
    ASSERT_EQ(boolean_seq[1], false);
    
    // Test inline sequence
    auto inline_seq = parser.get_value<std::vector<std::string>>("inline_sequence");
    ASSERT_EQ(inline_seq.size(), 3);
    ASSERT_EQ(inline_seq[0], "quick");
    ASSERT_EQ(inline_seq[1], "brown");
    ASSERT_EQ(inline_seq[2], "fox");
}

TEST(ParserYAMLTest, ErrorHandlingAndValidation) {
    WBE::ParserYAML parser;
    
    // Test malformed YAML handling
    ASSERT_THROW(parser.parse_from_buffer("invalid: yaml: content: ["), std::exception);
    
    // Test valid YAML with various edge cases
    parser.parse_from_buffer(R"(
null_value: null
empty_string: ""
zero: 0
boolean_true: true
boolean_false: false
)");
    
    // Test accessing non-existent keys
    ASSERT_FALSE(parser.contains("non_existent_key"));
    ASSERT_THROW(parser.get_value<std::string>("non_existent_key"), std::exception);
    
    // Test type access and validation
    ASSERT_TRUE(parser.contains("zero"));
    ASSERT_EQ(parser.get_value<int>("zero"), 0);
    
    // Test accessing valid keys
    ASSERT_TRUE(parser.contains("null_value"));
    ASSERT_TRUE(parser.contains("empty_string"));
    ASSERT_EQ(parser.get_value<std::string>("empty_string"), "");
    ASSERT_EQ(parser.get_value<bool>("boolean_true"), true);
    ASSERT_EQ(parser.get_value<bool>("boolean_false"), false);
}

TEST(ParserYAMLTest, KeyManagementAndUtilities) {
    WBE::ParserYAML parser;
    
    const std::string test_yaml = R"(
key1: value1
key2: 123
key3: true
nested:
  subkey1: subvalue1
  subkey2: 456
sequence:
  - item1
  - item2
  - item3
)";
    
    parser.parse_from_buffer(test_yaml);
    
    // Test get_all_keys functionality
    auto keys = parser.get_all_keys();
    ASSERT_EQ(keys.size(), 5);
    
    // Keys should include all top-level keys
    std::sort(keys.begin(), keys.end());
    std::vector<std::string> expected_keys = {"key1", "key2", "key3", "nested", "sequence"};
    std::sort(expected_keys.begin(), expected_keys.end());
    ASSERT_EQ(keys, expected_keys);
    
    // Test nested key access
    auto nested = parser.get_value<WBE::YAMLData>("nested");
    auto nested_keys = nested.get_all_keys();
    ASSERT_EQ(nested_keys.size(), 2);
    
    std::sort(nested_keys.begin(), nested_keys.end());
    std::vector<std::string> expected_nested = {"subkey1", "subkey2"};
    std::sort(expected_nested.begin(), expected_nested.end());
    ASSERT_EQ(nested_keys, expected_nested);
    
    // Test contains functionality at different levels
    ASSERT_TRUE(parser.contains("key1"));
    ASSERT_TRUE(parser.contains("nested"));
    ASSERT_TRUE(parser.contains("sequence"));
    ASSERT_FALSE(parser.contains("subkey1")); // Not at top level
    
    ASSERT_TRUE(nested.contains("subkey1"));
    ASSERT_TRUE(nested.contains("subkey2"));
    ASSERT_FALSE(nested.contains("key1")); // Not in nested level
}

TEST(ParserYAMLTest, ParseGLMVectors) {
    WBE::ParserYAML parser;

    const std::string glm_yaml = R"(
v2:
  x: 1.5
  y: 2.5
v3:
  x: 1.0
  y: 2.0
  z: 3.0
v4:
  x: -1.25
  y: 0.0
  z: 4.5
  w: 8.75
)";

    parser.parse_from_buffer(glm_yaml);

    glm::vec2 v2 = parser.get_value<glm::vec2>("v2");
    ASSERT_FLOAT_EQ(v2.x, 1.5f);
    ASSERT_FLOAT_EQ(v2.y, 2.5f);

    glm::vec3 v3 = parser.get_value<glm::vec3>("v3");
    ASSERT_FLOAT_EQ(v3.x, 1.0f);
    ASSERT_FLOAT_EQ(v3.y, 2.0f);
    ASSERT_FLOAT_EQ(v3.z, 3.0f);

    glm::vec4 v4 = parser.get_value<glm::vec4>("v4");
    ASSERT_FLOAT_EQ(v4.x, -1.25f);
    ASSERT_FLOAT_EQ(v4.y, 0.0f);
    ASSERT_FLOAT_EQ(v4.z, 4.5f);
    ASSERT_FLOAT_EQ(v4.w, 8.75f);

    glm::vec2 v2ip;
    parser.get_value<glm::vec2>("v2", v2ip);
    ASSERT_FLOAT_EQ(v2ip.x, 1.5f);
    ASSERT_FLOAT_EQ(v2ip.y, 2.5f);

    glm::vec3 v3ip;
    parser.get_value<glm::vec3>("v3", v3ip);
    ASSERT_FLOAT_EQ(v3ip.x, 1.0f);
    ASSERT_FLOAT_EQ(v3ip.y, 2.0f);
    ASSERT_FLOAT_EQ(v3ip.z, 3.0f);

    glm::vec4 v4ip;
    parser.get_value<glm::vec4>("v4", v4ip);
    ASSERT_FLOAT_EQ(v4ip.x, -1.25f);
    ASSERT_FLOAT_EQ(v4ip.y, 0.0f);
    ASSERT_FLOAT_EQ(v4ip.z, 4.5f);
    ASSERT_FLOAT_EQ(v4ip.w, 8.75f);
}

TEST(ParserYAMLTest, ParseGLMVectorsMissingFields) {
    WBE::ParserYAML parser;

    const std::string glm_yaml = R"(
v2:
  x: 1.0
v3:
  x: 1.0
  y: 2.0
v4:
  x: 0.0
  y: 0.0
  z: 0.0
)";

    parser.parse_from_buffer(glm_yaml);

    // Missing components should produce exceptions when accessed
    ASSERT_THROW(parser.get_value<glm::vec2>("v2"), std::exception);
    ASSERT_THROW(parser.get_value<glm::vec3>("v3"), std::exception);
    ASSERT_THROW(parser.get_value<glm::vec4>("v4"), std::exception);
}

TEST(ParserYAMLTest, YAMLDataSetMethod) {
  // Test the single-argument set(T&&) that assigns the current node
  WBE::YAMLData root;

  // Primitives: set the node to a scalar
  root.set(std::string("apple"));
  ASSERT_EQ(root.get<std::string>(), "apple");

  root.set(123);
  ASSERT_EQ(root.get<int>(), 123);

  root.set(3.14);
  ASSERT_DOUBLE_EQ(root.get<double>(), 3.14);

  // Arrays
  root.set(std::vector<int>{10, 20, 30});
  auto nums = root.get<std::vector<int>>();
  ASSERT_EQ(nums.size(), 3);
  ASSERT_EQ(nums[0], 10);
  ASSERT_EQ(nums[1], 20);
  ASSERT_EQ(nums[2], 30);

  // Buffer type
  WBE::Buffer<16> buf;
  strcpy(buf.buffer, "bufval");
  root.set(buf);
  ASSERT_EQ(root.get<std::string>(), "bufval");

  // glm vector
  glm::vec3 v{1.0f, 2.0f, 3.0f};
  root.set(v);
  auto v_out = root.get<glm::vec3>();
  ASSERT_FLOAT_EQ(v_out.x, 1.0f);
  ASSERT_FLOAT_EQ(v_out.y, 2.0f);
  ASSERT_FLOAT_EQ(v_out.z, 3.0f);

  // Nested YAMLData by copy
  WBE::YAMLData child;
  child.set_value("key1", "value1");
  child.set_value("key2", 777);
  root.set(child); // copy

  auto child_copy = root.get<WBE::YAMLData>();
  ASSERT_EQ(child_copy.get_value<std::string>("key1"), "value1");
  ASSERT_EQ(child_copy.get_value<int>("key2"), 777);

  // Nested YAMLData by move
  WBE::YAMLData child_move;
  child_move.set_value("moved", "yes");
  root.set(std::move(child_move));
  auto child_moved = root.get<WBE::YAMLData>();
  ASSERT_EQ(child_moved.get_value<std::string>("moved"), "yes");
}

TEST(ParserYAMLTest, YAMLDataSetValueMethod) {
  // Test setting primitive values, arrays and nested YAMLData via set_value(key, value)
  WBE::YAMLData root;

  // Primitives
  root.set_value("fruit", std::string("apple"));
  root.set_value("number", 123);
  root.set_value("floating", 3.14);

  ASSERT_EQ(root.get_value<std::string>("fruit"), "apple");
  ASSERT_EQ(root.get_value<int>("number"), 123);
  ASSERT_DOUBLE_EQ(root.get_value<double>("floating"), 3.14);

  // Arrays
  root.set_value("numbers", std::vector<int>{10, 20, 30});
  auto nums = root.get_value<std::vector<int>>("numbers");
  ASSERT_EQ(nums.size(), 3);
  ASSERT_EQ(nums[0], 10);
  ASSERT_EQ(nums[1], 20);
  ASSERT_EQ(nums[2], 30);

  // Nested YAMLData by copy
  WBE::YAMLData child;
  child.set_value("key1", "value1");
  child.set_value("key2", 777);
  root.set_value("child_copy", child);

  auto child_copy = root.get_value<WBE::YAMLData>("child_copy");
  ASSERT_EQ(child_copy.get_value<std::string>("key1"), "value1");
  ASSERT_EQ(child_copy.get_value<int>("key2"), 777);

  // Nested YAMLData by move (rvalue)
  WBE::YAMLData child_move;
  child_move.set_value("moved", "yes");
  root.set_value("child_move", std::move(child_move));

  auto child_moved = root.get_value<WBE::YAMLData>("child_move");
  ASSERT_EQ(child_moved.get_value<std::string>("moved"), "yes");
}

#endif
