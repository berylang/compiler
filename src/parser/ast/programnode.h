#pragma once
#include "node.h"
#include <vector>
#include <memory>

struct RunBlockNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
    RunBlockNode(int ln) {
        type = NodeType::RUN_BLOCK;
        line = ln;
    }
};

struct ProgramNode :public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> globals;
    std::unique_ptr<RunBlockNode> runBlock;

    ProgramNode(int ln = 1) {
        type = NodeType::PROGRAM;
        line = ln;
    }
};