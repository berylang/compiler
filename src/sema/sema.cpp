#include "sema.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include <iostream>

SemanticAnalyzer::SemanticAnalyzer(ASTNode* root)
   : root(root), errors(false) {}

void SemanticAnalyzer::analyze() {
   auto* program = static_cast<ProgramNode*>(root);

   for (auto& node : program->globals)
       analyzeNode(node.get());
   if (program->runBlock)
       for (auto& node : program->runBlock->statements)
           analyzeNode(node.get());
}

void SemanticAnalyzer::analyzeNode(ASTNode* node) {
   if (node->type == NodeType::VAR_DECL)
       analyzeVarDecl(node);
}


void SemanticAnalyzer::analyzeVarDecl(ASTNode* node) {
   auto* decl = static_cast<VarDeclNode*>(node);
   if (symbolTable.exists(decl->name)) {
       std::cerr << "bery: error: '" << decl->name << "' already declared.\n";
       errors = true;
       return;
   }
   if (decl->isConst && !decl->value) {
       std::cerr << "bery: error: const '" << decl->name << "' must be initialized.\n";
       errors = true;
       return;
   }
   if (decl->value) {
       if (!typeMatchesLiteral(decl->varType, decl->value->type)) {
           std::cerr << "bery: error: type mismatch for '" << decl->name << "'.\n";
           errors = true;
           return;
       }
   }
   symbolTable.add(decl->name, {decl->varType, decl->isConst, decl->value != nullptr, 0});
}
bool SemanticAnalyzer::typeMatchesLiteral(const std::string& type, NodeType litType) {
   if (type == "int"    && litType == NodeType::INT_LIT)     return true;
   if (type == "bigint"    && litType == NodeType::INT_LIT)     return true;
   if (type == "float"    && litType == NodeType::DECIMAL_LIT)     return true;
   if (type == "bool" && litType == NodeType::BOOL_LIT) return true;
   if (type == "double" && litType == NodeType::DECIMAL_LIT)  return true;
   return false;
}
bool SemanticAnalyzer::hasErrors() { return errors; }