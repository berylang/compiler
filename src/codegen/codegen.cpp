#include "codegen.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/expressions.h"
#include "../parser/ast/controlflow.h"
#include "../parser/ast/blocknode.h"
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
            if (node->type == NodeType::VAR_DECL) genVarDecl(node.get(), body);
            else if (node->type == NodeType::ARRAY_DECL) genArrayDecl(node.get(), body);
            else if (node->type == NodeType::ASSIGNMENT_EXPR) genExpression(node.get(), "any", body);
            else if (node->type == NodeType::IF_STMT) genIfStmt(node.get(), body);
            else if (node->type == NodeType::BLOCK) genBlock(node.get(), body);
            else if (node->type == NodeType::WHILE_STMT) genWhileStmt(node.get(), body);
            else if (node->type == NodeType::SWITCH_STMT) genSwitchStmt(node.get(), body);
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


void CodeGen::genBlock(ASTNode* node, std::ostream& out) {
    auto* block = static_cast<BlockNode*>(node);
    symTable.pushScope(); 
    for (auto& stmt : block->statements) {
        if (stmt->type == NodeType::VAR_DECL) genVarDecl(stmt.get(), out);
        else if (stmt->type == NodeType::ARRAY_DECL) genArrayDecl(stmt.get(), out);
        else if (stmt->type == NodeType::ASSIGNMENT_EXPR) genExpression(stmt.get(), "any", out);
        else if (stmt->type == NodeType::IF_STMT) genIfStmt(stmt.get(), out);
        else if (stmt->type == NodeType::BLOCK) genBlock(stmt.get(), out);
        else if (stmt->type == NodeType::WHILE_STMT) genWhileStmt(stmt.get(), out);
        else if (stmt->type == NodeType::SWITCH_STMT) genSwitchStmt(stmt.get(), out);
    }
    symTable.popScope();
}

void CodeGen::genIfStmt(ASTNode* node, std::ostream& out) {
    auto* ifStmt = static_cast<IfStmtNode*>(node);
    std::string condReg = genExpression(ifStmt->conditions.get(), "bool", out);

    int blockId = ++regCounter;
    std::string thenLbl = "if_then_" + std::to_string(blockId);
    std::string elseLbl = "if_else_" + std::to_string(blockId);
    std::string endLbl = "if_end_" + std::to_string(blockId);

    if (ifStmt->elseBranch) out << "    br i1 " << condReg << ", label %" << thenLbl << ", label %" << elseLbl << "\n";
    else out << "    br i1 " << condReg << ", label %" << thenLbl << ", label %" << endLbl << "\n";

    out << "\n" << thenLbl << ":\n";
    genBlock(ifStmt->ifBranch.get(), out);
    out << "    br label %" << endLbl << "\n"; 

    if (ifStmt->elseBranch) {
        out << "\n" << elseLbl << ":\n";
        if (ifStmt->elseBranch->type == NodeType::IF_STMT) genIfStmt(ifStmt->elseBranch.get(), out); 
        else genBlock(ifStmt->elseBranch.get(), out); 
        out << "    br label %" << endLbl << "\n"; 
    }

    out << "\n" << endLbl << ":\n";
}

void CodeGen::genWhileStmt(ASTNode* node, std::ostream& out) {
    auto* whilestmt = static_cast<WhileStmtNode*>(node);
    int blockid = ++regCounter;
    std::string conditionLabel = "while_cond_"+std::to_string(blockid);
    std::string bodyLabel = "while_body_"+std::to_string(blockid);
    std::string endLabel = "while_end_"+std::to_string(blockid);
    

    out << "    br label %" << conditionLabel << "\n";
    out << "\n" << conditionLabel << ":\n";
    std::string conditionReg = genExpression(whilestmt->condition.get(), "bool", out);
    out << "    br i1 " << conditionReg << ", label %" << bodyLabel << ", label %" << endLabel << "\n";
    //br i1 %13, label %while_body_15, label %while_end_18
    out << "\n" << bodyLabel << ":\n";
    genBlock(whilestmt->body.get(), out);
    out << "    br label %" << conditionLabel << "\n";
    out <<"\n" << endLabel << ":\n";
}

void CodeGen::genBreakStmt(ASTNode* node, std::ostream& out) {
    if (breakTracker.empty()) return;
    out << "    br label %" << breakTracker.back() << "\n";
}

