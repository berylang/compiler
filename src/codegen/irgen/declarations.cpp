#include "../codegen.h"
#include "../../parser/ast/vardecl.h"
#include "../../parser/ast/arraydeclare.h"
#include "../../parser/ast/functions.h"


void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<VarDeclNode*>(node);
   std::string lt = llvmType(decl->varType);
   std::string memReg = "%" + decl->name + "_" + std::to_string(++regCounter);
   symTable.add(decl->name, {decl->varType, decl->isConst, decl->value != nullptr, decl->line, memReg, lt});
   out << "    " << memReg << " = alloca " << lt << "\n";
   if (decl->varType == "string") {
        emitGCPush(memReg, lt, out);
    }
   if (!decl->value) return;
   std::string valReg = genExpression(decl->value.get(), decl->varType, out);
   out << "    store " << lt << " " << valReg << ", " << lt << "* " << memReg << "\n";
}

void CodeGen::genArrayDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<ArrayDeclNode*>(node);
   if (decl->dimensions.size() == 1 && decl->dimensions[0] == -1) {
        emitBREDecl("declare i8* @bery_array_new(i64)", "bery_array_new");
        std::string memReg = "%" + decl->name + "_" + std::to_string(++regCounter);
        symTable.add(decl->name, {"array<" + decl->elementType + ">", false, true, decl->line, memReg, "i8*"});
        out << "    " << memReg << " = alloca i8*\n";
        std::string arrReg = newReg();
        out << "    " << arrReg << " = call i8* @bery_array_new(i64 4)\n";
        out << "    store i8* " << arrReg << ", i8** " << memReg << "\n";
        emitGCPush(memReg, "i8*", out);
        return;
    }
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

void CodeGen::genFuncDef(ASTNode* node, std::ostream& out) {
    auto* func = static_cast<FunctionDefNode*>(node);
    std::string retLT = (func->returnType == "void") ? "void" : llvmType(func->returnType);
    currentFuncReturn = func->returnType;
    
    out << "\ndefine " << retLT << " @" << func->name << "(";
    for (size_t i = 0; i < func->parameters.size(); ++i) {
        out << llvmType(func->parameters[i].first) << " %" << func->parameters[i].second << "_arg";
        if (i + 1 < func->parameters.size()) out << ", ";
    }
    out << ") {\nentry:\n";
    
    symTable.pushScope();
    pushGCScope();
    for (auto& param : func->parameters) {
        std::string pType = llvmType(param.first);
        std::string pName = param.second;
        std::string memReg = "%" + pName + "_" + std::to_string(++regCounter);
        
        symTable.add(pName, {param.first, false, true, func->line, memReg, pType});
        
        out << "    " << memReg << " = alloca " << pType << "\n";
        out << "    store " << pType << " %" << pName << "_arg, " << pType << "* " << memReg << "\n";
    }
    
    for (auto& stmt : func->body->statements) genStatement(stmt.get(), out);
    int roots = popGCScope();
    emitGCPops(roots, out);
    symTable.popScope();
    bool endsWithReturn = false;
    if (!func->body->statements.empty() && 
        func->body->statements.back()->type == NodeType::RETURN_STMT) {
        endsWithReturn = true;
    }
    
    if (!endsWithReturn) {
        if (func->returnType == "void") out << "    ret void\n";
        else out << "    ret " << retLT << " 0\n"; 
    }
    
    out << "}\n";
    currentFuncReturn = "";
}

void CodeGen::genReturnStmt(ASTNode* node, std::ostream& out) {
    auto* retNode = static_cast<ReturnStmtNode*>(node);
    if (!retNode->value) {
        out << "    ret void\n";
    } else {
        std::string valReg = genExpression(retNode->value.get(), currentFuncReturn, out);
        out << "    ret " << llvmType(currentFuncReturn) << " " << valReg << "\n";
    }
}