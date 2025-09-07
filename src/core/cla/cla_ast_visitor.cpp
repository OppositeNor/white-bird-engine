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
#include "core/cla/cla_ast_visitor.hh"
#include "core/cla/cla_ast.hh"
#include "core/cla/cla_utils.hh"
#include "core/logging/log.hh"
#include "utils/defs.hh"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace WhiteBirdEngine {

void CLAASTVisitorToString::visit(CLAASTNodeRoot* p_node) {
    ss << std::string(2 * indent_depth, ' ') << "utility: " << p_node->get_utility_name() << "\n";
    auto operations = p_node->get_operations();
    if (!operations.empty()) {
        ss << std::string(2 * indent_depth, ' ') << "operations:\n";
        ++indent_depth;
        for (auto& operation : operations) {
            ss << std::string(2 * indent_depth, ' ') << "- ";
            operation->accept(this);
        }
        --indent_depth;
    }
}

void CLAASTVisitorToString::visit(class CLAASTNodeRootOperand* p_node) {
    ss << "operand: " << p_node->get_operand_name() << "\n";
}

void CLAASTVisitorToString::visit(class CLAASTNodeOperation* p_node) {
    ss << "operation: " << p_node->get_operation_name() << "\n";
    auto arguments = p_node->get_arguments();
    if (!arguments.empty()) {
        ss << std::string(2 * (indent_depth + 1), ' ') << "arguments:\n";
        for (auto& argument : arguments) {
            ss << std::string(2 * (indent_depth + 2), ' ') << "- " << argument << "\n";
        }
    }
}

void CLAASTVisitorAssembler::visit(CLAASTNodeRoot* p_node) {
    root.util_name = p_node->get_utility_name();
    auto operations = p_node->get_operations();
    for (auto& operation : operations) {
        operation->accept(this);
    }
}

void CLAASTVisitorAssembler::visit(CLAASTNodeRootOperand* p_node) {
    root.operands.push_back(p_node->get_operand_name());
}

void CLAASTVisitorAssembler::visit(CLAASTNodeOperation* p_node) {
    std::string operation_name = p_node->get_operation_name();
    int32_t arg_num;
    if (!get_operation_arg_count(operation_name, p_node, arg_num)) {
        return;
    }
    int32_t input_arg_num = p_node->get_arguments().size();
    if (input_arg_num < arg_num) {
        throw std::runtime_error("Failed to parse operation: " + operation_name + ", not enough arguments. Expected: "
                                 + std::to_string(arg_num) + ", inputed: " + std::to_string(input_arg_num) + ".");
    }
    get_operation(p_node, operation_name, arg_num);
}

bool CLAASTVisitorAssembler::get_operation_arg_count(std::string& p_op_name, CLAASTNodeOperation* p_node, int32_t& p_arg_num) {
    if (p_node->is_name_short()) {
        auto arg_name_short = arg_short_to_long.find(p_op_name[0]);
        if (arg_name_short == arg_short_to_long.end()) {
            wbe_console_log(WBE_CHANNEL_GLOBAL)->message("Unrecognized argument name: \"" + p_op_name +  "\", ignored.");
            return false;
        }
        p_op_name = arg_name_short->second;
    }
    auto find_arg_num = arg_count_long.find(p_op_name);
    WBE_DEBUG_ASSERT(!p_node->is_name_short() || find_arg_num != arg_count_long.end());
    if (find_arg_num == arg_count_long.end()) {
        wbe_console_log(WBE_CHANNEL_GLOBAL)->message("Unrecognized argument name: \"" + p_op_name +  "\", ignored.");
        return false;
    }
    p_arg_num = find_arg_num->second;
    return true;
}

void CLAASTVisitorAssembler::get_operation(CLAASTNodeOperation* p_node, const std::string& p_op_name, int32_t p_arg_num) {
    auto& arguments = p_node->get_arguments();
    int32_t input_arg_num = p_node->get_arguments().size();
    if (p_arg_num < 0) {
        p_arg_num = arguments.size();
    }
    CLAOperation operation{};
    operation.operation_name = p_op_name;
    int32_t i;
    for (i = 0; i < p_arg_num; ++i) {
        operation.arguments.push_back(arguments[i]);
    }
    for (; i < input_arg_num; ++i) {
        root.operands.push_back(arguments[i]);
    }
    root.operations.push_back(operation);
}

}
