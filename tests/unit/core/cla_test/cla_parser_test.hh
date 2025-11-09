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
#ifndef __WBE_CLA_PARSER_TEST_HH__
#define __WBE_CLA_PARSER_TEST_HH__

#include "core/cla/cla_parser.hh"
#include "core/cla/cla_ast.hh"
#include "core/cla/cla_ast_visitor.hh"
#include "global/global.hh"
#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(CLAParser, BasicUtilityOnly) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {"test_utility", WBE::CLAToken::Type::UTILITY_NAME}
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
        {"test_utility", WBE::CLAToken::Type::UTILITY_NAME},
        {"input.txt", WBE::CLAToken::Type::OPERAND}
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
        {"test_utility", WBE::CLAToken::Type::UTILITY_NAME},
        {"--help", WBE::CLAToken::Type::OPTION_LONG}
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
        {"test_utility", WBE::CLAToken::Type::UTILITY_NAME},
        {"-v", WBE::CLAToken::Type::OPTION_SHORT}
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
        {"compiler", WBE::CLAToken::Type::UTILITY_NAME},
        {"--output", WBE::CLAToken::Type::OPTION_LONG},
        {"program.exe", WBE::CLAToken::Type::OPERAND}
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
        {"logger", WBE::CLAToken::Type::UTILITY_NAME},
        {"-l", WBE::CLAToken::Type::OPTION_SHORT},
        {"debug", WBE::CLAToken::Type::OPERAND}
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
        {"compiler", WBE::CLAToken::Type::UTILITY_NAME},
        {"--include", WBE::CLAToken::Type::OPTION_LONG},
        {"/usr/include", WBE::CLAToken::Type::OPERAND},
        {"/opt/include", WBE::CLAToken::Type::OPERAND}
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
        {"gcc", WBE::CLAToken::Type::UTILITY_NAME},
        {"main.cpp", WBE::CLAToken::Type::OPERAND},
        {"--output", WBE::CLAToken::Type::OPTION_LONG},
        {"program", WBE::CLAToken::Type::OPERAND},
        {"-O", WBE::CLAToken::Type::OPTION_SHORT},
        {"3", WBE::CLAToken::Type::OPERAND},
        {"--verbose", WBE::CLAToken::Type::OPTION_LONG}
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
        {"processor", WBE::CLAToken::Type::UTILITY_NAME},
        {"file1.txt", WBE::CLAToken::Type::OPERAND},
        {"file2.txt", WBE::CLAToken::Type::OPERAND},
        {"--format", WBE::CLAToken::Type::OPTION_LONG},
        {"json", WBE::CLAToken::Type::OPERAND},
        {"-v", WBE::CLAToken::Type::OPTION_SHORT}
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
        {"g++", WBE::CLAToken::Type::UTILITY_NAME},
        {"main.cpp", WBE::CLAToken::Type::OPERAND},
        {"utils.cpp", WBE::CLAToken::Type::OPERAND},
        {"--std", WBE::CLAToken::Type::OPTION_LONG},
        {"c++17", WBE::CLAToken::Type::OPERAND},
        {"-O", WBE::CLAToken::Type::OPTION_SHORT},
        {"3", WBE::CLAToken::Type::OPERAND},
        {"--output", WBE::CLAToken::Type::OPTION_LONG},
        {"program", WBE::CLAToken::Type::OPERAND},
        {"--include", WBE::CLAToken::Type::OPTION_LONG},
        {"/usr/include", WBE::CLAToken::Type::OPERAND},
        {"/opt/include", WBE::CLAToken::Type::OPERAND},
        {"--debug", WBE::CLAToken::Type::OPTION_LONG}
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
        {"utility", WBE::CLAToken::Type::UTILITY_NAME},
        {"--verbose", WBE::CLAToken::Type::OPTION_LONG},
        {"--debug", WBE::CLAToken::Type::OPTION_LONG},
        {"-h", WBE::CLAToken::Type::OPTION_SHORT},
        {"--help", WBE::CLAToken::Type::OPTION_LONG}
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
        {"--invalid", WBE::CLAToken::Type::OPTION_LONG}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    ASSERT_THROW(parser->parse(tokens), std::runtime_error);
}

TEST(CLAParser, ErrorInvalidTokenInRootOperand) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {"utility", WBE::CLAToken::Type::UTILITY_NAME},
        {"invalid_utility", WBE::CLAToken::Type::UTILITY_NAME}
    };

    auto parser = WBE::make_ref<WBE::CLAParser>(WBE::global_allocator());
    ASSERT_THROW(parser->parse(tokens), std::runtime_error);
}

TEST(CLAParser, OptionsWithNoArgumentsFollowedByMoreOptions) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));

    std::vector<WBE::CLAToken> tokens = {
        {"test", WBE::CLAToken::Type::UTILITY_NAME},
        {"-v", WBE::CLAToken::Type::OPTION_SHORT},
        {"--debug", WBE::CLAToken::Type::OPTION_LONG},
        {"--output", WBE::CLAToken::Type::OPTION_LONG},
        {"file.txt", WBE::CLAToken::Type::OPERAND}
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
        {"cat", WBE::CLAToken::Type::UTILITY_NAME},
        {"file.txt", WBE::CLAToken::Type::OPERAND}
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
        {"concat", WBE::CLAToken::Type::UTILITY_NAME},
        {"file1.txt", WBE::CLAToken::Type::OPERAND},
        {"file2.txt", WBE::CLAToken::Type::OPERAND},
        {"file3.txt", WBE::CLAToken::Type::OPERAND}
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
        {"tool", WBE::CLAToken::Type::UTILITY_NAME},
        {"-abc", WBE::CLAToken::Type::OPTION_SHORT}
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
        {"tool", WBE::CLAToken::Type::UTILITY_NAME},
        {"-o", WBE::CLAToken::Type::OPTION_SHORT},
        {"output.txt", WBE::CLAToken::Type::OPERAND}
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
        {"compiler", WBE::CLAToken::Type::UTILITY_NAME},
        {"-gO", WBE::CLAToken::Type::OPTION_SHORT},
        {"--std", WBE::CLAToken::Type::OPTION_LONG},
        {"c++17", WBE::CLAToken::Type::OPERAND}
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
        {"tool", WBE::CLAToken::Type::UTILITY_NAME},
        {"-f", WBE::CLAToken::Type::OPTION_SHORT},
        {"file.txt", WBE::CLAToken::Type::OPERAND},
        {"-vdx", WBE::CLAToken::Type::OPTION_SHORT},
        {"-o", WBE::CLAToken::Type::OPTION_SHORT},
        {"output.txt", WBE::CLAToken::Type::OPERAND}
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
        {"ls", WBE::CLAToken::Type::UTILITY_NAME},
        {"-la", WBE::CLAToken::Type::OPTION_SHORT},
        {"file1.txt", WBE::CLAToken::Type::OPERAND},
        {"file2.txt", WBE::CLAToken::Type::OPERAND}
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
