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

namespace WhiteBirdEngine {

void CLAASTNodeRoot::accept(CLAASTVisitor* p_cla_ast_visitor) {
    p_cla_ast_visitor->visit(this);
}

void CLAASTNodeRootOperand::accept(CLAASTVisitor* p_cla_ast_visitor) {
    p_cla_ast_visitor->visit(this);
}

void CLAASTNodeOperation::accept(CLAASTVisitor* p_cla_ast_visitor) {
    p_cla_ast_visitor->visit(this);
}

}
