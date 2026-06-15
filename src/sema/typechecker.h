#pragma once
#include "../parser/ast/node.h"
#include "symboltable.h"
#include <string>

class TypeChecker {
public:
    TypeChecker(SymbolTable& symTable, bool& errorsFlag);
    
    std::string analyzeExpression(ASTNode* node);
    bool typeMatchesLiteral(const std::string& type, NodeType litType);

private:
    SymbolTable& symbolTable;
    bool& errors;
};
