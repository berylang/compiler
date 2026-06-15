#pragma once
#include "../parser/ast/node.h"
#include "symboltable.h"
#include <string>
#include <vector>
#include <unordered_map>


struct FunctionSignature {
    std::string returnType;
    std::vector<std::string> paramTypes;
};

class TypeChecker {
public:
    TypeChecker(SymbolTable& symTable, std::unordered_map<std::string, FunctionSignature>& funcs, bool& errorsFlag);
    
    std::string analyzeExpression(ASTNode* node);
    bool typeMatchesLiteral(const std::string& type, NodeType litType);

private:
    SymbolTable& symbolTable;
    std::unordered_map<std::string, FunctionSignature>& functions;
    bool& errors;
};
