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
#include "core/cla/cla_token.hh"
#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(CLALexer, BasicUtilityOnly) {
    // Test with just utility name
    WBE::CLALexer lexer;
    const char* argv[] = {"test_utility"};
    auto tokens = lexer.apply(1, argv);

    ASSERT_EQ(tokens.size(), 1);
    ASSERT_EQ(tokens[0].value, "test_utility");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);
}

TEST(CLALexer, UtilityWithSingleOperand) {
    // Test utility with single operand
    WBE::CLALexer lexer;
    const char* argv[] = {"test_utility", "input.txt"};
    auto tokens = lexer.apply(2, argv);

    ASSERT_EQ(tokens.size(), 2);

    // Utility name
    ASSERT_EQ(tokens[0].value, "test_utility");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Operand
    ASSERT_EQ(tokens[1].value, "input.txt");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, ShortOptions) {
    // Test short options (-v, -h, etc.)
    WBE::CLALexer lexer;
    const char* argv[] = {"gcc", "-v", "-O", "-h"};
    auto tokens = lexer.apply(4, argv);

    ASSERT_EQ(tokens.size(), 4);

    // Utility name
    ASSERT_EQ(tokens[0].value, "gcc");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Short options
    ASSERT_EQ(tokens[1].value, "-v");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPTION_SHORT);

    ASSERT_EQ(tokens[2].value, "-O");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_SHORT);

    ASSERT_EQ(tokens[3].value, "-h");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_SHORT);
}

TEST(CLALexer, LongOptions) {
    // Test long options (--verbose, --help, etc.)
    WBE::CLALexer lexer;
    const char* argv[] = {"compiler", "--verbose", "--help", "--output"};
    auto tokens = lexer.apply(4, argv);

    ASSERT_EQ(tokens.size(), 4);

    // Utility name
    ASSERT_EQ(tokens[0].value, "compiler");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Long options
    ASSERT_EQ(tokens[1].value, "--verbose");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPTION_LONG);

    ASSERT_EQ(tokens[2].value, "--help");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_LONG);

    ASSERT_EQ(tokens[3].value, "--output");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_LONG);
}

TEST(CLALexer, MixedTokenTypes) {
    // Test mixed short options, long options, and operands
    WBE::CLALexer lexer;
    const char* argv[] = {"gcc", "source.cpp", "-O", "--output", "binary", "--verbose", "-g"};
    auto tokens = lexer.apply(7, argv);

    ASSERT_EQ(tokens.size(), 7);

    // Utility name
    ASSERT_EQ(tokens[0].value, "gcc");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Operand
    ASSERT_EQ(tokens[1].value, "source.cpp");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);

    // Short option
    ASSERT_EQ(tokens[2].value, "-O");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_SHORT);

    // Long option
    ASSERT_EQ(tokens[3].value, "--output");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_LONG);

    // Operand (argument to --output)
    ASSERT_EQ(tokens[4].value, "binary");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPERAND);

    // Long option
    ASSERT_EQ(tokens[5].value, "--verbose");
    ASSERT_EQ(tokens[5].type, WBE::CLAToken::Type::OPTION_LONG);

    // Short option
    ASSERT_EQ(tokens[6].value, "-g");
    ASSERT_EQ(tokens[6].type, WBE::CLAToken::Type::OPTION_SHORT);
}

