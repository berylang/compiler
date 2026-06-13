#pragma once
#include "node.h"
#include <vector>
#include <memory>

struct BlockNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
    BlockNode(int ln) {
        type = NodeType::BLOCK;
        line = ln;
    }
};