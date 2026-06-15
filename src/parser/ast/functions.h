#pragma once
#include "node.h"
#include "blocknode.h"
#include <string>
#include <vector>
#include <memory>
#include <utility>

struct FunctionDefNode : public ASTNode {
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters; 
    std::string returnType;
    std::unique_ptr<BlockNode> body;

    FunctionDefNode(const std::string& name, std::vector<std::pair<std::string, std::string>> params, const std::string& retType, std::unique_ptr<BlockNode> b, int ln)
        : name(name), parameters(std::move(params)), returnType(retType), body(std::move(b)) {
        type = NodeType::FUNC_DEF;
        line = ln;
    }
};

struct ReturnStmtNode : public ASTNode {
    std::unique_ptr<ASTNode> value;

    ReturnStmtNode(std::unique_ptr<ASTNode> val, int ln)
        : value(std::move(val)) {
        type = NodeType::RETURN_STMT;
        line = ln;
    }
};

struct CallExprNode : public ASTNode {
    std::string callee; 
    std::vector<std::unique_ptr<ASTNode>> arguments;

    CallExprNode(const std::string& callee, std::vector<std::unique_ptr<ASTNode>> args, int ln)
        : callee(callee), arguments(std::move(args)) {
        type = NodeType::CALL_EXPR;
        line = ln;
    }
};