TEST(CLALexer, MultipleOperands) {
    // Test multiple operands (multiple input files)
    WBE::CLALexer lexer;
    const char* argv[] = {"cat", "file1.txt", "file2.txt", "file3.txt"};
    auto tokens = lexer.apply(4, argv);

    ASSERT_EQ(tokens.size(), 4);

    // Utility name
    ASSERT_EQ(tokens[0].value, "cat");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Multiple operands
    ASSERT_EQ(tokens[1].value, "file1.txt");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);

    ASSERT_EQ(tokens[2].value, "file2.txt");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPERAND);

    ASSERT_EQ(tokens[3].value, "file3.txt");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, EdgeCaseTokens) {
    // Test edge cases like single dash, double dash alone, etc.
    WBE::CLALexer lexer;
    const char* argv[] = {"test", "-", "--", "normal_arg"};
    auto tokens = lexer.apply(4, argv);

    ASSERT_EQ(tokens.size(), 4);

    // Utility name
    ASSERT_EQ(tokens[0].value, "test");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Single dash (should be short option)
    ASSERT_EQ(tokens[1].value, "-");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPTION_SHORT);

    // Double dash alone (should be long option)
    ASSERT_EQ(tokens[2].value, "--");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_LONG);

    // Normal argument
    ASSERT_EQ(tokens[3].value, "normal_arg");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, ComplexRealWorldExample) {
    // Test a complex realistic compiler command
    WBE::CLALexer lexer;
    const char* argv[] = {
        "g++",
        "main.cpp",
        "utils.cpp",
        "-std=c++17",
        "--optimize",
        "-O3",
        "--output",
        "program",
        "-I/usr/include",
        "--debug",
        "-g"
    };
    auto tokens = lexer.apply(11, argv);

    ASSERT_EQ(tokens.size(), 11);

    // Utility
    ASSERT_EQ(tokens[0].value, "g++");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Source files (operands)
    ASSERT_EQ(tokens[1].value, "main.cpp");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);

    ASSERT_EQ(tokens[2].value, "utils.cpp");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPERAND);

    // Short option with value
    ASSERT_EQ(tokens[3].value, "-std=c++17");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPTION_SHORT);

    // Long option
    ASSERT_EQ(tokens[4].value, "--optimize");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPTION_LONG);

    // Short option
    ASSERT_EQ(tokens[5].value, "-O3");
    ASSERT_EQ(tokens[5].type, WBE::CLAToken::Type::OPTION_SHORT);

    // Long option
    ASSERT_EQ(tokens[6].value, "--output");
    ASSERT_EQ(tokens[6].type, WBE::CLAToken::Type::OPTION_LONG);

    // Output file name (operand)
    ASSERT_EQ(tokens[7].value, "program");
    ASSERT_EQ(tokens[7].type, WBE::CLAToken::Type::OPERAND);

    // Include path (short option)
    ASSERT_EQ(tokens[8].value, "-I/usr/include");
    ASSERT_EQ(tokens[8].type, WBE::CLAToken::Type::OPTION_SHORT);

    // Debug long option
    ASSERT_EQ(tokens[9].value, "--debug");
    ASSERT_EQ(tokens[9].type, WBE::CLAToken::Type::OPTION_LONG);

    // Debug short option
    ASSERT_EQ(tokens[10].value, "-g");
    ASSERT_EQ(tokens[10].type, WBE::CLAToken::Type::OPTION_SHORT);
}

TEST(CLALexer, PathsAndSpecialCharacters) {
    // Test paths and arguments with special characters
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
    auto tokens = lexer.apply(7, argv);

    ASSERT_EQ(tokens.size(), 7);

    // Utility
    ASSERT_EQ(tokens[0].value, "myapp");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Path operand
    ASSERT_EQ(tokens[1].value, "/path/to/file.txt");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);

    // Long option
    ASSERT_EQ(tokens[2].value, "--config");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPTION_LONG);

    // Config path operand
    ASSERT_EQ(tokens[3].value, "/etc/myapp/config.json");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);

    // Short option
    ASSERT_EQ(tokens[4].value, "-D");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPTION_SHORT);

    // Define value operand
    ASSERT_EQ(tokens[5].value, "VERSION=1.0.0");
    ASSERT_EQ(tokens[5].type, WBE::CLAToken::Type::OPERAND);

    // File with underscores
    ASSERT_EQ(tokens[6].value, "file_with_underscores.txt");
    ASSERT_EQ(tokens[6].type, WBE::CLAToken::Type::OPERAND);
}

TEST(CLALexer, EmptyAndSpecialValues) {
    // Test with empty strings and special values
    WBE::CLALexer lexer;
    const char* argv[] = {"utility", "", "123", "true", "false"};
    auto tokens = lexer.apply(5, argv);

    ASSERT_EQ(tokens.size(), 5);

    // Utility
    ASSERT_EQ(tokens[0].value, "utility");
    ASSERT_EQ(tokens[0].type, WBE::CLAToken::Type::UTILITY_NAME);

    // Empty string operand
    ASSERT_EQ(tokens[1].value, "");
    ASSERT_EQ(tokens[1].type, WBE::CLAToken::Type::OPERAND);

    // Numeric operand
    ASSERT_EQ(tokens[2].value, "123");
    ASSERT_EQ(tokens[2].type, WBE::CLAToken::Type::OPERAND);

    // Boolean-like operands
    ASSERT_EQ(tokens[3].value, "true");
    ASSERT_EQ(tokens[3].type, WBE::CLAToken::Type::OPERAND);

    ASSERT_EQ(tokens[4].value, "false");
    ASSERT_EQ(tokens[4].type, WBE::CLAToken::Type::OPERAND);
}

#endif
