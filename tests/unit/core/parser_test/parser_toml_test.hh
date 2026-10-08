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
#ifndef WBE_FILE_PARSER_TOML_TEST_HH
#define WBE_FILE_PARSER_TOML_TEST_HH

#include "core/parser/parser_toml.hh"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "parser_test_general.hh"
#include "utils/utils.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace WBE = WhiteBirdEngine;

inline const std::string TOML_GENERAL_TEST_STR1 =
    R"(
# this is a comment
fruit = "apple" # another comment
vegetable = "lettice"
number = 123
)";

inline const std::string TOML_GENERAL_TEST_STR2 =
    R"(
vegetable = "lettice"
numbers = [123, -200, 5, 60, -123]
)";

inline const std::string TOML_GENERAL_TEST_STR2_MULTILINE =
    R"(
vegetable = "lettice"
numbers = [
    123,
    -200,
    5,
    60,
    -123,
]
)";

inline const std::string TOML_GENERAL_TEST_STR3 =
    R"(
numbers = [123, -200, 5, 60, -123]

[test_pair]
test_key1 = "test_val1"
test_key2 = 123
test_key3 = 3.14
)";

TEST(ParserTOMLTest, ParseFromBuffer) {
    WBE::ParserTOML parser;
    test_parser_general(parser, TOML_GENERAL_TEST_STR1, TOML_GENERAL_TEST_STR2, TOML_GENERAL_TEST_STR3);
    test_parser_general(parser, TOML_GENERAL_TEST_STR1, TOML_GENERAL_TEST_STR2_MULTILINE, TOML_GENERAL_TEST_STR3);
}

TEST(ParserTOMLTest, ParseFromBufferEdgeCases) {
    WBE::ParserTOML parser;
    parser.parse_from_buffer("");
    ASSERT_TRUE(parser.get_all_keys().empty());
    parser.parse_from_buffer("empty_table = {}\n");
    ASSERT_TRUE(parser.get_value<WBE::TOMLData>("empty_table").get_all_keys().empty());
    parser.parse_from_buffer("[level1.level2.level3]\nlevel4 = \"deep_value\"\n");
    ASSERT_EQ(parser.get_value<WBE::TOMLData>("level1")
                  .get_value<WBE::TOMLData>("level2")
                  .get_value<WBE::TOMLData>("level3")
                  .get_value<std::string>("level4"),
        "deep_value");
    ASSERT_FALSE(parser.contains("missing"));
    ASSERT_THROW(parser.get_value<std::string>("missing"), std::runtime_error);
}

TEST(ParserTOMLTest, DefaultDataIsEmpty) {
    WBE::TOMLData data;
    ASSERT_TRUE(data.get_all_keys().empty());
    ASSERT_FALSE(data.contains("key"));
    ASSERT_THROW(data.get<int32_t>(), std::runtime_error);
    ASSERT_THROW(data.get_value<int32_t>("key"), std::runtime_error);
}

TEST(ParserTOMLTest, ParseArrayOfTables) {
    WBE::ParserTOML parser;
    parser.parse_from_buffer(R"(
[[products]]
id = 101
name = "Laptop"
tags = ["electronics", "computers", "portable"]

[products.details]
brand = "TechCorp"
specs = { ram = "16GB", storage = "512GB SSD" }

[[products]]
id = 102
name = "Mouse"
active = false
tags = []
)");
    auto products = parser.get_value<std::vector<WBE::TOMLData>>("products");
    ASSERT_EQ(products.size(), 2);
    ASSERT_EQ(products[0].get_value<int>("id"), 101);
    ASSERT_EQ(products[0].get_value<std::string>("name"), "Laptop");
    auto details = products[0].get_value<WBE::TOMLData>("details");
    ASSERT_EQ(details.get_value<std::string>("brand"), "TechCorp");
    ASSERT_EQ(details.get_value<WBE::TOMLData>("specs").get_value<std::string>("storage"), "512GB SSD");
    auto tags = products[0].get_value<std::vector<std::string>>("tags");
    ASSERT_EQ(tags, (std::vector<std::string>{"electronics", "computers", "portable"}));
    ASSERT_EQ(products[1].get_value<int>("id"), 102);
    ASSERT_FALSE(products[1].get_value<bool>("active"));
    ASSERT_TRUE(products[1].get_value<std::vector<std::string>>("tags").empty());
    ASSERT_EQ(parser.get_all_keys(), (std::vector<std::string>{"products"}));
}

