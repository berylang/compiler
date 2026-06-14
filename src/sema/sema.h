#pragma once
#include <memory>
#include "../parser/ast/node.h"
#include "symboltable.h"


class SemanticAnalyzer {
public:
   SemanticAnalyzer(ASTNode* root);
   void analyze();
   bool hasErrors();
   SymbolTable symbolTable;
   int loopOrSwitchDepth = 0;


private:
   ASTNode* root;
   
   bool errors;


   void analyzeIfStmt(ASTNode* node);
   void analyzeWhileStmt(ASTNode* node);
   void analyzeDoWhileStmt(ASTNode* node);
   void analyzeBlock(ASTNode* node);
   void analyzeNode(ASTNode* node);
   void analyzeVarDecl(ASTNode* node);
   void analyzeArrayDecl(ASTNode* node);
   void analyzeSwitchStmt(ASTNode* node);
   void analyzeBreakStmt(ASTNode* node);

   //@deprecated do not use typeMatchesLiteral()
   bool typeMatchesLiteral(const std::string& type, NodeType litType);
   std::string analyzeExpression(ASTNode* node);
};
  
