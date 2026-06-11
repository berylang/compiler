#pragma once
#include "node.h"
#include <string>
#include <memory>

struct VarDeclNode : public ASTNode {
    std::string varType;
    std::string name;
    std::unique_ptr<ASTNode> value;
    bool isConst;


    VarDeclNode(std::string varType, std::string name, std::unique_ptr<ASTNode> value, int l, bool isConst=false) 
    : varType(varType), name(name), value(std::move(value)), isConst(isConst) {
        type = NodeType::VAR_DECL;
        line = l;
    }

};