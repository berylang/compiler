#include "../codegen.h"
#include "../../parser/ast/controlflow.h"
#include "../../parser/ast/blocknode.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/literals.h"
#include "../../sema/symboltable.h"
#include <iostream>

void CodeGen::genBlock(ASTNode* node, std::ostream& out) {
    auto* block = static_cast<BlockNode*>(node);
    symTable.pushScope(); 
    pushGCScope();
    for (auto& stmt : block->statements) {
        genStatement(stmt.get(), out);
    }
    int roots = popGCScope();
    emitGCPops(roots, out);
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
    
    breakTracker.push_back(endLabel);
    continueTracker.push_back(conditionLabel);
    out << "    br label %" << conditionLabel << "\n";
    out << "\n" << conditionLabel << ":\n";
    std::string conditionReg = genExpression(whilestmt->condition.get(), "bool", out);
    out << "    br i1 " << conditionReg << ", label %" << bodyLabel << ", label %" << endLabel << "\n";
    //br i1 %13, label %while_body_15, label %while_end_18
    out << "\n" << bodyLabel << ":\n";
    genBlock(whilestmt->body.get(), out);
    out << "    br label %" << conditionLabel << "\n";
    out <<"\n" << endLabel << ":\n";
    continueTracker.pop_back();
    breakTracker.pop_back();
}

