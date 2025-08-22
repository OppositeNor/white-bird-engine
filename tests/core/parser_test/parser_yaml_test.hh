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


#endif
