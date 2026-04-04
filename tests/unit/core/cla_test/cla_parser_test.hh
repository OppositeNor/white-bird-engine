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
#ifndef WBE_FILE_CLA_PARSER_TEST_HH
#define WBE_FILE_CLA_PARSER_TEST_HH

#include "core/cla/cla_parser.hh"
#include "core/cla/cla_ast.hh"
#include "core/cla/cla_ast_visitor.hh"
#include "core/cla/cla_utils.hh"
#include "core/memory/reference_strong.hh"
#include "core/engine_core.hh"
#include "global/global.hh"
#include "platform/file_system/directory.hh"
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <vector>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(CLAParser, BasicUtilityOnly) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="test_utility", .type=WBE::CLAToken::Type::UTILITY_NAME}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: test_utility\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, UtilityWithSingleRootOperand) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="test_utility", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="input.txt", .type=WBE::CLAToken::Type::OPERAND}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: test_utility\n"
        "operations:\n"
        "  - operand: input.txt\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, UtilityWithLongOptionOnly) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="test_utility", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="--help", .type=WBE::CLAToken::Type::OPTION_LONG}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: test_utility\n"
        "operations:\n"
        "  - operation: help\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, UtilityWithShortOptionOnly) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="test_utility", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-v", .type=WBE::CLAToken::Type::OPTION_SHORT}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: test_utility\n"
        "operations:\n"
        "  - operation: -v\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, LongOptionWithArgument) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="compiler", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="--output", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="program.exe", .type=WBE::CLAToken::Type::OPERAND}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: compiler\n"
        "operations:\n"
        "  - operation: output\n"
        "    arguments:\n"
        "      - program.exe\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, ShortOptionWithArgument) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="logger", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-l", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="debug", .type=WBE::CLAToken::Type::OPERAND}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: logger\n"
        "operations:\n"
        "  - operation: -l\n"
        "    arguments:\n"
        "      - debug\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, OptionWithMultipleArguments) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="compiler", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="--include", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="/usr/include", .type=WBE::CLAToken::Type::OPERAND},
        {.value="/opt/include", .type=WBE::CLAToken::Type::OPERAND}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: compiler\n"
        "operations:\n"
        "  - operation: include\n"
        "    arguments:\n"
        "      - /usr/include\n"
        "      - /opt/include\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, MultipleOperationsAndOperands) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="gcc", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="main.cpp", .type=WBE::CLAToken::Type::OPERAND},
        {.value="--output", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="program", .type=WBE::CLAToken::Type::OPERAND},
        {.value="-O", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="3", .type=WBE::CLAToken::Type::OPERAND},
        {.value="--verbose", .type=WBE::CLAToken::Type::OPTION_LONG}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: gcc\n"
        "operations:\n"
        "  - operand: main.cpp\n"
        "  - operation: output\n"
        "    arguments:\n"
        "      - program\n"
        "  - operation: -O\n"
        "    arguments:\n"
        "      - 3\n"
        "  - operation: verbose\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, MixedRootOperandsAndOptions) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="processor", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="file1.txt", .type=WBE::CLAToken::Type::OPERAND},
        {.value="file2.txt", .type=WBE::CLAToken::Type::OPERAND},
        {.value="--format", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="json", .type=WBE::CLAToken::Type::OPERAND},
        {.value="-v", .type=WBE::CLAToken::Type::OPTION_SHORT}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: processor\n"
        "operations:\n"
        "  - operand: file1.txt\n"
        "  - operand: file2.txt\n"
        "  - operation: format\n"
        "    arguments:\n"
        "      - json\n"
        "  - operation: -v\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, ComplexRealWorldExample) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="g++", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="main.cpp", .type=WBE::CLAToken::Type::OPERAND},
        {.value="utils.cpp", .type=WBE::CLAToken::Type::OPERAND},
        {.value="--std", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="c++17", .type=WBE::CLAToken::Type::OPERAND},
        {.value="-O", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="3", .type=WBE::CLAToken::Type::OPERAND},
        {.value="--output", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="program", .type=WBE::CLAToken::Type::OPERAND},
        {.value="--include", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="/usr/include", .type=WBE::CLAToken::Type::OPERAND},
        {.value="/opt/include", .type=WBE::CLAToken::Type::OPERAND},
        {.value="--debug", .type=WBE::CLAToken::Type::OPTION_LONG}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: g++\n"
        "operations:\n"
        "  - operand: main.cpp\n"
        "  - operand: utils.cpp\n"
        "  - operation: std\n"
        "    arguments:\n"
        "      - c++17\n"
        "  - operation: -O\n"
        "    arguments:\n"
        "      - 3\n"
        "  - operation: output\n"
        "    arguments:\n"
        "      - program\n"
        "  - operation: include\n"
        "    arguments:\n"
        "      - /usr/include\n"
        "      - /opt/include\n"
        "  - operation: debug\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, ConsecutiveOptions) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="utility", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="--verbose", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="--debug", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="-h", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="--help", .type=WBE::CLAToken::Type::OPTION_LONG}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: utility\n"
        "operations:\n"
        "  - operation: verbose\n"
        "  - operation: debug\n"
        "  - operation: -h\n"
        "  - operation: help\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, ErrorInvalidFirstToken) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="--invalid", .type=WBE::CLAToken::Type::OPTION_LONG}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    ASSERT_THROW(parser->parse(tokens), std::runtime_error);
}

