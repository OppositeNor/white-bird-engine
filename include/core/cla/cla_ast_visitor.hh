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
#ifndef __WBE_CLA_AST_VISITOR_HH__
#define __WBE_CLA_AST_VISITOR_HH__
namespace WhiteBirdEngine {

/**
 * @class CLAASTVisitor
 * @brief Visitor for CLA AST nodes.
 *
 */
class CLAASTVisitor {
public:
    CLAASTVisitor() = default;
    virtual ~CLAASTVisitor() {}
    CLAASTVisitor(const CLAASTVisitor&) = delete;
    CLAASTVisitor(CLAASTVisitor&&) = delete;
    CLAASTVisitor& operator=(const CLAASTVisitor&) = delete;
    CLAASTVisitor& operator=(CLAASTVisitor&&) = delete;


    /**
     * @brief Visit root node.
     *
     * @param p_node The root node.
     */
    virtual void visit(class CLAASTNodeRoot* p_node) = 0;

    /**
     * @brief Visit no argument operations.
     *
     * @param p_node The operation node.
     */
    virtual void visit(class CLAASTNodeRootOperand* p_node) = 0;

    /**
     * @brief Visit operations.
     *
     * @param p_node The node to visit.
     */
    virtual void visit(class CLAASTNodeOperation* p_node) = 0;

};

}

#endif
