#pragma once
#include "node.h"
#include "accessSpecifier.h"
#include <string>
#include <memory>
#include <vector>

struct VarDeclNode : public ASTNode {
    std::string varType;
    std::string name;
    std::unique_ptr<ASTNode> value;
    AccessSpecifier access;
    bool isConst;


    VarDeclNode(std::string varType, std::string name, std::unique_ptr<ASTNode> value, AccessSpecifier access, int l, bool isConst=false) 
    : varType(varType), name(name), value(std::move(value)),access(access), isConst(isConst) {
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