void CodeGen::genDoWhileStmt(ASTNode* node, std::ostream& out) {
    auto* dowhile = static_cast<DoWhileStmtNode*>(node);
    int blockId = ++regCounter;
    
    std::string bodyLbl = "dowhile_body_" + std::to_string(blockId);
    std::string condLbl = "dowhile_cond_" + std::to_string(blockId);
    std::string endLbl = "dowhile_end_" + std::to_string(blockId);

    breakTracker.push_back(endLbl);
    continueTracker.push_back(condLbl);
    out << "    br label %" << bodyLbl << "\n";
    out << "\n" << bodyLbl << ":\n";
    genBlock(dowhile->body.get(), out);
    continueTracker.pop_back();
    out << "    br label %" << condLbl << "\n";
    out << "\n" << condLbl << ":\n";
    std::string condReg = genExpression(dowhile->condition.get(), "bool", out);
    
    out << "    br i1 " << condReg << ", label %" << bodyLbl << ", label %" << endLbl << "\n";
    out << "\n" << endLbl << ":\n";
    
    breakTracker.pop_back();
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
        pushGCScope();
        for (auto& stmt : c.statements) {
            genStatement(stmt.get(), out);
        }
        int roots = popGCScope();
        emitGCPops(roots, out);
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
        pushGCScope();
        for (auto& stmt : sw->defaultBlock) {
            genStatement(stmt.get(), out);
        }
        int roots = popGCScope();
        emitGCPops(roots, out); 
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

void CodeGen::genBreakStmt(ASTNode* node, std::ostream& out) {
    if (breakTracker.empty()) return;
    out << "    br label %" << breakTracker.back() << "\n";
}

void CodeGen::genContinueStmt(ASTNode* node, std::ostream& out) {
    if (continueTracker.empty()) return;
    out << "    br label %" << continueTracker.back() << "\n";
}

void CodeGen::genForStmt(ASTNode* node, std::ostream& out) {
    auto* forStmt = static_cast<ForStmtNode*>(node);
    
    symTable.pushScope();
    pushGCScope();
    if (forStmt->init) {
        genStatement(forStmt->init.get(), out);
    }
    
    
    int blockid = ++regCounter;
    std::string condLbl = "for_cond_" + std::to_string(blockid);
    std::string bodyLbl = "for_body_" + std::to_string(blockid);
    std::string updateLbl = "for_update_" + std::to_string(blockid);
    std::string endLbl = "for_end_" + std::to_string(blockid);
    
    out << "    br label %" << condLbl << "\n\n";
    out << condLbl << ":\n";
    if (forStmt->condition) {
        std::string condReg = genExpression(forStmt->condition.get(), "bool", out);
        out << "    br i1 " << condReg << ", label %" << bodyLbl << ", label %" << endLbl << "\n";
    } else {
        out << "    br label %" << bodyLbl << "\n";
    }
    out << "\n" << bodyLbl << ":\n";
    
    breakTracker.push_back(endLbl);
    continueTracker.push_back(updateLbl);
    
    genBlock(forStmt->body.get(), out);
    
    continueTracker.pop_back();
    breakTracker.pop_back();
    
    out << "    br label %" << updateLbl << "\n";
    out << "\n" << updateLbl << ":\n";
    if (forStmt->update) {
        genExpression(forStmt->update.get(), "any", out);
    }
    out << "    br label %" << condLbl << "\n";
    out << "\n" << endLbl << ":\n";
    symTable.popScope();
    int roots = popGCScope();
    emitGCPops(roots, out);
}

void CodeGen::genForInStmt(ASTNode* node, std::ostream& out) {
    auto* forIn = static_cast<ForInNode*>(node);
    symTable.pushScope();
    pushGCScope();

    if (forIn->rangeEnd) {
        std::string lt = "i32"; 
        bool isFloat = false;
        
        if (forIn->varType == "float" || forIn->varType == "double") {
            lt = "double";
            isFloat = true;
        } else if (forIn->varType == "char") {
            lt = "i8";
        } else if (forIn->varType == "bigint") {
            lt = "i64";
        }
        std::string varReg = "%" + forIn->varName + "_" + std::to_string(++regCounter);
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = forIn->varType;
        sym.isInitialized = true;
        sym.line = forIn->line;
        sym.llvmRegister = varReg;
        sym.llvmAllocType = lt;
        symTable.add(forIn->varName, sym);
        out << "    " << varReg << " = alloca " << lt << "\n";
        std::string startReg = genExpression(forIn->iterableOrStart.get(), lt, out);
        out << "    store " << lt << " " << startReg << ", " << lt << "* " << varReg << "\n";
        
        std::string limitReg = genExpression(forIn->rangeEnd.get(), lt, out);
        std::string stepReg = isFloat ? "1.0" : "1"; 
        if (forIn->step) {
            stepReg = genExpression(forIn->step.get(), lt, out);
        }
        int blockid = ++regCounter;
        std::string condLbl = "forin_cond_" + std::to_string(blockid);
        std::string bodyLbl = "forin_body_" + std::to_string(blockid);
        std::string updateLbl = "forin_update_" + std::to_string(blockid);
        std::string endLbl = "forin_end_" + std::to_string(blockid);
        
        out << "    br label %" << condLbl << "\n\n";
        out << condLbl << ":\n";
        std::string currValReg = "%load_" + std::to_string(++regCounter);
        out << "    " << currValReg << " = load " << lt << ", " << lt << "* " << varReg << "\n";
        
        std::string condReg = "%cmp_" + std::to_string(++regCounter);
        if (isFloat) {
            out << "    " << condReg << " = fcmp ole " << lt << " " << currValReg << ", " << limitReg << "\n";
        } else {
            out << "    " << condReg << " = icmp sle " << lt << " " << currValReg << ", " << limitReg << "\n";
        }
        
        out << "    br i1 " << condReg << ", label %" << bodyLbl << ", label %" << endLbl << "\n";
        out << "\n" << bodyLbl << ":\n";
        breakTracker.push_back(endLbl);
        continueTracker.push_back(updateLbl);
        genBlock(forIn->body.get(), out);
        continueTracker.pop_back();
        breakTracker.pop_back();
        out << "    br label %" << updateLbl << "\n";
        out << "\n" << updateLbl << ":\n";
        std::string upLoadReg = "%load_upd_" + std::to_string(++regCounter);
        out << "    " << upLoadReg << " = load " << lt << ", " << lt << "* " << varReg << "\n";
        
        std::string addReg = "%add_" + std::to_string(++regCounter);
        if (isFloat) {
            out << "    " << addReg << " = fadd " << lt << " " << upLoadReg << ", " << stepReg << "\n";
        } else {
            out << "    " << addReg << " = add " << lt << " " << upLoadReg << ", " << stepReg << "\n";
        }
        out << "    store " << lt << " " << addReg << ", " << lt << "* " << varReg << "\n";
        out << "    br label %" << condLbl << "\n";
        out << "\n" << endLbl << ":\n";
        
    } else {
        std::string lt = "i32"; 
        if (forIn->varType == "float" || forIn->varType == "double") lt = "double";
        else if (forIn->varType == "char") lt = "i8";
        else if (forIn->varType == "bool") lt = "i1";
        
        std::string idxType = "i32";
        std::string varReg = "%" + forIn->varName + "_" + std::to_string(++regCounter);
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = forIn->varType;
        sym.isInitialized = true;
        sym.line = forIn->line;
        sym.llvmRegister = varReg;
        sym.llvmAllocType = lt;
        symTable.add(forIn->varName, sym);
        out << "    " << varReg << " = alloca " << lt << "\n";
        
        std::string idxReg = "%forin_idx_" + std::to_string(++regCounter);
        out << "    " << idxReg << " = alloca " << idxType << "\n";
        out << "    store " << idxType << " 0, " << idxType << "* " << idxReg << "\n";

        auto* identNode = static_cast<IdentNode*>(forIn->iterableOrStart.get());
        std::string arrName = identNode->name;
        std::string arrPtr = genExpression(forIn->iterableOrStart.get(), "ptr", out);
        
        int arrSize = symTable.get(arrName).arraySize; 
        std::string limitReg = std::to_string(arrSize);

        int blockid = ++regCounter;
        std::string condLbl = "forin_cond_" + std::to_string(blockid);
        std::string bodyLbl = "forin_body_" + std::to_string(blockid);
        std::string updateLbl = "forin_update_" + std::to_string(blockid);
        std::string endLbl = "forin_end_" + std::to_string(blockid);

        out << "    br label %" << condLbl << "\n\n";
        out << condLbl << ":\n";
        std::string currIdxReg = "%load_idx_" + std::to_string(++regCounter);
        out << "    " << currIdxReg << " = load " << idxType << ", " << idxType << "* " << idxReg << "\n";
        
        std::string condReg = "%cmp_" + std::to_string(++regCounter);
        out << "    " << condReg << " = icmp slt " << idxType << " " << currIdxReg << ", " << limitReg << "\n";
        out << "    br i1 " << condReg << ", label %" << bodyLbl << ", label %" << endLbl << "\n";

        out << "\n" << bodyLbl << ":\n";
        
        std::string gepReg = "%gep_" + std::to_string(++regCounter);
        out << "    " << gepReg << " = getelementptr inbounds " << lt << ", " << lt << "* " << arrPtr << ", i32 0, " << idxType << " " << currIdxReg << "\n";
 
        std::string valReg = "%load_val_" + std::to_string(++regCounter);
        out << "    " << valReg << " = load " << lt << ", " << lt << "* " << gepReg << "\n";
        out << "    store " << lt << " " << valReg << ", " << lt << "* " << varReg << "\n";

        breakTracker.push_back(endLbl);
        continueTracker.push_back(updateLbl); 

        genBlock(forIn->body.get(), out);

        continueTracker.pop_back();
        breakTracker.pop_back();
        out << "    br label %" << updateLbl << "\n";

        out << "\n" << updateLbl << ":\n";
        std::string upLoadIdx = "%load_upd_idx_" + std::to_string(++regCounter);
        out << "    " << upLoadIdx << " = load " << idxType << ", " << idxType << "* " << idxReg << "\n";
        
        std::string addIdx = "%add_idx_" + std::to_string(++regCounter);
        out << "    " << addIdx << " = add " << idxType << " " << upLoadIdx << ", 1\n";
        out << "    store " << idxType << " " << addIdx << ", " << idxType << "* " << idxReg << "\n";
        
        out << "    br label %" << condLbl << "\n";
        out << "\n" << endLbl << ":\n";
    }

    symTable.popScope();
    int roots = popGCScope();
    emitGCPops(roots, out);
}