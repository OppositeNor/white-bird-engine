#ifndef __WBE_PARSER_JSON_TEST_H__
#define __WBE_PARSER_JSON_TEST_H__

#include <cstdint>
#include <gtest/gtest.h>
#include "core/parser/parser_json.h"

namespace WBE = WhiteBirdEngine;

TEST(ParserJSONTest, ParseFromBuffer) {
    WBE::ParserJSON parser;
    parser.parse_from_buffer(R"({ "fruit" : "apple", "vegetable" : "lettice", "number" : 123 })");
    ASSERT_EQ(parser.get_value<std::string>("fruit"), static_cast<std::string>("apple"));
    ASSERT_EQ(parser.get_value<std::string>("vegetable"), static_cast<std::string>("lettice"));
    ASSERT_EQ(parser.get_value<int32_t>("number"), static_cast<int32_t>(123));

    parser.parse_from_buffer("{ \"vegetable\" : \"lettice\", \"numbers\" : [123, -200, 5, 60, -123] }");
    ASSERT_EQ(parser.get_value<std::string>("vegetable"), static_cast<std::string>("lettice"));
    ASSERT_EQ(parser.get_value<std::vector<int>>("numbers")[0], static_cast<int32_t>(123));
    ASSERT_EQ(parser.get_value<std::vector<int>>("numbers")[1], static_cast<int32_t>(-200));
    ASSERT_EQ(parser.get_value<std::vector<int>>("numbers")[2], static_cast<int32_t>(5));
    ASSERT_EQ(parser.get_value<std::vector<int>>("numbers")[3], static_cast<int32_t>(60));
    ASSERT_EQ(parser.get_value<std::vector<int>>("numbers")[4], static_cast<int32_t>(-123));
}

#endif