TEST(CLAParser, ErrorInvalidTokenInRootOperand) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="utility", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="invalid_utility", .type=WBE::CLAToken::Type::UTILITY_NAME}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    ASSERT_THROW(parser->parse(tokens), std::runtime_error);
}

TEST(CLAParser, OptionsWithNoArgumentsFollowedByMoreOptions) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="test", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-v", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="--debug", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="--output", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="file.txt", .type=WBE::CLAToken::Type::OPERAND}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: test\n"
        "operations:\n"
        "  - operation: -v\n"
        "  - operation: debug\n"
        "  - operation: output\n"
        "    arguments:\n"
        "      - file.txt\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, SingleRootOperandOnly) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="cat", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="file.txt", .type=WBE::CLAToken::Type::OPERAND}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: cat\n"
        "operations:\n"
        "  - operand: file.txt\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, MultipleRootOperands) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {.value="concat", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="file1.txt", .type=WBE::CLAToken::Type::OPERAND},
        {.value="file2.txt", .type=WBE::CLAToken::Type::OPERAND},
        {.value="file3.txt", .type=WBE::CLAToken::Type::OPERAND}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);

    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();

    std::string expected = "utility: concat\n"
        "operations:\n"
        "  - operand: file1.txt\n"
        "  - operand: file2.txt\n"
        "  - operand: file3.txt\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, MultipleShortOptionsInOnePrefix) {
    // Test multiple short options in one '-' prefix (like -abc)
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    std::vector<WBE::CLAToken> tokens = {
        {.value="tool", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-abc", .type=WBE::CLAToken::Type::OPTION_SHORT}
    };
    
    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);
    
    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();
    
    std::string expected = "utility: tool\n"
                          "operations:\n"
                          "  - operation: a\n"
                          "  - operation: b\n"
                          "  - operation: c\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, SingleShortOptionWithArgument) {
    // Test single short option can take arguments
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    std::vector<WBE::CLAToken> tokens = {
        {.value="tool", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-o", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="output.txt", .type=WBE::CLAToken::Type::OPERAND}
    };
    
    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);
    
    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();
    
    std::string expected = "utility: tool\n"
                          "operations:\n"
                          "  - operation: -o\n"
                          "    arguments:\n"
                          "      - output.txt\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, MultipleShortOptionsFollowedByLongOption) {
    // Test multiple short options followed by long option with argument
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    std::vector<WBE::CLAToken> tokens = {
        {.value="compiler", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-gO", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="--std", .type=WBE::CLAToken::Type::OPTION_LONG},
        {.value="c++17", .type=WBE::CLAToken::Type::OPERAND}
    };
    
    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);
    
    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();
    
    std::string expected = "utility: compiler\n"
                          "operations:\n"
                          "  - operation: g\n"
                          "  - operation: O\n"
                          "  - operation: std\n"
                          "    arguments:\n"
                          "      - c++17\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, MixedSingleAndMultipleShortOptions) {
    // Test mixing single short options (with args) and multiple short options (no args)
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    std::vector<WBE::CLAToken> tokens = {
        {.value="tool", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-f", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="file.txt", .type=WBE::CLAToken::Type::OPERAND},
        {.value="-vdx", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="-o", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="output.txt", .type=WBE::CLAToken::Type::OPERAND}
    };
    
    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);
    
    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();
    
    std::string expected = "utility: tool\n"
                          "operations:\n"
                          "  - operation: -f\n"
                          "    arguments:\n"
                          "      - file.txt\n"
                          "  - operation: v\n"
                          "  - operation: d\n"
                          "  - operation: x\n"
                          "  - operation: -o\n"
                          "    arguments:\n"
                          "      - output.txt\n";
    ASSERT_EQ(result, expected);
}

TEST(CLAParser, MultipleShortOptionsWithOperands) {
    // Test multiple short options followed by root operands
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    std::vector<WBE::CLAToken> tokens = {
        {.value="ls", .type=WBE::CLAToken::Type::UTILITY_NAME},
        {.value="-la", .type=WBE::CLAToken::Type::OPTION_SHORT},
        {.value="file1.txt", .type=WBE::CLAToken::Type::OPERAND},
        {.value="file2.txt", .type=WBE::CLAToken::Type::OPERAND}
    };
    
    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    auto ast = parser->parse(tokens);
    
    WBE::CLAASTVisitorToString visitor;
    ast->accept(&visitor);
    std::string result = visitor.get_string();
    
    std::string expected = "utility: ls\n"
                          "operations:\n"
                          "  - operation: l\n"
                          "  - operation: a\n"
                          "  - operand: file1.txt\n"
                          "  - operand: file2.txt\n";
    ASSERT_EQ(result, expected);
}

#endif
