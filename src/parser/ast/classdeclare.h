#pragma once
#include "node.h"
#include <string>
#include <vector>
#include <memory>

struct ClassDeclNode : public ASTNode {
    std::string name;

    std::vector<std::unique_ptr<ASTNode>> attributes;
    std::vector<std::unique_ptr<ASTNode>> methods;

    ClassDeclNode(const std::string& className, int ln ): name (className){
            type = NodeType::CLASS_DEF;
            line=ln;
        }
};