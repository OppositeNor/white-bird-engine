// Copyright (c) 2025 OppositeNor.
// Licensed under GPL-3.0
#include "core/cla/cla_ast.hh"
#include "core/cla/cla_ast_visitor.hh"

namespace WhiteBirdEngine {

void CLAASTNodeRoot::accept(CLAASTVisitor* p_cla_ast_visitor) {
    p_cla_ast_visitor->visit(this);
    for (auto& operation : operations) {
        operation->accept(p_cla_ast_visitor);
    }
}

void CLAASTNodeOperationNA::accept(CLAASTVisitor* p_cla_ast_visitor) {
    p_cla_ast_visitor->visit(this);
}

void CLAASTNodeOperation::accept(CLAASTVisitor* p_cla_ast_visitor) {
    p_cla_ast_visitor->visit(this);
}

}
