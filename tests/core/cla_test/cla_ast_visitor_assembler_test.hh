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
#ifndef __WBE_CLA_AST_VISITOR_ASSEMBLER_TEST_HH__
#define __WBE_CLA_AST_VISITOR_ASSEMBLER_TEST_HH__

#include "core/cla/cla_ast.hh"
#include "core/cla/cla_ast_visitor.hh"
#include "core/memory/reference_strong.hh"
#include "global/global.hh"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <stdexcept>

namespace WBE = WhiteBirdEngine;

TEST(CLAASTVisitorAssembler, EmptyUtility) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "test_utility", std::move(operations));

    WBE::CLAASTVisitorAssembler assembler;
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "test_utility");
    ASSERT_TRUE(result.operations.empty());
    ASSERT_TRUE(result.operands.empty());
}

TEST(CLAASTVisitorAssembler, OnlyOperands) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "file1.txt"));
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "file2.txt"));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "copy", std::move(operations));

    WBE::CLAASTVisitorAssembler assembler;
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "copy");
    ASSERT_TRUE(result.operations.empty());
    ASSERT_EQ(result.operands.size(), 2);
    ASSERT_EQ(result.operands[0], "file1.txt");
    ASSERT_EQ(result.operands[1], "file2.txt");
}

TEST(CLAASTVisitorAssembler, OperationWithExactArguments) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("output", 1);

    std::vector<std::string> arguments = {"result.txt"};
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "output", std::move(arguments), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "compiler", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "compiler");
    ASSERT_EQ(result.operations.size(), 1);
    ASSERT_EQ(result.operations[0].operation_name, "output");
    ASSERT_EQ(result.operations[0].arguments.size(), 1);
    ASSERT_EQ(result.operations[0].arguments[0], "result.txt");
    ASSERT_TRUE(result.operands.empty());
}

TEST(CLAASTVisitorAssembler, OperationWithExcessArguments) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("format", 1);

    std::vector<std::string> arguments = {"json", "extra_file1.txt", "extra_file2.txt"};
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "format", std::move(arguments), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "converter", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "converter");
    ASSERT_EQ(result.operations.size(), 1);
    ASSERT_EQ(result.operations[0].operation_name, "format");
    ASSERT_EQ(result.operations[0].arguments.size(), 1);
    ASSERT_EQ(result.operations[0].arguments[0], "json");
    
    ASSERT_EQ(result.operands.size(), 2);
    ASSERT_EQ(result.operands[0], "extra_file1.txt");
    ASSERT_EQ(result.operands[1], "extra_file2.txt");
}

TEST(CLAASTVisitorAssembler, OperationWithZeroArguments) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("help", 0);

    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "help", false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "tool", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "tool");
    ASSERT_EQ(result.operations.size(), 1);
    ASSERT_EQ(result.operations[0].operation_name, "help");
    ASSERT_TRUE(result.operations[0].arguments.empty());
    ASSERT_TRUE(result.operands.empty());
}

TEST(CLAASTVisitorAssembler, OperationWithInsufficientArguments) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("include", 2);

    std::vector<std::string> arguments = {"single_arg"};
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "include", std::move(arguments), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "compiler", std::move(operations));
    
    ASSERT_THROW({
        root->accept(&assembler);
    }, std::runtime_error);
}

TEST(CLAASTVisitorAssembler, ShortOptionNames) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("output", 'o', 1);

    std::vector<std::string> arguments = {"result.out"};
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "o", std::move(arguments), true));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "tool", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "tool");
    ASSERT_EQ(result.operations.size(), 1);
    ASSERT_EQ(result.operations[0].operation_name, "output");
    ASSERT_EQ(result.operations[0].arguments.size(), 1);
    ASSERT_EQ(result.operations[0].arguments[0], "result.out");
    ASSERT_TRUE(result.operands.empty());
}