void CodeGen::genSwitchStmt(ASTNode* node, std::ostream& out) {
    auto* sw = static_cast<SwitchStmtNode*>(node);

    std::string condReg = genExpression(sw->condition.get(), "any", out);
    std::string llvmCondType = "i32";
    
    int blockId = ++regCounter;
    std::string endLbl = "switch_end_" + std::to_string(blockId);
    
    breakTracker.push_back(endLbl);
    std::string nextCmpLbl = "switch_cmp_0_" + std::to_string(blockId);
    out << "    br label %" << nextCmpLbl << "\n";

    for (size_t i = 0; i < sw->cases.size(); i++) {
        auto& c = sw->cases[i];
        
        std::string bodyLbl = "switch_body_" + std::to_string(i) + "_" + std::to_string(blockId);
        std::string nextCmp = (i + 1 < sw->cases.size()) ? "switch_cmp_" + std::to_string(i + 1) + "_" + std::to_string(blockId) : (sw->hasDefault ? "switch_default_" + std::to_string(blockId) : endLbl);
        out << "\n" << nextCmpLbl << ":\n";
        std::string caseReg = genExpression(c.value.get(), "any", out);
        std::string isMatch = newReg();
        out << "    " << isMatch << " = icmp eq " << llvmCondType << " " << condReg << ", " << caseReg << "\n";
        out << "    br i1 " << isMatch << ", label %" << bodyLbl << ", label %" << nextCmp << "\n";
        
        nextCmpLbl = nextCmp;

        out << "\n" << bodyLbl << ":\n";
        symTable.pushScope();
        for (auto& stmt : c.statements) {
            if (stmt->type == NodeType::BREAK_STMT) genBreakStmt(stmt.get(), out);
            else if (stmt->type == NodeType::ASSIGNMENT_EXPR) genExpression(stmt.get(), "any", out);
            else if (stmt->type == NodeType::VAR_DECL) genVarDecl(stmt.get(), out);
            else if (stmt->type == NodeType::ARRAY_DECL) genArrayDecl(stmt.get(), out);
            else if (stmt->type == NodeType::IF_STMT) genIfStmt(stmt.get(), out);
            else if (stmt->type == NodeType::BLOCK) genBlock(stmt.get(), out);
            else if (stmt->type == NodeType::WHILE_STMT) genWhileStmt(stmt.get(), out);
        }
        symTable.popScope();
        
        bool endsWithBreak = !c.statements.empty() && c.statements.back()->type == NodeType::BREAK_STMT;
        if (!endsWithBreak) {
            std::string nextBody = (i + 1 < sw->cases.size()) ? "switch_body_" + std::to_string(i + 1) + "_" + std::to_string(blockId) : (sw->hasDefault ? "switch_default_body_" + std::to_string(blockId) : endLbl);
            out << "    br label %" << nextBody << "\n";
        }
    }

    if (sw->hasDefault) {
        out << "\nswitch_default_" << blockId << ":\n";
        out << "    br label %switch_default_body_" << blockId << "\n";
        out << "\nswitch_default_body_" << blockId << ":\n";
        symTable.pushScope();
        for (auto& stmt : sw->defaultBlock) {
             if (stmt->type == NodeType::BREAK_STMT) genBreakStmt(stmt.get(), out);
            else if (stmt->type == NodeType::ASSIGNMENT_EXPR) genExpression(stmt.get(), "any", out);
            else if (stmt->type == NodeType::VAR_DECL) genVarDecl(stmt.get(), out);
            else if (stmt->type == NodeType::ARRAY_DECL) genArrayDecl(stmt.get(), out);
            else if (stmt->type == NodeType::IF_STMT) genIfStmt(stmt.get(), out);
            else if (stmt->type == NodeType::BLOCK) genBlock(stmt.get(), out);
            else if (stmt->type == NodeType::WHILE_STMT) genWhileStmt(stmt.get(), out);
        }
        symTable.popScope();
        
        bool endsWithBreak = !sw->defaultBlock.empty() && sw->defaultBlock.back()->type == NodeType::BREAK_STMT;
        if (!endsWithBreak) {
             out << "    br label %" << endLbl << "\n";
        }
    } else {
        out << "\n" << nextCmpLbl << ":\n";
        out << "    br label %" << endLbl << "\n";
    }

    out << "\n" << endLbl << ":\n";
    breakTracker.pop_back(); 
}