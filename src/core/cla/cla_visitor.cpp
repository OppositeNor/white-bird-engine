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
