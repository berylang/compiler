#pragma once
#include "node.h"
#include <string>
#include <memory>
#include <vector>

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

struct EnumDeclNode : public ASTNode {
    std::string name;
    std::vector<std::string> values;

    EnumDeclNode(std::string name, std::vector<std::string> values, int ln)
        : name(name), values(std::move(values)) {
        type = NodeType::ENUM_DECL;
        line = ln;
    }
};