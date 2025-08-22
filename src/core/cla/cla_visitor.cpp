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
#include "core/cla/cla_ast.hh"
#include "core/cla/cla_ast_visitor.hh"
#include <stdexcept>
#include <string>

namespace WhiteBirdEngine {


void CLAASTVisitorParamCheck::visit(CLAASTNodeRoot* p_node) {
    return;
}

void CLAASTVisitorParamCheck::visit(CLAASTNodeOperationNA* p_node) {
    auto iter = operations->find(p_node->get_operation_name());
    if (iter == operations->end()) {
        throw std::runtime_error("Unrecognized operation: " + p_node->get_operation_name() + ".");
    }
    if (iter->second.capacity() != 0) {
        throw std::runtime_error("Operation: " + iter->first + " expects " + std::to_string(iter->second.size()) + " arguments. But none is set.");
    }
    // For no-argument operations, push back an empty string flags the operation.
    iter->second.push_back("");
}

void CLAASTVisitorParamCheck::visit(CLAASTNodeOperation* p_node) {
    auto iter = operations->find(p_node->get_operation_name());
    if (iter == operations->end()) {
        return;
    }
    auto& provided_arguments = p_node->get_arguments();
    if (iter->second.capacity() != provided_arguments.size()) {
        throw std::runtime_error("Operation: " + iter->first + " expects " + std::to_string(iter->second.size())
                                 + " arguments. But provided " + std::to_string(provided_arguments.size()) + " arguments.");
    }
    for (auto& arg : provided_arguments) {
        iter->second.push_back(arg);
    }
}

}
