#pragma once
#include "node.h"
#include <string>



struct IntLitNode : public ASTNode {
    long long value;
    IntLitNode(long long v, int ln) : value(v) {
        type = NodeType::INT_LIT;
        line = ln;
    }
};

struct BoolLitNode: public ASTNode {
    bool value;
    BoolLitNode(bool v, int ln) : value(v) {
        type = NodeType::BOOL_LIT;
        line = ln;
    }
};

struct DecimalLitNode : public ASTNode {
    double value;
    DecimalLitNode(double v, int ln) : value(v) {
        type = NodeType::DECIMAL_LIT;
        line = ln;
    }
};

struct IdentNode : public ASTNode {
    std::string name;
    std::string varType;

    IdentNode(const std::string& name, const std::string& varType, int ln):
        name(name), varType(varType) {
            type = NodeType::IDENT;
            line = ln;
        }
};

struct CharLitNode : public ASTNode {
    char value;
    CharLitNode(char v, int ln) : value(v) {
        type = NodeType::CHAR_LIT;
        line = ln;
    }
};

struct StringLitNode : public ASTNode {
    std::string value;
    StringLitNode(std::string& s, int ln) : value(s) {
        type = NodeType::STRING_LIT;
        line = ln;
    } 
};

struct NullLitNode : public ASTNode {
    NullLitNode(int ln) {
        type = NodeType :: NULL_LIT;
        line = ln;
    }
};