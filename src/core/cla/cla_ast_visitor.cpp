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

}
