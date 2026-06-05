#include "codegen.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include <cstring>
#include <unordered_map>
#include <sstream>
#include <cstdint>

CodeGen::CodeGen(ASTNode* root)
   : root(root), regCounter(0) {}

void CodeGen::generate(const std::string& outputPath) {
   auto* program = static_cast<ProgramNode*>(root);
   std::ostringstream body;
   body << "define i32 @main() {\n";
   body << "entry:\n";

   if (program->runBlock) {
       for (auto& node : program->runBlock->statements) {
           if (node->type == NodeType::VAR_DECL)
               genVarDecl(node.get(), body);
       }
   }
   body << "    ret i32 0\n";
   body << "}\n";
   std::ofstream out(outputPath);

   for (auto& node : program->globals) {
       if (node->type != NodeType::VAR_DECL) continue;
       auto* decl = static_cast<VarDeclNode*>(node.get());
       std::string lt = llvmType(decl->varType);
       std::string initVal = "0";
       if (decl->value) {
           if (decl->value->type == NodeType::INT_LIT) {
            initVal = std::to_string(static_cast<IntLitNode*>(decl->value.get())->value);
           } 
           else if (decl->value->type == NodeType::BOOL_LIT) {
            initVal = static_cast<BoolLitNode*>(decl->value.get())->value? "1" :"0";
           }
       }
       out << "@" << decl->name << " = global " << lt << " " << initVal << "\n";
   }
   out << "\n" << body.str();
}

void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<VarDeclNode*>(node);
   std::string lt = llvmType(decl->varType);
   out << "    %" << decl->name << " = alloca " << lt << "\n";
   if (!decl->value) return;
   std::string valReg = genLiteral(decl->value.get(), decl->varType, out);
   out << "    store " << lt << " " << valReg << ", " << lt << "* %" << decl->name << "\n";
}

std::string CodeGen::genLiteral(ASTNode* node, const std::string& varType, std::ostream& out) {
   std::string reg = newReg();
   if (node->type == NodeType::INT_LIT) {
       auto* lit = static_cast<IntLitNode*>(node);
       out << "    " << reg << " = add " << llvmType(varType) << " 0, " << lit->value << "\n";
       return reg;
   }

   if (node->type == NodeType::BOOL_LIT) {
      auto* lit = static_cast<BoolLitNode*>(node);
      out << "    " << reg << " = add i1 0, " << (lit->value? 1:0) << "\n";
      return reg;
   }
   return "0";
}

std::string CodeGen::llvmType(const std::string& t) {
   if (t == "int") return "i32";
   if (t == "bool") return "i1";
   return "i32";
}
std::string CodeGen::newReg() {
   return "%" + std::to_string(++regCounter);
}