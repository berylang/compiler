#include "../codegen.h"
#include "../../parser/ast/vardecl.h"
#include "../../parser/ast/arraydeclare.h"


void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<VarDeclNode*>(node);
   std::string lt = llvmType(decl->varType);
   std::string memReg = "%" + decl->name + "_" + std::to_string(++regCounter);
   symTable.add(decl->name, {decl->varType, decl->isConst, decl->value != nullptr, decl->line, memReg, lt});
   out << "    " << memReg << " = alloca " << lt << "\n";
   if (!decl->value) return;
   std::string valReg = genExpression(decl->value.get(), decl->varType, out);
   out << "    store " << lt << " " << valReg << ", " << lt << "* " << memReg << "\n";
}

void CodeGen::genArrayDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<ArrayDeclNode*>(node);
   std::string lt = llvmType(decl->elementType);

   std::string arrType = lt;
   for (int i = decl->dimensions.size() - 1; i >= 0; --i) {
      arrType = "[" + std::to_string(decl->dimensions[i]) + " x " + arrType + "]";
   }
   
   std::string memReg = "%" + decl->name + "_" + std::to_string(++regCounter);
   std::string typeSig = decl->elementType;
   for (size_t i = 0; i < decl->dimensions.size(); ++i) typeSig += "[]";
   
   symTable.add(decl->name, {typeSig, false, !decl->initializers.empty(), decl->line, memReg, arrType});

   out << "    " << memReg << " = alloca " << arrType << "\n";
   if (decl->initializers.empty()) return;
   std::string flatPtr = newReg();
   out << "    " << flatPtr << " = bitcast " << arrType << "* " << memReg << " to " << lt << "*\n";

   for (size_t i = 0; i < decl->initializers.size(); ++i) {
      std::string valReg = genExpression(decl->initializers[i].get(), decl->elementType, out);
      std::string ptrReg = newReg();
      out << "    " << ptrReg << " = getelementptr " << lt << ", " << lt << "* " << flatPtr << ", i32 " << i << "\n";
      out << "    store " << lt << " " << valReg << ", " << lt << "* " << ptrReg << "\n";
   }
}