#include "codegen.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/expressions.h"
#include "../sema/symboltable.h"
#include <fstream>
#include <iomanip>

CodeGen::CodeGen(ASTNode* root, SymbolTable& symTable)
   : root(root), symTable(symTable),regCounter(0), strCounter(0) {}


void CodeGen::generate(const std::string& outputPath) {
    auto* program = static_cast<ProgramNode*>(root);
    
    std::ostringstream globalsOut;
    globalsOut << "declare double @llvm.pow.f64(double, double)\n";

    for (auto& node : program->globals) {
        if (node->type == NodeType::VAR_DECL) {
            auto* decl = static_cast<VarDeclNode*>(node.get());
            std::string lt = llvmType(decl->varType);
            std::string initVal = extractConstant(decl->value.get());
            
            symTable.get(decl->name).llvmRegister = "@" + decl->name;
            globalsOut << "@" << decl->name << " = global " << lt << " " << initVal << "\n";
        } 
        else if (node->type == NodeType::ARRAY_DECL) {
            auto* decl = static_cast<ArrayDeclNode*>(node.get());
            symTable.get(decl->name).llvmRegister = "@" + decl->name;
            std::string lt = llvmType(decl->elementType);
            int resolvedSize = decl->size >= 0 ? decl->size : (int)decl->initializers.size();
            std::string arrType = "[" + std::to_string(resolvedSize) + " x " + lt + "]";
            
            std::string initVal;
            if (decl->initializers.empty()) {
                initVal = "zeroinitializer";
            } else {
                initVal = "[";
                for (size_t i = 0; i < decl->initializers.size(); ++i) {
                    std::string elemVal = extractConstant(decl->initializers[i].get());
                    initVal += lt + " " + elemVal;
                    if (i + 1 < decl->initializers.size()) {
                        initVal += ", ";
                    }
                }
                if (decl->initializers.size() < (size_t)resolvedSize) {
                    for (size_t i = decl->initializers.size(); i < (size_t)resolvedSize; ++i) {
                        initVal += ", " + lt + " 0";
                    }
                }
                initVal += "]";
            }
            globalsOut << "@" << decl->name << " = global " << arrType << " " << initVal << "\n";
        }
    }

    std::ostringstream body;
    body << "\ndefine i32 @main() {\n";
    body << "entry:\n";

    if (program->runBlock) {
        symTable.pushScope();
        for (auto& node : program->runBlock->statements) {
            if (node->type == NodeType::VAR_DECL)
                genVarDecl(node.get(), body);
            else if (node->type == NodeType::ARRAY_DECL)
                genArrayDecl(node.get(), body);
            else if (node->type == NodeType::ASSIGNMENT_EXPR) 
                genExpression(node.get(), "any", body); // Make sure this router is here!
        }
        symTable.popScope();
    }
    body << "    ret i32 0\n";
    body << "}\n";

    std::ofstream out(outputPath);
    out << globalsOut.str();
    out << globalStrings.str() << "\n";
    out << body.str();
}


std::string CodeGen::llvmType(const std::string& t) {
   if (t == "int") return "i32";
   if (t == "bigint") return "i64";
   if (t == "bool") return "i1";
   if (t == "float") return "float";
   if (t == "double") return "double";
   if (t == "char") return "i8";
   if (t == "string") return "i8*";
   return "i32";
}

std::string CodeGen::escapeLLVMString(const std::string& str) {
    std::ostringstream escaped;

    for (char c : str) {
        if (c == '\n') escaped << "\\0A";
        else if (c == '\t') escaped << "\\09";
        else if (c == '\r') escaped << "\\0D";
        else if (c == '\\') escaped << "\\5C";
        else if (c == '"') escaped << "\\22";
        else if (c < 32 || c > 126) {
            escaped << "\\" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)(unsigned char) c;
        } else {
            escaped << c;
        }
    }
    
    escaped << "\\00";
    return escaped.str();
}

std::string CodeGen::extractConstant(ASTNode* node) {
    if (!node) return "0";
    if (node->type == NodeType::INT_LIT) {
        return std::to_string(static_cast<IntLitNode*>(node)->value);
    } else if (node->type == NodeType::DECIMAL_LIT) {
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(17) << static_cast<DecimalLitNode*>(node)->value;
    return ss.str();
}
    else if (node->type == NodeType::BOOL_LIT) {
        return static_cast<BoolLitNode*>(node)->value ? "1" : "0";
    } 
    else if (node->type == NodeType::CHAR_LIT) {
        return std::to_string(static_cast<CharLitNode*>(node)->value);
    }else if (node->type == NodeType::STRING_LIT) {
        auto* lit = static_cast<StringLitNode*>(node);
        std::string strVal = lit->value;
        std::string escapedStr = escapeLLVMString(strVal);
        int len = strVal.length() + 1;
        std::string globalName = "@.str." + std::to_string(strCounter++);
        globalStrings << globalName << " = private unnamed_addr constant [" << len << " x i8] c\"" << escapedStr << "\"\n";
        return "getelementptr ([" + std::to_string(len) + " x i8], [" + std::to_string(len) + " x i8]* " + globalName + ", i32 0, i32 0)";
    }
    else if (node->type == NodeType::NULL_LIT) {
        return "null";
    }
    else if (node->type == NodeType::UNARY_EXPR) {
        auto* unary = static_cast<UnaryExprNode*>(node);
        if (unary->optr == "-") {
            return "-" + extractConstant(unary->operand.get());
        }
    }
    return "0";
}
std::string CodeGen::newReg() {
   return "%" + std::to_string(++regCounter);
}