TEST(CLAASTVisitorAssembler, MultipleOperations) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("verbose", 0);
    assembler.register_option("output", 1);
    assembler.register_option("format", 'f', 1);

    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "verbose", false));
    
    std::vector<std::string> output_args = {"final.out"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "output", std::move(output_args), false));
    
    std::vector<std::string> format_args = {"xml"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "f", std::move(format_args), true));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "processor", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "processor");
    ASSERT_EQ(result.operations.size(), 3);
    
    ASSERT_EQ(result.operations[0].operation_name, "verbose");
    ASSERT_TRUE(result.operations[0].arguments.empty());
    
    ASSERT_EQ(result.operations[1].operation_name, "output");
    ASSERT_EQ(result.operations[1].arguments.size(), 1);
    ASSERT_EQ(result.operations[1].arguments[0], "final.out");
    
    ASSERT_EQ(result.operations[2].operation_name, "format");
    ASSERT_EQ(result.operations[2].arguments.size(), 1);
    ASSERT_EQ(result.operations[2].arguments[0], "xml");
    
    ASSERT_TRUE(result.operands.empty());
}

TEST(CLAASTVisitorAssembler, MixedOperandsAndOperations) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("output", 1);
    assembler.register_option("verbose", 0);

    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "input1.txt"));
    
    std::vector<std::string> output_args = {"result.txt"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "output", std::move(output_args), false));
    
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "input2.txt"));
    
    std::vector<std::string> verbose_args = {"extra1", "extra2"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "verbose", std::move(verbose_args), false));
    
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "input3.txt"));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "merger", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "merger");
    ASSERT_EQ(result.operations.size(), 2);
    
    ASSERT_EQ(result.operations[0].operation_name, "output");
    ASSERT_EQ(result.operations[0].arguments.size(), 1);
    ASSERT_EQ(result.operations[0].arguments[0], "result.txt");
    
    ASSERT_EQ(result.operations[1].operation_name, "verbose");
    ASSERT_TRUE(result.operations[1].arguments.empty());
    
    ASSERT_EQ(result.operands.size(), 5);
    ASSERT_EQ(result.operands[0], "input1.txt");
    ASSERT_EQ(result.operands[1], "input2.txt");
    ASSERT_EQ(result.operands[2], "extra1");
    ASSERT_EQ(result.operands[3], "extra2");
    ASSERT_EQ(result.operands[4], "input3.txt");
}

TEST(CLAASTVisitorAssembler, UnregisteredOperation) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("known", 1);

    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    
    std::vector<std::string> known_args = {"arg1"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "known", std::move(known_args), false));
    
    std::vector<std::string> unknown_args = {"arg2"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "unknown", std::move(unknown_args), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "tool", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "tool");
    ASSERT_EQ(result.operations.size(), 1);
    ASSERT_EQ(result.operations[0].operation_name, "known");
    ASSERT_EQ(result.operations[0].arguments.size(), 1);
    ASSERT_EQ(result.operations[0].arguments[0], "arg1");
    ASSERT_TRUE(result.operands.empty());
}

TEST(CLAASTVisitorAssembler, UnregisteredShortOption) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("output", 'o', 1);

    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    
    std::vector<std::string> known_args = {"file.out"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "o", std::move(known_args), true));
    
    std::vector<std::string> unknown_args = {"value"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "x", std::move(unknown_args), true));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "tool", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "tool");
    ASSERT_EQ(result.operations.size(), 1);
    ASSERT_EQ(result.operations[0].operation_name, "output");
    ASSERT_EQ(result.operations[0].arguments.size(), 1);
    ASSERT_EQ(result.operations[0].arguments[0], "file.out");
    ASSERT_TRUE(result.operands.empty());
}

