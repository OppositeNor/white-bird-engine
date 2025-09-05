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
#include <cstdint>
#include <sstream>
#include <string>
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

/**
 * @class CLAASTVisitorPrint
 * @brief AST visitor which prints the structure of the tree.
 *
 */
class CLAASTVisitorToString : public CLAASTVisitor {
public:
    CLAASTVisitorToString()
        : indent_depth(0) {}
    virtual ~CLAASTVisitorToString() override {}

    virtual void visit(class CLAASTNodeRoot* p_node) override;
    virtual void visit(class CLAASTNodeRootOperand* p_node) override;
    virtual void visit(class CLAASTNodeOperation* p_node) override;

    /**
     * @brief Get the string of the visited AST.
     *
     * @return The string of the visited AST.
     */
    std::string get_string() const {
        return ss.str();
    }
private:
    uint32_t indent_depth;
    std::stringstream ss;
};

}

#endif
