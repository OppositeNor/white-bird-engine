#ifndef __WBE_CLA_AST_HH__
#define __WBE_CLA_AST_HH__

#include "core/memory/reference_strong.hh"
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
    CLAASTNodeRoot(const std::string& p_utility_name, std::vector<Ref<CLAASTNode>> p_operations)
        : utility_name(p_utility_name), operations(p_operations) {}
    virtual ~CLAASTNodeRoot() override {}

    virtual void accept(CLAASTVisitor* p_cla_ast_visitor) override;

private:
    std::string utility_name;
    std::vector<Ref<CLAASTNode>> operations;
};

/**
 * @class CLAASTNodeOperationNA
 * @brief Operation node that contains no arguments for CLA AST.
 *
 */
class CLAASTNodeOperationNA : public CLAASTNode {
public:
    /**
     * @brief Constructor.
     *
     * @param p_operation_name The name of the operation.
     */
    CLAASTNodeOperationNA(const std::string& p_operation_name)
        : operation_name(p_operation_name) {}

    virtual ~CLAASTNodeOperationNA() override {}

    virtual void accept(CLAASTVisitor* p_cla_ast_visitor) override;

private:
    std::string operation_name;
};

class CLAASTNodeOperation : public CLAASTNode {

    /**
     * @brief Constructor.
     *
     * @param p_operation_name The name of the operation.
     */
    CLAASTNodeOperation(const std::string& p_operation_name, std::vector<std::string>&& p_arguments)
        : operation_name(p_operation_name), arguments(std::move(p_arguments)) {}

    virtual ~CLAASTNodeOperation() override {}

    virtual void accept(CLAASTVisitor* p_cla_ast_visitor) override;
private:
    std::string operation_name;
    std::vector<std::string> arguments;
};

}

#endif
