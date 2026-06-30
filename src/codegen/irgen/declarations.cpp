#include "../codegen.h"
#include "../../parser/ast/vardecl.h"
#include "../../parser/ast/arraydeclare.h"
#include "../../parser/ast/classes.h"
#include "../../parser/ast/functions.h"
#include "../../parser/ast/classes.h"

void CodeGen::genClassDecl(ASTNode* node) {
    auto* cls = static_cast<ClassDefNode*>(node);

    ClassLayout layout;
    layout.name = cls->name;
    layout.llvmStructType = "%class." + cls->name;
    std::ostringstream structDef;
    structDef << layout.llvmStructType << " = type { ";

    if (cls->attributes) {
        for (size_t i = 0; i < cls->attributes->attributes.size(); ++i) {
            auto* field = static_cast<VarDeclNode*>(cls->attributes->attributes[i].get());
            std::string lt = llvmType(field->varType);
            layout.fields.push_back({field->varType, field->name});
            layout.fieldIndex[field->name] = (int)i;
            structDef << lt;
            if (i + 1 < cls->attributes->attributes.size()) structDef << ", ";
        }
    }

    if (layout.fields.empty()) structDef << "i8";
    structDef << " }";

    globalStrings << structDef.str() << "\n";
    if (cls->methods) {
        for (auto& m : cls->methods->methods) {
            auto* func = static_cast<FunctionDefNode*>(m.get());
            std::string mangledName = cls->name + "_" + func->name;

            CodeGenFunctionSignature sig;
            sig.returnType = func->returnType;
            sig.paramTypes.push_back(cls->name + "*");
            for (auto& p : func->parameters) sig.paramTypes.push_back(p.first);
            functions[mangledName] = sig;

            std::ostringstream methodOut;
            std::string retLT = (func->returnType == "void" || func->returnType.empty())
                                 ? "void" : llvmType(func->returnType);
            currentFuncReturn = func->returnType;

            methodOut << "\ndefine " << retLT << " @" << mangledName
                      << "(" << layout.llvmStructType << "* %self_arg";
            for (auto& p : func->parameters)
                methodOut << ", " << llvmType(p.first) << " %" << p.second << "_arg";
            methodOut << ") {\nentry:\n";

            symTable.pushScope();
            pushGCScope();

            std::string selfReg = "%self_" + std::to_string(++regCounter);
            methodOut << "    " << selfReg << " = alloca " << layout.llvmStructType << "*\n";
            methodOut << "    store " << layout.llvmStructType << "* %self_arg, "
                      << layout.llvmStructType << "** " << selfReg << "\n";
            symTable.add(cls->attributes->selfRef,
            Symbol{
                .name = cls->attributes->selfRef,
                .type = cls->name,
                .isConst = false,
                .isInitialized = true,
                .llvmRegister = selfReg,
                .llvmAllocType = layout.llvmStructType + "*",
                .line = cls->line
            });

            std::string loadedSelf = newReg();
            methodOut << "    " << loadedSelf << " = load " << layout.llvmStructType << "*, "
                      << layout.llvmStructType << "** " << selfReg << "\n";

            for (auto& [berryT, fieldName] : layout.fields) {
                std::string lt = llvmType(berryT);
                int idx = layout.fieldIndex[fieldName];
                std::string gepReg = "%" + fieldName + "_" + std::to_string(++regCounter);
                methodOut << "    " << gepReg << " = getelementptr inbounds "
                          << layout.llvmStructType << ", " << layout.llvmStructType << "* "
                          << loadedSelf << ", i32 0, i32 " << idx << "\n";
                symTable.add(fieldName,
                Symbol{
                    .name = fieldName,
                    .type = berryT,
                    .isConst = false,
                    .isInitialized = true,
                    .llvmRegister = gepReg,
                    .llvmAllocType = lt,
                    .line = cls->line
                });
            }

            for (auto& p : func->parameters) {
                std::string pLT  = llvmType(p.first);
                std::string pReg = "%" + p.second + "_" + std::to_string(++regCounter);
                symTable.add(p.second,
                Symbol{
                    .name = p.second,
                    .type = p.first,
                    .isConst = false,
                    .isInitialized = true,
                    .llvmRegister = pReg,
                    .llvmAllocType = pLT,
                    .line = func->line
                });
                methodOut << "    " << pReg << " = alloca " << pLT << "\n";
                methodOut << "    store " << pLT << " %" << p.second << "_arg, "
                          << pLT << "* " << pReg << "\n";
            }

            for (auto& stmt : func->body->statements)
                genStatement(stmt.get(), methodOut);

            int roots = popGCScope();
            emitGCPops(roots, methodOut);
            symTable.popScope();

            bool endsWithReturn = !func->body->statements.empty() &&
                func->body->statements.back()->type == NodeType::RETURN_STMT;
            if (!endsWithReturn) {
                if (func->returnType == "void" || func->returnType.empty())
                    methodOut << "    ret void\n";
                else
                    methodOut << "    ret " << retLT << " 0\n";
            }
            methodOut << "}\n";
            globalStrings << methodOut.str();
            currentFuncReturn = "";
        }
    }

    classLayouts[cls->name] = std::move(layout);
}

void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
    auto* decl = static_cast<VarDeclNode*>(node);
    std::string lt = llvmType(decl->varType);
    std::string memReg = "%" + decl->name + "_" + std::to_string(++regCounter);
    Symbol sym;
    sym.symbolType = SymbolType::VARIABLE;
    sym.type = decl->varType;
    sym.isConst = decl->isConst;
    sym.isInitialized = decl->value != nullptr;
    sym.line = decl->line;
    sym.llvmRegister = memReg;
    sym.llvmAllocType = lt;
    symTable.add(decl->name, sym);
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
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = "array<" + decl->elementType + ">";
        sym.isInitialized = true;
        sym.line = decl->line;
        sym.llvmRegister = memReg;
        sym.llvmAllocType = "i8*";
        symTable.add(decl->name, sym);
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
   
   Symbol sym;
   sym.symbolType = SymbolType::VARIABLE;
   sym.type = typeSig;
   sym.isInitialized = !decl->initializers.empty();
   sym.line = decl->line;
   sym.llvmRegister = memReg;
   sym.llvmAllocType = arrType;
   sym.arrayDimensions = decl->dimensions;
   sym.arraySize = 1;
   for (int d : decl->dimensions) sym.arraySize *= d;
   symTable.add(decl->name, sym);

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
        
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = param.first;
        sym.isInitialized = true;
        sym.line = func->line;
        sym.llvmRegister = memReg;
        sym.llvmAllocType = pType;
        symTable.add(pName, sym);
        
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