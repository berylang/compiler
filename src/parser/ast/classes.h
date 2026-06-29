#pragma once
#include "node.h"
#include <string>
#include <vector>
#include <memory>

struct AttributeSectionNode : public ASTNode {
    std::string selfRef;
    std::vector<std::unique_ptr<ASTNode>> attributes;
    AttributeSectionNode(std::string self, std::vector<std::unique_ptr<ASTNode>> attrs, int ln) {
        type = NodeType::ATTRIBUTE_SECTION;
        line = ln;
        selfRef = std::move(self);
        attributes = std::move(attrs);
    }
};

struct MethodSectionNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> methods;
    MethodSectionNode(std::vector<std::unique_ptr<ASTNode>> m, int ln) {
        type = NodeType::METHOD_SECTION;
        line = ln;
        methods = std::move(m);
    }
};

struct ClassDefNode : public ASTNode {
    std::string name;
    std::unique_ptr<AttributeSectionNode> attributes;
    std::unique_ptr<MethodSectionNode> methods; // optional
    ClassDefNode(std::string n,std::unique_ptr<AttributeSectionNode> attr,std::unique_ptr<MethodSectionNode> meth,int ln) {
        type = NodeType::CLASS_DEF;
        line = ln;
        name = std::move(n);
        attributes = std::move(attr);
        methods = std::move(meth);
    }
};