TEST(CLAASTVisitorAssembler, InsufficientArgumentsErrorMessage) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("complex", 3);

    std::vector<std::string> arguments = {"arg1", "arg2"};
    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "complex", std::move(arguments), false));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "tool", std::move(operations));
    
    try {
        root->accept(&assembler);
        FAIL() << "Expected std::runtime_error to be thrown";
    } catch (const std::runtime_error& e) {
        std::string error_msg = e.what();
        EXPECT_TRUE(error_msg.find("Failed to parse operation: complex") != std::string::npos);
        EXPECT_TRUE(error_msg.find("not enough arguments") != std::string::npos);
        EXPECT_TRUE(error_msg.find("Expected: 3") != std::string::npos);
        EXPECT_TRUE(error_msg.find("inputed: 2") != std::string::npos);
    } catch (...) {
        FAIL() << "Expected std::runtime_error to be thrown";
    }
}

TEST(CLAASTVisitorAssembler, ComplexRealWorldScenario) {
    std::unique_ptr<WBE::Global> global = std::make_unique<WBE::Global>(0, nullptr, WBE::Directory({"test_env"}));
    
    WBE::CLAASTVisitorAssembler assembler;
    assembler.register_option("output", 'o', 1);
    assembler.register_option("include", 'I', 1);
    assembler.register_option("optimization", 'O', 1);
    assembler.register_option("verbose", 'v', 0);
    assembler.register_option("debug", 0);
    assembler.register_option("define", 'D', 2);

    std::vector<WBE::Ref<WBE::CLAASTNode>> operations;
    
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "main.cpp"));
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "helper.cpp"));
    
    std::vector<std::string> output_args = {"program.exe"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "o", std::move(output_args), true));
    
    std::vector<std::string> include_args = {"/usr/include", "extra_lib"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "I", std::move(include_args), true));
    
    std::vector<std::string> opt_args = {"2"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "O", std::move(opt_args), true));
    
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "verbose", false));
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "debug", false));
    
    std::vector<std::string> define_args = {"VERSION", "1.0", "BUILD_TYPE"};
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeOperation>(WBE::global_allocator(), "D", std::move(define_args), true));
    
    operations.push_back(WBE::make_ref<WBE::CLAASTNodeRootOperand>(WBE::global_allocator(), "utils.cpp"));

    auto root = WBE::make_ref<WBE::CLAASTNodeRoot>(WBE::global_allocator(), "gcc", std::move(operations));
    
    root->accept(&assembler);

    WBE::CLARoot result = assembler.get_cla_info();
    ASSERT_EQ(result.util_name, "gcc");
    ASSERT_EQ(result.operations.size(), 6);
    
    ASSERT_EQ(result.operations[0].operation_name, "output");
    ASSERT_EQ(result.operations[0].arguments.size(), 1);
    ASSERT_EQ(result.operations[0].arguments[0], "program.exe");
    
    ASSERT_EQ(result.operations[1].operation_name, "include");
    ASSERT_EQ(result.operations[1].arguments.size(), 1);
    ASSERT_EQ(result.operations[1].arguments[0], "/usr/include");
    
    ASSERT_EQ(result.operations[2].operation_name, "optimization");
    ASSERT_EQ(result.operations[2].arguments.size(), 1);
    ASSERT_EQ(result.operations[2].arguments[0], "2");
    
    ASSERT_EQ(result.operations[3].operation_name, "verbose");
    ASSERT_TRUE(result.operations[3].arguments.empty());
    
    ASSERT_EQ(result.operations[4].operation_name, "debug");
    ASSERT_TRUE(result.operations[4].arguments.empty());
    
    ASSERT_EQ(result.operations[5].operation_name, "define");
    ASSERT_EQ(result.operations[5].arguments.size(), 2);
    ASSERT_EQ(result.operations[5].arguments[0], "VERSION");
    ASSERT_EQ(result.operations[5].arguments[1], "1.0");
    
    ASSERT_EQ(result.operands.size(), 5);
    ASSERT_EQ(result.operands[0], "main.cpp");
    ASSERT_EQ(result.operands[1], "helper.cpp");
    ASSERT_EQ(result.operands[2], "extra_lib");
    ASSERT_EQ(result.operands[3], "BUILD_TYPE");
    ASSERT_EQ(result.operands[4], "utils.cpp");
}

#endif
