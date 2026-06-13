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
        type = NodeType::if_STMT;
        line = ln;
    }
};
