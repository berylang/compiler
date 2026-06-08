#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include "../parser/ast/node.h"

class CodeGen {
public:
   CodeGen(ASTNode* root);
   void generate(const std::string& outputPath);

private:
   ASTNode* root;
   int regCounter;

   std::string newReg();
   std::string llvmType(const std::string& berryType);
   void genVarDecl(ASTNode* node, std::ostream& out);
   
   std::string genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out);
};