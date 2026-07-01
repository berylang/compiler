#include "sema.h"

/*

    Semantic Analyzer, Declarations,

    this file analyze - variable declarations, array declarations, function declarations, enum declarations


*/

#include "../parser/ast/vardecl.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/functions.h"
#include "../parser/ast/classes.h"
#include <iostream>
#include <unordered_set>

void SemanticAnalyzer::analyzeVarDecl(ASTNode* node) {
    auto* decl = static_cast<VarDeclNode*>(node);
    static const std::unordered_set<std::string> primitiveTypes = {
        "int", "bigint", "bool", "float", "double", "char", "string"
    };
    if (!primitiveTypes.count(decl->varType) && !classes.count(decl->varType)) {
        std::cerr << "Bery:Error [Line " << decl->line << "]: Unknown type '" << decl->varType << "'\n";
        errors = true;
        return;
    }
    if (symbolTable.existsInCurrentScope(decl->name)) {
        std::cerr << "Bery:Error [Line "<< decl->line<< "]: '" << decl->name << "' already declared in this scope.\n";
        errors = true;
        return;
    }
    if (decl->isConst && !decl->value) {
        std::cerr << "Bery:Error [Line "<< decl->line <<"]: constant '" << decl->name << "' must be initialized.\n";
        errors = true;
        return;
    }
    if (decl->value) {
    std::string exprtype = typeChecker.analyzeExpression(decl->value.get());

        if(exprtype!="unknown" && exprtype!=decl->varType){
            if (exprtype == "null") {
                if (decl->varType != "string") {
                    std::cerr << "Bery:Error [Line "<< decl->line << "]: Cannot assign 'null' to non-reference type '" << decl->varType << "'\n";
                    errors = true;
                    return;
                }
            }
            else if(!(decl->varType == "float" && exprtype == "int")&&
            !(decl->varType == "double" && exprtype == "int") &&
            !(decl->varType == "bigint" && exprtype == "int")&&
            !(decl->varType == "double" && exprtype == "float")&&
            !(decl->varType == "float" && exprtype == "double")){
                std::cerr<<"Bery:Error [Line " << decl->line << "]: Type mismatch for "<<decl->name<<" . Expected '"<<decl->varType<<"', got '"<<exprtype<<"' \n";
                errors = true;
                return;
            }
        }
    }
    symbolTable.addVariable(decl->name, decl->varType, decl->isConst, decl->value != nullptr, decl->line);
}

void SemanticAnalyzer::analyzeArrayDecl(ASTNode* node) {
    auto* decl = static_cast<ArrayDeclNode*>(node);

    if (symbolTable.existsInCurrentScope(decl->name)) {
        std::cerr << "Bery:Error [Line "<< decl->line <<"]: '" << decl->name << "' already declared.\n";
        errors = true; return;
    }
    if (decl->dimensions.size() == 1 && decl->dimensions[0] == -1 && decl->initializers.empty()) {
        symbolTable.addVariable(decl->name, "array<" + decl->elementType + ">", false, false, decl->line);
        return;
    }
    int totalSize = 1;
    int inferredDim = -1;
    for (size_t i = 0; i < decl->dimensions.size(); ++i) {
        if (decl->dimensions[i] < 0) {
            if (i != 0) {
                std::cerr << "Bery:Error [Line "<< decl->line <<"]: Only the first dimension can be omitted.\n";
                errors = true; return;
            }
            inferredDim = i;
        } else if (decl->dimensions[i] == 0) {
            std::cerr << "Bery:Error [Line "<< decl->line <<"]: Dimensions must be greater than 0.\n";
            errors = true; return;
        } else {
            totalSize *= decl->dimensions[i];
        }
    }
    if (inferredDim != -1) {
        if (decl->initializers.empty()) {
            std::cerr << "Bery:Error [Line "<< decl->line <<"]: Must have an initializer list if dimension is omitted.\n";
            errors = true; return;
        }
        if (decl->initializers.size() % totalSize != 0) {
            std::cerr << "Bery:Error [Line "<< decl->line <<"]: Initializer list size does not match multi-dimensional bounds.\n";
            errors = true; return;
        }
        decl->dimensions[0] = decl->initializers.size() / totalSize;
        totalSize *= decl->dimensions[0];
    }

    if (!decl->initializers.empty() && decl->initializers.size() > (size_t)totalSize) {
        std::cerr << "Bery:Error [Line "<< decl->line <<"]: initializer list count exceeds array size.\n";
        errors = true; return;
    }

    for (auto& initVal : decl->initializers) {
        std::string exprType = typeChecker.analyzeExpression(initVal.get());
        if (exprType != "unknown" && exprType != decl->elementType) {
            if (!(decl->elementType == "float" && exprType == "int") &&
                !(decl->elementType == "double" && exprType == "int")) {
                std::cerr << "Bery:Error [Line "<< decl->line <<"]: Type mismatch in array initialization.\n";
                errors = true; return;
            }
        }
    }
    std::string typeSignature = decl->elementType;
    for (size_t i = 0; i < decl->dimensions.size(); ++i) typeSignature += "[]";
    symbolTable.addVariable(decl->name, typeSignature, false, !decl->initializers.empty(), decl->line, decl->dimensions);
}

