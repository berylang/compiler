#include "../codegen.h"
#include <string>
#include <iomanip>
#include "../../parser/ast/literals.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/vardecl.h"


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
    else if (stmt->type == NodeType::ENUM_DECL) {
        auto* enumDecl = static_cast<EnumDeclNode*>(stmt);
        int currentValue = 0;
        
        for (const auto& val : enumDecl->values) {
            std::string mangledName = enumDecl->name + "." + val;
            std::string lt = "i32";
            std::string safeRegName = enumDecl->name + "_" + val; 
            std::string memReg = "%" + safeRegName + "_" + std::to_string(++regCounter);
            
            Symbol sym;
            sym.symbolType = SymbolType::VARIABLE;
            sym.type = "int";
            sym.isConst = true;
            sym.isInitialized = true;
            sym.line = enumDecl->line;
            sym.llvmRegister = memReg;
            sym.llvmAllocType = lt;
            symTable.add(mangledName, sym);
            out << "    " << memReg << " = alloca " << lt << "\n";
            out << "    store " << lt << " " << currentValue++ << ", " << lt << "* " << memReg << "\n";
        }
    }
    else if (stmt->type == NodeType::FOR_STMT) genForStmt(stmt, out);
    else if (stmt->type == NodeType::FOR_IN_STMT) genForInStmt(stmt, out);
}

int CodeGen::alignOf(const std::string& llvmT) {
    if (llvmT == "i1" || llvmT == "i8")return 1;
    if (llvmT == "i32" || llvmT == "float") return 4;
    if (llvmT == "i64" || llvmT == "double" || llvmT == "i8*") return 8;
    return 4;
}

std::string CodeGen::emitAlloca(const std::string& llvmT, std::ostream& out) {
    std::string reg = newReg();
    out << "    " << reg << " = alloca " << llvmT << ", align " << alignOf(llvmT) << "\n";
    return reg;
}

void CodeGen::emitStore(const std::string& llvmT, const std::string& val, const std::string& ptr, std::ostream& out) {
    out << "    store " << llvmT << " " << val << ", " << llvmT << "* " << ptr << ", align " << alignOf(llvmT) << "\n";
}

std::string CodeGen::emitLoad(const std::string& llvmT, const std::string& ptr, std::ostream& out) {
    std::string reg = newReg();
    out << "    " << reg << " = load " << llvmT << ", " << llvmT << "* " << ptr << ", align " << alignOf(llvmT) << "\n";
    return reg;
}

std::string CodeGen::emitSext(const std::string& fromT, const std::string& val, const std::string& toT, std::ostream& out) {
    std::string reg = newReg();
    out << "    " << reg << " = sext " << fromT << " " << val << " to " << toT << "\n";
    return reg;
}

std::string CodeGen::emitBoxValue(const std::string& llvmT, const std::string& valReg, std::ostream& out) {
    std::string slotReg = emitAlloca(llvmT, out);
    emitStore(llvmT, valReg, slotReg, out);
    std::string castReg = newReg();
    out << "    " << castReg << " = bitcast " << llvmT << "* " << slotReg << " to i8*\n";
    return castReg;
}

