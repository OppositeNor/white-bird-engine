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
#ifndef __WBE_CLA_AST_VISITOR_TEST_HH__
#define __WBE_CLA_AST_VISITOR_TEST_HH__

#include "core/cla/cla_ast.hh"
#include "core/cla/cla_ast_visitor.hh"
#include "core/memory/reference_strong.hh"
#include "global/global.hh"
#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace WBE = WhiteBirdEngine;

TEST(CLAASTVisitorToString, EmptyUtility) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "test_utility", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: test_utility\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, UtilityWithSingleOperand) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "input_file"));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "test_utility", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: test_utility\n"
        "operations:\n"
        "  - operand: input_file\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, UtilityWithSingleOperation) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "help", false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "test_utility", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: test_utility\n"
        "operations:\n"
        "  - operation: help\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, UtilityWithOperationAndArguments) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<std::string> arguments = {"output.txt", "verbose"};
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "output", std::move(arguments), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "test_utility", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: test_utility\n"
        "operations:\n"
        "  - operation: output\n"
        "    arguments:\n"
        "      - output.txt\n"
        "      - verbose\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, UtilityWithMultipleOperations) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;

    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "input.txt"));
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "verbose", false));
    std::vector<std::string> arguments = {"json", "2"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "format", std::move(arguments), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "converter", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: converter\n"
        "operations:\n"
        "  - operand: input.txt\n"
        "  - operation: verbose\n"
        "  - operation: format\n"
        "    arguments:\n"
        "      - json\n"
        "      - 2\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, OperationWithSingleArgument) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<std::string> arguments = {"debug"};
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "level", std::move(arguments), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "logger", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: logger\n"
        "operations:\n"
        "  - operation: level\n"
        "    arguments:\n"
        "      - debug\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, ComplexUtilityExample) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;

    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "source.cpp"));
    std::vector<std::string> output_args = {"compiled.exe"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "output", std::move(output_args), false));
    std::vector<std::string> opt_args = {"3"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "O", std::move(opt_args), true));
    std::vector<std::string> include_args = {"/usr/include", "/opt/include"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "include", std::move(include_args), false));
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "debug", false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "gcc", std::move(operations));
    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);
    std::string expected = "utility: gcc\n"
        "operations:\n"
        "  - operand: source.cpp\n"
        "  - operation: output\n"
        "    arguments:\n"
        "      - compiled.exe\n"
        "  - operation: O\n"
        "    arguments:\n"
        "      - 3\n"
        "  - operation: include\n"
        "    arguments:\n"
        "      - /usr/include\n"
        "      - /opt/include\n"
        "  - operation: debug\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, MultipleOperands) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "file1.txt"));
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "file2.txt"));
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "file3.txt"));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "concat", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: concat\n"
        "operations:\n"
        "  - operand: file1.txt\n"
        "  - operand: file2.txt\n"
        "  - operand: file3.txt\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitorToString, OperationArgumentManipulation) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    auto operation = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "include", false);
    operation->push_argument("/usr/local/include");
    operation->push_argument("/opt/local/include");

    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(operation);

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "compiler", std::move(operations));

    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);

    std::string expected = "utility: compiler\n"
        "operations:\n"
        "  - operation: include\n"
        "    arguments:\n"
        "      - /usr/local/include\n"
        "      - /opt/local/include\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitor, MultipleShortOptionsYAMLFormat) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    auto op_a = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "a", true);
    auto op_b = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "b", true);
    auto op_c = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "c", true);
    
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(op_a);
    operations.push_back(op_b);
    operations.push_back(op_c);
    
    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "tool", std::move(operations));
    
    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);
    
    std::string expected = "utility: tool\n"
        "operations:\n"
        "  - operation: a\n"
        "  - operation: b\n"
        "  - operation: c\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

TEST(CLAASTVisitor, MixedSingleAndMultipleShortOptionsYAMLFormat) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    auto op_f = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "-f", true);
    op_f->push_argument("input.txt");
    
    auto op_v = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "v", true);
    auto op_d = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "d", true);
    
    auto op_o = WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "-o", true);
    op_o->push_argument("output.txt");
    
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(op_f);
    operations.push_back(op_v);
    operations.push_back(op_d);
    operations.push_back(op_o);
    
    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "tool", std::move(operations));
    
    WBE::CLAASTVisitorToString visitor;
    root->accept(&visitor);
    
    std::string expected = "utility: tool\n"
        "operations:\n"
        "  - operation: -f\n"
        "    arguments:\n"
        "      - input.txt\n"
        "  - operation: v\n"
        "  - operation: d\n"
        "  - operation: -o\n"
        "    arguments:\n"
        "      - output.txt\n";
    ASSERT_EQ(visitor.get_string(), expected);
}

#endif