TEST(ParserTOMLTest, StrictScalarTypes) {
    WBE::ParserTOML parser;
    parser.parse_from_buffer(R"(
integer = 3
negative = -1
large = 4294967296
float_value = 2.5
text = "3"
flag = true
)");
    ASSERT_DOUBLE_EQ(parser.get_value<double>("integer"), 3.0);
    ASSERT_FLOAT_EQ(parser.get_value<float>("float_value"), 2.5F);
    ASSERT_EQ(parser.get_value<int64_t>("large"), 4294967296LL);
    ASSERT_TRUE(parser.get_value<bool>("flag"));
    ASSERT_THROW(parser.get_value<uint32_t>("negative"), std::runtime_error);
    ASSERT_THROW(parser.get_value<int32_t>("large"), std::runtime_error);
    ASSERT_THROW(parser.get_value<int32_t>("float_value"), std::runtime_error);
    ASSERT_THROW(parser.get_value<int32_t>("text"), std::runtime_error);
    ASSERT_THROW(parser.get_value<std::string>("integer"), std::runtime_error);
    ASSERT_THROW(parser.get_value<bool>("integer"), std::runtime_error);
}

TEST(ParserTOMLTest, BufferRetrieval) {
    WBE::ParserTOML parser;
    parser.parse_from_buffer(R"(
short_text = "Hello"
empty_text = ""
special_chars = "Line1\nLine2\tTab\"Quote'"
long_text = "This is a very long text that exceeds small buffer capacity"
)");
    WBE::Buffer<16> small_buffer{};
    WBE::Buffer<128> large_buffer{};
    parser.get_data().get_value("short_text", small_buffer);
    ASSERT_STREQ(small_buffer.buffer, "Hello");
    parser.get_data().get_value("empty_text", small_buffer);
    ASSERT_STREQ(small_buffer.buffer, "");
    parser.get_data().get_value("special_chars", large_buffer);
    ASSERT_STREQ(large_buffer.buffer, "Line1\nLine2\tTab\"Quote'");
    ASSERT_THROW(parser.get_data().get_value("long_text", small_buffer), std::runtime_error);
    parser.get_data().get_value("long_text", large_buffer);
    ASSERT_STREQ(large_buffer.buffer, "This is a very long text that exceeds small buffer capacity");
}

TEST(ParserTOMLTest, ParseGLMTypes) {
    WBE::ParserTOML parser;
    parser.parse_from_buffer(R"(
uv = { u = 0.25, v = 0.75 }
position2 = { x = 1, y = 2 }
position3 = { x = 1.5, y = -2, z = 3 }
color = { x = 0.1, y = 0.2, z = 0.3, w = 1.0 }
rotation = { x = 0.0, y = 0.0, z = 0.0, w = 1.0 }
missing_z = { x = 1, y = 2 }
)");
    ASSERT_EQ(parser.get_value<glm::vec2>("uv"), glm::vec2(0.25F, 0.75F));
    ASSERT_EQ(parser.get_value<glm::vec2>("position2"), glm::vec2(1.0F, 2.0F));
    ASSERT_EQ(parser.get_value<glm::vec3>("position3"), glm::vec3(1.5F, -2.0F, 3.0F));
    ASSERT_EQ(parser.get_value<glm::vec4>("color"), glm::vec4(0.1F, 0.2F, 0.3F, 1.0F));
    ASSERT_EQ(parser.get_value<glm::quat>("rotation"), glm::quat(1.0F, 0.0F, 0.0F, 0.0F));
    ASSERT_THROW(parser.get_value<glm::vec3>("missing_z"), std::runtime_error);
}

TEST(ParserTOMLTest, GLMMat4RoundTripAndValidation) {
    glm::mat4 matrix(0.0F);
    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            matrix[column][row] = static_cast<float>((column * 4) + row);
        }
    }
    WBE::TOMLData data;
    data.set_value("matrix", matrix);
    ASSERT_EQ(data.get_value<glm::mat4>("matrix"), matrix);
    WBE::ParserTOML parser;
    parser.parse_from_buffer("short = [1, 2, 3]\n");
    ASSERT_THROW(parser.get_value<glm::mat4>("short"), std::runtime_error);
}

