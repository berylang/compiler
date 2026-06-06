#pragma once
#include "node.h"
#include <string>



struct IntLitNode : public ASTNode {
    long long value;
    IntLitNode(long long v) : value(v) {
        type = NodeType::INT_LIT;
    }
};

struct DoubleLitNode: public ASTNode{
     double value;
     DoubleLitNode(double v):value(v){
        type=NodeType::DOUBLE_LIT;
     }
};

struct BoolLitNode: public ASTNode {
    bool value;
    BoolLitNode(bool v) : value(v) {type = NodeType::BOOL_LIT;}
};
struct IdentNode : public ASTNode {
    std::string name;
    std::string varType;

    IdentNode(const std::string& name, const std::string& varType):
        name(name), varType(varType) {
            type = NodeType::IDENT;
        }
};

