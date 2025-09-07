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
#include "core/cla/cla_utils.hh"
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
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

/**
 * @class CLAASTOrganizer
 * @brief AST visitor which assembles the operations according
 * to the number of arguments each option accepts.
 *
 */
class CLAASTVisitorAssembler : public CLAASTVisitor {
public:
    CLAASTVisitorAssembler()
        : root({}) {
    }
    virtual ~CLAASTVisitorAssembler() override {}

    virtual void visit(class CLAASTNodeRoot* p_node) override;
    virtual void visit(class CLAASTNodeRootOperand* p_node) override;
    virtual void visit(class CLAASTNodeOperation* p_node) override;

    void register_option(const std::string& p_option_name, uint32_t p_arg_count) {
        arg_count_long[p_option_name] = p_arg_count;
    }

    void register_option(const std::string& p_option_name, char p_option_name_short, uint32_t p_arg_count) {
        arg_count_long[p_option_name] = p_arg_count;
        arg_short_to_long[p_option_name_short] = p_option_name;
    }

    CLARoot get_cla_info() const {
        return root;
    }

private:
    std::unordered_map<std::string, int32_t> arg_count_long;
    std::unordered_map<char, std::string> arg_short_to_long;
    bool get_operation_arg_count(std::string& p_op_name, CLAASTNodeOperation* p_node, int32_t& p_arg_num);
    void get_operation(CLAASTNodeOperation* p_node, const std::string& p_op_name, int32_t p_arg_num);

    CLARoot root;
};

}

#endif
