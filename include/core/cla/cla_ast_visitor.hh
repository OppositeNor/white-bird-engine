#ifndef __WBE_CLA_AST_VISITOR_HH__
#define __WBE_CLA_AST_VISITOR_HH__
#include <map>
#include <string>
#include <vector>
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
    virtual void visit(class CLAASTNodeOperationNA* p_node) = 0;

    /**
     * @brief Visit operations.
     *
     * @param p_node The node to visit.
     */
    virtual void visit(class CLAASTNodeOperation* p_node) = 0;

};

class CLAASTVisitorParamCheck : public CLAASTVisitor {
public:
    virtual ~CLAASTVisitorParamCheck() override {}
    CLAASTVisitorParamCheck(const CLAASTVisitorParamCheck&) = delete;
    CLAASTVisitorParamCheck(CLAASTVisitorParamCheck&&) = delete;
    CLAASTVisitorParamCheck& operator=(const CLAASTVisitorParamCheck&) = delete;
    CLAASTVisitorParamCheck& operator=(CLAASTVisitorParamCheck&&) = delete;

    CLAASTVisitorParamCheck(std::map<std::string, std::vector<std::string>>& p_operations)
        : operations(&p_operations) {}

    virtual void visit(class CLAASTNodeRoot* p_node) override;
    virtual void visit(class CLAASTNodeOperationNA* p_node) override;
    virtual void visit(class CLAASTNodeOperation* p_node) override;

private:
    std::map<std::string, std::vector<std::string>>* operations;
};

}

#endif
