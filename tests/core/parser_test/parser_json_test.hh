// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
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

#endif
