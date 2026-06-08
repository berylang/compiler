#pragma once
#include <memory>
#include "../parser/ast/node.h"
#include "symbotltable.h"


class SemanticAnalyzer {
public:
   SemanticAnalyzer(ASTNode* root);
   void analyze();
   bool hasErrors();


private:
   ASTNode* root;
   SymbolTable symbolTable;
   bool errors;


   void analyzeNode(ASTNode* node);
   void analyzeVarDecl(ASTNode* node);
   void analyzeArrayDecl(ASTNode* node);
   bool typeMatchesLiteral(const std::string& type, NodeType litType);
   std::string analyzeExpression(ASTNode* node);
};
  
