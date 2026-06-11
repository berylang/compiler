#pragma once
#include "node.h"
#include <string>
#include <memory>



struct UnaryExprNode : public ASTNode{
    std::string optr;
    std::unique_ptr<ASTNode> operand;

    UnaryExprNode(std::string& opr, std::unique_ptr<ASTNode> expr):
        optr(opr), operand(std::move(expr)){
            type = NodeType::UNARY_EXPR;
        }

};

struct GroupedExprNode : public ASTNode{
    std::unique_ptr<ASTNode> expression;

    GroupedExprNode(std::unique_ptr<ASTNode> expr):
        expression(std::move(expr)){
            type = NodeType::GROUPED_EXPR;
        }
};

struct BinaryExprNode : public ASTNode{
    std::string optr;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    std::string opType;

    BinaryExprNode(std::string& opr, std::unique_ptr<ASTNode> l,std::unique_ptr<ASTNode> r):
        optr(opr),
        left(std::move(l)),
        right(std::move(r)),
        opType("int")
        {
            type = NodeType::BINARY_EXPR;
        }

};

struct BetweenExprNode : public ASTNode{
    std::unique_ptr<ASTNode> value;
    std::unique_ptr<ASTNode> lower;
    std::unique_ptr<ASTNode> upper;
    bool isNegated;
    std::string opType;

    BetweenExprNode(
        std::unique_ptr<ASTNode> val,
        std::unique_ptr<ASTNode> low,
        std::unique_ptr<ASTNode> up,
        bool neg
    )
        : value(std::move(val)),
          lower(std::move(low)),
          upper(std::move(up)),
          isNegated(neg),
          opType("int")
    {
        type = NodeType::BETWEEN_EXPR;
    }
};