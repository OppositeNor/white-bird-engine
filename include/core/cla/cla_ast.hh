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
#ifndef __WBE_CLA_AST_HH__
#define __WBE_CLA_AST_HH__

#include "core/memory/reference_strong.hh"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace WhiteBirdEngine {

class CLAASTVisitor;

/**
 * @class CLAASTNode
 * @brief Node of an abstract syntax tree for CLAs.
 */
class CLAASTNode {
public:
    CLAASTNode() = default;
    virtual ~CLAASTNode() {}
    CLAASTNode(const CLAASTNode&) = delete;
    CLAASTNode(CLAASTNode&&) = delete;
    CLAASTNode operator=(const CLAASTNode&) = delete;
    CLAASTNode operator=(CLAASTNode&&) = delete;

    /**
     * @brief Accepts an visitor.
     *
     * @param p_cla_ast_visitor The visitor to accept.
     */
    virtual void accept(CLAASTVisitor* p_cla_ast_visitor) = 0;
};

/**
 * @class CLAASTNodeRoot
 * @brief Root node for CLA AST.
 * The root node contain informations about the utility name and the operations.
 */
class CLAASTNodeRoot : public CLAASTNode {
public:
    /**
     * @brief Constructor.
     *
     * @param p_utility_name The name of the utility.
     */
    CLAASTNodeRoot(const std::string& p_utility_name, std::vector<Ref<CLAASTNode>>&& p_operations)
        : utility_name(p_utility_name), operations(std::move(p_operations)) {}
    virtual ~CLAASTNodeRoot() override {}

    virtual void accept(CLAASTVisitor* p_cla_ast_visitor) override;

    /**
     * @brief Get the utility name.
     *
     * @return The utility name.
     */
    const std::string& get_utility_name() const {
        return utility_name;
    }

    /**
     * @brief Get the operations.
     *
     * @return The operations.
     */
    const std::vector<Ref<CLAASTNode>>& get_operations() const {
        return operations;
    }


private:
    std::string utility_name;
    std::vector<Ref<CLAASTNode>> operations;
};

/**
 * @class CLAASTNodeOperation
 * @brief Operation node for CLA AST.
 *
 */
class CLAASTNodeOperation : public CLAASTNode {
public:
    /**
     * @brief Constructor.
     *
     * @param p_operation_name The name of the operation.
     * @param p_is_short Is the name a short notation.
     */
    CLAASTNodeOperation(const std::string& p_operation_name, bool p_is_short)
        : CLAASTNodeOperation(p_operation_name, {}, p_is_short) {}
    /**
     * @brief Constructor.
     *
     * @param p_operation_name The name of the operation.
     * @param p_arguments The arguments of the operation.
     * @param p_is_short Is the name a short notation.
     */
    CLAASTNodeOperation(const std::string& p_operation_name, std::vector<std::string>&& p_arguments, bool p_is_short)
        : is_short(p_is_short), operation_name(p_operation_name), arguments(std::move(p_arguments)) {}

    virtual ~CLAASTNodeOperation() override {}

    virtual void accept(CLAASTVisitor* p_cla_ast_visitor) override;

    /**
     * @brief Get operation name.
     *
     * @return The name of the operation.
     */
    const std::string& get_operation_name() const {
        return operation_name;
    }

    /**
     * @brief Get the arguments.
     *
     * @return The arguments of the operation.
     */
    const std::vector<std::string>& get_arguments() const {
        return arguments;
    }

    /**
     * @brief Push argument into this operation node.
     *
     * @param p_argument The argument to be pushed.
     */
    void push_argument(const std::string& p_argument) {
        arguments.push_back(p_argument);
    }

    /**
     * @brief Remove the argument of a specific index.
     *
     * @param p_index The index to be removed.
     */
    void remove_argument(uint32_t p_index) {
        if (arguments.size() >= p_index) {
            throw std::runtime_error("Failed to remove argument: index " + std::to_string(p_index) + " out of range."
                                     " arguments size: " + std::to_string(arguments.size()));
        }
        arguments.erase(arguments.begin() + p_index);
    }

    /**
     * @brief Is the name of this operation short.
     *
     * @return True if the name of this operation is short. False otherwise.
     */
    bool is_name_short() const {
        return is_short;
    }

private:
    bool is_short;
    std::string operation_name;
    std::vector<std::string> arguments;
};

/**
 * @class CLAASTNodeRootOperand
 * @brief Root operand.
 *
 */
class CLAASTNodeRootOperand : public CLAASTNode {
public:
    /**
     * @brief Constructor.
     *
     * @param p_operand_name The name of the operand.
     */
    CLAASTNodeRootOperand(const std::string& p_operand_name)
        : operand_name(p_operand_name) {}

    virtual ~CLAASTNodeRootOperand() override {}

    virtual void accept(CLAASTVisitor* p_cla_ast_visitor) override;

    /**
     * @brief Get operation name.
     *
     * @return The name of the operation.
     */
    const std::string& get_operand_name() const {
        return operand_name;
    }

private:
    std::string operand_name;
};

}

#endif
