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
#ifndef __WBE_CLA_LEXER_TEST_HH__
#define __WBE_CLA_LEXER_TEST_HH__

#include "core/cla/cla_lexer.hh"
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace WBE = WhiteBirdEngine;

TEST(CLALexer, BasicUtilityOnly) {
    WBE::CLALexer lexer;
    const char* argv[] = {"test_utility"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 1);
    ASSERT_EQ(tokens[0].value, "test_utility");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
}

TEST(CLALexer, UtilityWithSingleOperand) {
    WBE::CLALexer lexer;
    const char* argv[] = {"test_utility", "input.txt"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 2);
    ASSERT_EQ(tokens[0].value, "test_utility");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "input.txt");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, ShortOptions) {
    WBE::CLALexer lexer;
    const char* argv[] = {"gcc", "-v", "-O", "-h"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 4);
    ASSERT_EQ(tokens[0].value, "gcc");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "-v");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPTION_SHORT);
    ASSERT_EQ(tokens[2].value, "-O");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_SHORT);
    ASSERT_EQ(tokens[3].value, "-h");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_SHORT);
}

TEST(CLALexer, LongOptions) {
    WBE::CLALexer lexer;
    const char* argv[] = {"compiler", "--verbose", "--help", "--output"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 4);
    ASSERT_EQ(tokens[0].value, "compiler");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "--verbose");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[2].value, "--help");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[3].value, "--output");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_LONG);
}

TEST(CLALexer, MixedTokenTypes) {
    WBE::CLALexer lexer;
    const char* argv[] = {"gcc", "source.cpp", "-O", "--output", "binary", "--verbose", "-g"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 7);
    ASSERT_EQ(tokens[0].value, "gcc");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "source.cpp");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[2].value, "-O");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_SHORT);
    ASSERT_EQ(tokens[3].value, "--output");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[4].value, "binary");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[5].value, "--verbose");
    ASSERT_EQ(tokens[5].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[6].value, "-g");
    ASSERT_EQ(tokens[6].type, WBE::CLAToken::Type::OPTION_SHORT);
}

TEST(CLALexer, MultipleOperands) {
    WBE::CLALexer lexer;
    const char* argv[] = {"cat", "file1.txt", "file2.txt", "file3.txt"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 4);
    ASSERT_EQ(tokens[0].value, "cat");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "file1.txt");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[2].value, "file2.txt");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[3].value, "file3.txt");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, EdgeCaseTokens) {
    WBE::CLALexer lexer;
    const char* argv[] = {"test", "-", "--", "normal_arg"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), argc);
    ASSERT_EQ(tokens[0].value, "test");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "-");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPTION_SHORT);
    ASSERT_EQ(tokens[2].value, "--");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[3].value, "normal_arg");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, ComplexRealWorldExample) {
    WBE::CLALexer lexer;
    const char* argv[] = {
        "g++",
        "main.cpp",
        "utils.cpp",
        "--optimize",
        "--output",
        "program",
        "--debug",
        "-g"
    };
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), argc);
    ASSERT_EQ(tokens[0].value, "g++");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "main.cpp");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[2].value, "utils.cpp");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[3].value, "--optimize");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[4].value, "--output");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[5].value, "program");
    ASSERT_EQ(tokens[5].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[6].value, "--debug");
    ASSERT_EQ(tokens[6].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[7].value, "-g");
    ASSERT_EQ(tokens[7].type, WBE::CLAToken::Type::OPTION_SHORT);
}

TEST(CLALexer, PathsAndSpecialCharacters) {
    WBE::CLALexer lexer;
    const char* argv[] = {
        "myapp",
        "/path/to/file.txt",
        "--config",
        "/etc/myapp/config.json",
        "-D",
        "VERSION=1.0.0",
        "file_with_underscores.txt"
    };
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 7);
    ASSERT_EQ(tokens[0].value, "myapp");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "/path/to/file.txt");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[2].value, "--config");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_LONG);
    ASSERT_EQ(tokens[3].value, "/etc/myapp/config.json");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[4].value, "-D");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPTION_SHORT);
    ASSERT_EQ(tokens[5].value, "VERSION=1.0.0");
    ASSERT_EQ(tokens[5].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[6].value, "file_with_underscores.txt");
    ASSERT_EQ(tokens[6].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, EmptyAndSpecialValues) {
    WBE::CLALexer lexer;
    const char* argv[] = {"utility", "", "123", "true", "false"};
    uint32_t argc = sizeof(argv) / sizeof(const char*);
    auto tokens = lexer.apply(argc, argv);

    ASSERT_EQ(tokens.size(), 5);
    ASSERT_EQ(tokens[0].value, "utility");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
    ASSERT_EQ(tokens[1].value, "");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[2].value, "123");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[3].value, "true");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);
    ASSERT_EQ(tokens[4].value, "false");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPERAND);
}

#endif