TEST(ParserTOMLTest, SetAndSetValue) {
    WBE::TOMLData data;
    data.set_value("name", std::string("engine"));
    data.set_value("literal", "text");
    data.set_value("count", static_cast<uint32_t>(42));
    data.set_value("ratio", 0.5);
    data.set_value("enabled", true);
    data.set_value("position", glm::vec3(1.0F, 2.0F, 3.0F));
    data.set_value("ids", std::vector<int32_t>{1, 2, 3});
    WBE::Buffer<16> buffer{};
    strncpy(buffer.buffer, "buffered", sizeof(buffer.buffer));
    data.set_value("buffer", buffer);
    WBE::TOMLData child;
    child.set_value("value", 7);
    data.set_value("child", child);
    data.set_value("children", std::vector<WBE::TOMLData>{child, child});
    data.set_value("empty_child", WBE::TOMLData());

    ASSERT_EQ(data.get_value<std::string>("name"), "engine");
    ASSERT_EQ(data.get_value<std::string>("literal"), "text");
    ASSERT_EQ(data.get_value<uint32_t>("count"), 42U);
    ASSERT_DOUBLE_EQ(data.get_value<double>("ratio"), 0.5);
    ASSERT_TRUE(data.get_value<bool>("enabled"));
    ASSERT_EQ(data.get_value<glm::vec3>("position"), glm::vec3(1.0F, 2.0F, 3.0F));
    ASSERT_EQ(data.get_value<std::vector<int32_t>>("ids"), (std::vector<int32_t>{1, 2, 3}));
    ASSERT_EQ(data.get_value<std::string>("buffer"), "buffered");
    ASSERT_EQ(data.get_value<WBE::TOMLData>("child").get_value<int32_t>("value"), 7);
    ASSERT_EQ(data.get_value<std::vector<WBE::TOMLData>>("children").size(), 2);
    ASSERT_TRUE(data.get_value<WBE::TOMLData>("empty_child").get_all_keys().empty());

    // Assigning a key on a non-table value replaces it with a table.
    WBE::TOMLData scalar;
    scalar.set(5);
    ASSERT_EQ(scalar.get<int32_t>(), 5);
    scalar.set_value("key", 1);
    ASSERT_EQ(scalar.get_value<int32_t>("key"), 1);

    ASSERT_THROW(data.set_value("too_large", std::numeric_limits<uint64_t>::max()), std::runtime_error);
}

TEST(ParserTOMLTest, CopyIsDeep) {
    WBE::TOMLData original;
    original.set_value("value", 1);
    WBE::TOMLData copy = original;
    copy.set_value("value", 2);
    ASSERT_EQ(original.get_value<int32_t>("value"), 1);
    ASSERT_EQ(copy.get_value<int32_t>("value"), 2);
    WBE::TOMLData moved = std::move(copy);
    ASSERT_EQ(moved.get_value<int32_t>("value"), 2);
}

TEST(ParserTOMLTest, OutputIsReparsable) {
    WBE::TOMLData data;
    data.set_value("name", std::string("demo"));
    WBE::TOMLData child;
    child.set_value("value", 3);
    data.set_value("child", child);
    std::ostringstream stream;
    stream << data;
    WBE::ParserTOML parser;
    parser.parse_from_buffer(stream.str());
    ASSERT_EQ(parser.get_value<std::string>("name"), "demo");
    ASSERT_EQ(parser.get_value<WBE::TOMLData>("child").get_value<int32_t>("value"), 3);
}

TEST(ParserTOMLTest, MalformedInputThrows) {
    WBE::ParserTOML parser;
    ASSERT_THROW(parser.parse_from_buffer("key = \n"), std::runtime_error);
    ASSERT_THROW(parser.parse_from_buffer("key = 1\nkey = 2\n"), std::runtime_error);
    ASSERT_THROW(parser.parse(WBE::Path("test_env/does_not_exist.toml")), std::runtime_error);
}

#endif
