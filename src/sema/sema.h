#pragma once
#include <memory>
#include "../parser/ast/node.h"
#include "symboltable.h"
#include "typechecker.h"
#include <unordered_map>

class SemanticAnalyzer {
public:
   SemanticAnalyzer(ASTNode* root);
   void analyze();
   bool hasErrors();
   SymbolTable symbolTable;


private:
   ASTNode* root;
   TypeChecker typeChecker; 
   int loopOrSwitchDepth = 0;
   int loopDepth = 0;
   bool errors;
   std::unordered_map<std::string, FunctionSignature> functions;
   std::string currentFunctionReturnType = "";

   void analyzeIfStmt(ASTNode* node);
   void analyzeWhileStmt(ASTNode* node);
   void analyzeDoWhileStmt(ASTNode* node);
   void analyzeForStmt(ASTNode* node);
   void analyzeBlock(ASTNode* node);
   void analyzeNode(ASTNode* node);
   void analyzeVarDecl(ASTNode* node);
   void analyzeArrayDecl(ASTNode* node);
   void analyzeSwitchStmt(ASTNode* node);
   void analyzeBreakStmt(ASTNode* node);
   void analyzeContinueStmt(ASTNode* node);

   void analyzeFuncDef(ASTNode* node);
   void analyzeReturnStmt(ASTNode* node);
};