void SemanticAnalyzer::analyzeFuncDef(ASTNode* node) {
    auto* func = static_cast<FunctionDefNode*>(node);
    currentFunctionReturnType = func->returnType;
    
    symbolTable.pushScope();
    for (auto& param : func->parameters) {
        symbolTable.addVariable(param.second, param.first, false, true, func->line);
    }
    
    for (auto& stmt : func->body->statements) 
        analyzeNode(stmt.get());
    
    symbolTable.popScope();
    currentFunctionReturnType = "";
}

void SemanticAnalyzer::analyzeReturnStmt(ASTNode* node) {
    auto* ret = static_cast<ReturnStmtNode*>(node);
    if (currentFunctionReturnType == "") {
        std::cerr << "Bery:Error [Line " << ret->line << "]: 'return' used outside of a function.\n";
        errors = true; 
        return;
    }
    
    if (!ret->value) {
        if (currentFunctionReturnType != "void") {
            std::cerr << "Bery:Error [Line " << ret->line << "]: Expected return value of type '" << currentFunctionReturnType << "'\n";
            errors = true;
        }
    } else {
        std::string valType = typeChecker.analyzeExpression(ret->value.get());
        if (valType != "unknown" && valType != currentFunctionReturnType) {
             if (!(currentFunctionReturnType == "float" && valType == "int") &&
                 !(currentFunctionReturnType == "double" && valType == "float") &&
                 !(currentFunctionReturnType == "double" && valType == "int") &&
                 !(currentFunctionReturnType == "bigint" && valType == "int")) {
                 std::cerr << "Bery:Error [Line " << ret->line << "]: Type mismatch in return. Expected '" << currentFunctionReturnType << "', got '" << valType << "'\n";
                 errors = true;
             }
        }
    }
}

void SemanticAnalyzer::analyzeEnumDecl(ASTNode* node) {
    auto* enumDecl = static_cast<EnumDeclNode*>(node);
    for (const auto& val : enumDecl->values) {
        std::string mangledName = enumDecl->name + "." + val; 
        if (symbolTable.existsInCurrentScope(mangledName)) {
            std::cerr << "Bery:Error [Line " << enumDecl->line << "]: Enum value '" << mangledName << "' already declared.\n";
            errors = true;
        } else {
            symbolTable.addVariable(mangledName, "int", true, true, enumDecl->line);
        }
    }
}

void SemanticAnalyzer::analyzeClassDecl(ASTNode* node) {
    auto* cls = static_cast<ClassDefNode*>(node);

    std::unordered_set<std::string> seen;
    if (cls->attributes) {
        for (auto& attr : cls->attributes->attributes) {
            auto* field = static_cast<VarDeclNode*>(attr.get());
            if (seen.count(field->name)) {
                std::cerr << "Bery:Error [Line " << field->line << "]: Duplicate field '"
                          << field->name << "' in class '" << cls->name << "'\n";
                errors = true;
            }
            seen.insert(field->name);
        }
    }

    if (cls->methods) {
        for (auto& m : cls->methods->methods) {
            auto* func = static_cast<FunctionDefNode*>(m.get());
            FunctionSignature sig;
            sig.returnType = func->returnType;
            for (auto& p : func->parameters) sig.paramTypes.push_back(p.first);
            functions[cls->name + "." + func->name] = sig;

            currentFunctionReturnType = func->returnType;
            symbolTable.pushScope();
            if (cls->attributes) {
                symbolTable.addVariable(cls->attributes->selfRef, cls->name, false, true, cls->line);
                for (auto& attr : cls->attributes->attributes) {
                    auto* field = static_cast<VarDeclNode*>(attr.get());
                    symbolTable.addVariable(field->name, field->varType, false,true, field->line);
                }
            }

            for (auto& p : func->parameters)
                symbolTable.addVariable(p.second,p.first,false, true,func->line);

            for (auto& stmt : func->body->statements)
                analyzeNode(stmt.get());

            symbolTable.popScope();
            currentFunctionReturnType = "";
        }
    }
}