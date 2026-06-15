#include "codegen.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/expressions.h"
#include "../parser/ast/controlflow.h"
#include "../parser/ast/blocknode.h"
#include "../parser/ast/functions.h"
#include "../sema/symboltable.h"
#include <fstream>
#include <iomanip>
#include <functional>

CodeGen::CodeGen(ASTNode* root, SymbolTable& symTable)
   : root(root), symTable(symTable),regCounter(0), strCounter(0) {}

void CodeGen::generate(const std::string& outputPath) {
    auto* program = static_cast<ProgramNode*>(root);
    
    std::ostringstream globalsOut;
    globalsOut << "declare double @llvm.pow.f64(double, double)\n";

    for (auto& node : program->globals) {
        if (node->type == NodeType::FUNC_DEF) {
            auto* func = static_cast<FunctionDefNode*>(node.get());
            CodeGenFunctionSignature sig;
            sig.returnType = func->returnType;
            for (auto& p : func->parameters) sig.paramTypes.push_back(p.first);
            functions[func->name] = sig;
            
            genFuncDef(node.get(), globalsOut);
        }
    }

    for (auto& node : program->globals) {
        if (node->type == NodeType::VAR_DECL) {
            auto* decl = static_cast<VarDeclNode*>(node.get());
            std::string lt = llvmType(decl->varType);
            std::string initVal = extractConstant(decl->value.get());
            
            symTable.get(decl->name).llvmRegister = "@" + decl->name;
            symTable.get(decl->name).llvmAllocType = lt;             
            globalsOut << "@" << decl->name << " = global " << lt << " " << initVal << "\n";
        }
        else if (node->type == NodeType::ARRAY_DECL) {
            auto* decl = static_cast<ArrayDeclNode*>(node.get());
            symTable.get(decl->name).llvmRegister = "@" + decl->name;
            std::string lt = llvmType(decl->elementType);
            
            int totalSize = 1;
            for (size_t i = 0; i < decl->dimensions.size(); ++i) {
                totalSize *= decl->dimensions[i];
            }
            std::string arrType = lt;
            for (int i = decl->dimensions.size() - 1; i >= 0; --i) {
                arrType = "[" + std::to_string(decl->dimensions[i]) + " x " + arrType + "]";
            }
            symTable.get(decl->name).llvmAllocType = arrType;
            std::string initVal;
            if (decl->initializers.empty()) {
                initVal = "zeroinitializer";
            } else {
                std::function<std::string(int, int)> buildNestedInit = 
                [&](int dimIndex, int flatOffset) -> std::string {
                    if (dimIndex == decl->dimensions.size() - 1) {
                        std::string res = "[";
                        int size = decl->dimensions[dimIndex];
                        for (int i = 0; i < size; ++i) {
                            int idx = flatOffset + i;
                            std::string val = (idx < (int)decl->initializers.size()) ? extractConstant(decl->initializers[idx].get()) : "0";
                            res += lt + " " + val;
                            if (i + 1 < size) res += ", ";
                        }
                        res += "]";
                        return res;
                    }

                    std::string res = "[";
                    int size = decl->dimensions[dimIndex];
                    int childFlatSize = 1;
                    for (size_t i = dimIndex + 1; i < decl->dimensions.size(); ++i) {
                        childFlatSize *= decl->dimensions[i];
                    }
                    std::string childType = lt;
                    for (int i = decl->dimensions.size() - 1; i > dimIndex; --i) {
                        childType = "[" + std::to_string(decl->dimensions[i]) + " x " + childType + "]";
                    }

                    for (int i = 0; i < size; ++i) {
                        res += childType + " " + buildNestedInit(dimIndex + 1, flatOffset + (i * childFlatSize));
                        if (i + 1 < size) res += ", ";
                    }
                    res += "]";
                    return res;
                };

                initVal = buildNestedInit(0, 0);
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
            genStatement(node.get(), body);
        }
        symTable.popScope();
    }
    body << "    br label %main_end\n";
    body << "\nmain_end:\n";
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

void CodeGen::genStatement(ASTNode* stmt, std::ostream& out) {
    if (!stmt) return;
    
    if (stmt->type == NodeType::VAR_DECL) genVarDecl(stmt, out);
    else if (stmt->type == NodeType::ARRAY_DECL) genArrayDecl(stmt, out);
    else if (stmt->type == NodeType::ASSIGNMENT_EXPR || stmt->type == NodeType::UNARY_EXPR ||stmt->type == NodeType::CALL_EXPR) genExpression(stmt, "any", out);
    else if (stmt->type == NodeType::IF_STMT) genIfStmt(stmt, out);
    else if (stmt->type == NodeType::WHILE_STMT) genWhileStmt(stmt, out);
    else if (stmt->type == NodeType::DOWHILE_STMT) genDoWhileStmt(stmt, out);
    else if (stmt->type == NodeType::SWITCH_STMT) genSwitchStmt(stmt, out);
    else if (stmt->type == NodeType::BREAK_STMT) genBreakStmt(stmt, out);
    else if (stmt->type == NodeType::BLOCK) genBlock(stmt, out);
    else if (stmt->type == NodeType::PASS_STMT) {}
    else if (stmt->type == NodeType::CONTINUE_STMT) genContinueStmt(stmt, out);
    else if (stmt->type == NodeType::RETURN_STMT) genReturnStmt(stmt, out);
    else if (stmt->type == NodeType::FOR_STMT) genForStmt(stmt, out);
    else if (stmt->type == NodeType::FOR_IN_STMT) genForInStmt(stmt, out);
}