#pragma once
#include "node.h"
#include <string>
#include <memory>



struct UnaryExprNode : public ASTNode{
    std::string optr;
    std::unique_ptr<ASTNode> operand;

    UnaryExprNode(std::string& opr, std::unique_ptr<ASTNode> expr, int ln):
        optr(opr), operand(std::move(expr)){
            type = NodeType::UNARY_EXPR;
            line = ln;
        }

};

struct GroupedExprNode : public ASTNode{
    std::unique_ptr<ASTNode> expression;

    GroupedExprNode(std::unique_ptr<ASTNode> expr, int ln):
        expression(std::move(expr)){
            type = NodeType::GROUPED_EXPR;
            line = ln;
        }
};

struct BinaryExprNode : public ASTNode{
    std::string optr;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    std::string opType;

    BinaryExprNode(std::string& opr, std::unique_ptr<ASTNode> l,std::unique_ptr<ASTNode> r, int ln):
        optr(opr),
        left(std::move(l)),
        right(std::move(r)),
        opType("int")
        {
            type = NodeType::BINARY_EXPR;
            line = ln;
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
        bool neg,
        int ln
    )
        : value(std::move(val)),
          lower(std::move(low)),
          upper(std::move(up)),
          isNegated(neg),
          opType("int")
    {
        type = NodeType::BETWEEN_EXPR;
        line = ln;
    }
};

struct TernaryExprNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> trueExpr;
    std::unique_ptr<ASTNode> falseExpr;
    std::string resolvedType;

    TernaryExprNode(std::unique_ptr<ASTNode> c, std::unique_ptr<ASTNode> te, std::unique_ptr<ASTNode> fe, int ln) :
    condition(std::move(c)), trueExpr(std::move(te)), falseExpr(std::move(fe)), resolvedType("int") {
        type = NodeType::TERNARY_EXPR;
        line = ln;
    }
};

struct AssignmentExprNode : public ASTNode {
    std::unique_ptr<ASTNode> target;
    std::unique_ptr<ASTNode> value;

    AssignmentExprNode(std::unique_ptr<ASTNode> t, std::unique_ptr<ASTNode> v, int ln)
        : target(std::move(t)), value(std::move(v)) {
        type = NodeType::ASSIGNMENT_EXPR;
        line = ln;
    }
};

struct CastExprNode : public ASTNode {
    std::string targetType;
    std::unique_ptr<ASTNode> expr;
    std::string srcType;

    CastExprNode(std::string t, std::unique_ptr<ASTNode> e, int ln) : targetType(t), expr(std::move(e)) {
        type = NodeType::CAST_EXPR;
        line = ln;
    }
};

struct IndexExprNode : public ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> index;

    IndexExprNode(std::string name, std::unique_ptr<ASTNode> index, int ln)
        : name(name), index(std::move(index)) {
        type = NodeType::INDEX_EXPR;
        line = ln;
    }
};