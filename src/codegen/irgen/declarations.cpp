#include "../codegen.h"
#include "../../parser/ast/vardecl.h"
#include "../../parser/ast/arraydeclare.h"


void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<VarDeclNode*>(node);
   std::string lt = llvmType(decl->varType);
   out << "    %" << decl->name << " = alloca " << lt << "\n";
   if (!decl->value) return;
   std::string valReg = genExpression(decl->value.get(), decl->varType, out);
   out << "    store " << lt << " " << valReg << ", " << lt << "* %" << decl->name << "\n";
}

void CodeGen::genArrayDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<ArrayDeclNode*>(node);
   std::string lt = llvmType(decl->elementType);
   int resolvedSize = decl->size >= 0 ? decl->size : (int)decl->initializers.size();
   std::string arrType = "[" + std::to_string(resolvedSize) + " x " + lt + "]";
   
   out << "    %" << decl->name << " = alloca " << arrType << "\n";
   if (decl->initializers.empty()) return;

   for (size_t i = 0; i < decl->initializers.size(); ++i) {
       std::string valReg = genExpression(decl->initializers[i].get(), decl->elementType, out);
       std::string ptrReg = newReg();
       out << "    " << ptrReg << " = getelementptr " << arrType << ", " << arrType << "* %" << decl->name << ", i32 0, i32 " << i << "\n";
       out << "    store " << lt << " " << valReg << ", " << lt << "* " << ptrReg << "\n";
   }
}