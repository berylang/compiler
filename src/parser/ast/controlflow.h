#pragma once
#include "node.h"
#include "blocknode.h"
#include <vector>
#include <memory>

struct IfStmtNode : public ASTNode
{
    std::unique_ptr<ASTNode> conditions;
    std::unique_ptr<BlockNode> ifBranch;
    std::unique_ptr<ASTNode> elseBranch;

    IfStmtNode(
        std::unique_ptr<ASTNode> c,
        std::unique_ptr<BlockNode> i,
        std::unique_ptr<ASTNode> e,
        int ln
    )
        : conditions(std::move(c)),
          ifBranch(std::move(i)),
          elseBranch(std::move(e))

    {
        type = NodeType::IF_STMT;
        line = ln;
    }
};

struct CaseBlock
{
    std::unique_ptr<ASTNode> value;
    std::vector<std::unique_ptr<ASTNode>> statements;
};

struct SwitchStmtNode : public ASTNode
{
    std::unique_ptr<ASTNode> condition;
    std::vector<CaseBlock> cases;

    std::vector<std::unique_ptr<ASTNode>> defaultBlock;
    bool hasDefault;

    SwitchStmtNode(int ln)
    {
        type = NodeType::SWITCH_STMT;
        line = ln;
        hasDefault = false;
    }
};

struct BreakStmtNode : public ASTNode
{
    BreakStmtNode(int ln)
    {
        type = NodeType::BREAK_STMT;
        line = ln;
    }
};
struct WhileStmtNode : public ASTNode{
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<BlockNode> body;

    WhileStmtNode(
        std::unique_ptr<ASTNode> c,
        std::unique_ptr<BlockNode> b,
        int ln
    ):    condition(std::move(c)),
          body(std::move(b))
    {
        type = NodeType::WHILE_STMT;
        line = ln;

    }
          
};
