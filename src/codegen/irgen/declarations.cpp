#include "../codegen.h"
#include "../../parser/ast/vardecl.h"
#include "../../parser/ast/arraydeclare.h"


void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<VarDeclNode*>(node);
   std::string lt = llvmType(decl->varType);
   std::string memReg = "%" + decl->name + "_" + std::to_string(++regCounter);
   symTable.add(decl->name, {decl->varType, decl->isConst, decl->value != nullptr, decl->line, memReg});
   out << "    " << memReg << " = alloca " << lt << "\n";
   if (!decl->value) return;
   std::string valReg = genExpression(decl->value.get(), decl->varType, out);
   out << "    store " << lt << " " << valReg << ", " << lt << "* " << memReg << "\n";
}

void CodeGen::genArrayDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<ArrayDeclNode*>(node);
   std::string lt = llvmType(decl->elementType);
   int resolvedSize = decl->size >= 0 ? decl->size : (int)decl->initializers.size();
   std::string arrType = "[" + std::to_string(resolvedSize) + " x " + lt + "]";
   
   std::string memReg = "%" + decl->name + "_" + std::to_string(++regCounter);
   symTable.add(decl->name, {decl->elementType + "[]", false, !decl->initializers.empty(), decl->line, memReg});

   out << "    " << memReg << " = alloca " << arrType << "\n";
   if (decl->initializers.empty()) return;

   for (size_t i = 0; i < decl->initializers.size(); ++i) {
       std::string valReg = genExpression(decl->initializers[i].get(), decl->elementType, out);
       std::string ptrReg = newReg();
       out << "    " << ptrReg << " = getelementptr " << arrType << ", " << arrType << "* " << memReg << ", i32 0, i32 " << i << "\n";
       out << "    store " << lt << " " << valReg << ", " << lt << "* " << ptrReg << "\n";
   }
}