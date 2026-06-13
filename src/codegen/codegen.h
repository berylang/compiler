#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include "../parser/ast/node.h"
#include "../sema/symboltable.h"

class CodeGen {
public:
   CodeGen(ASTNode* root, SymbolTable& symTable);
   void generate(const std::string& outputPath);

private:
   ASTNode* root;
   int regCounter;
   int strCounter = 0;
   SymbolTable& symTable;
   std::ostringstream globalStrings;
   std::vector<std::string> breakTracker;
   std::string extractConstant(ASTNode* node);

   std::string newReg();
   std::string llvmType(const std::string& berryType);
   std::string escapeLLVMString(const std::string& str);

   void genVarDecl(ASTNode* node, std::ostream& out);
   void genArrayDecl(ASTNode* node, std::ostream& out);
   
   std::string genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out);

   void genIfStmt(ASTNode* node, std::ostream& out);
   void genWhileStmt(ASTNode* node, std::ostream& out);
   void genBlock(ASTNode* node, std::ostream& out);

   void genSwitchStmt(ASTNode* node, std::ostream& out);
   void genBreakStmt(ASTNode* node, std::ostream& out);
};
