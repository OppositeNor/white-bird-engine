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

#endif
