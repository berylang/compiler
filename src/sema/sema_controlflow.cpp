#include "sema.h"

/*

    Semantic Analyzer, Declarations,

    this file analyze - 
        if statements,
        switch statements,
        for loops,
        for in loops
        while loops
        do-while loops, 
        and continue / break statements


*/
#include <iostream>
#include "../parser/ast/blocknode.h"
#include "../parser/ast/controlflow.h"


void SemanticAnalyzer::analyzeBlock(ASTNode* node) { 
    auto* block = static_cast<BlockNode*>(node);
    symbolTable.pushScope();
    
    for(auto& statement:block->statements){
        analyzeNode(statement.get());
    }

    symbolTable.popScope();
}

void SemanticAnalyzer::analyzeIfStmt(ASTNode* node) { 
    auto* ifStmt = static_cast<IfStmtNode*>(node);
    std::string conditionType = typeChecker.analyzeExpression(ifStmt->conditions.get());

    if(conditionType != "bool" && conditionType != "unknown"){
        std::cerr << "Bery:Error [Line " << ifStmt->line << "]: if condition must evaluate to bool \n";
        errors = true;
    }

    analyzeBlock(ifStmt->ifBranch.get());

    if(ifStmt->elseBranch){
        if(ifStmt->elseBranch->type == NodeType::IF_STMT){
            analyzeIfStmt(ifStmt->elseBranch.get());
        }
        else{
            analyzeBlock(ifStmt->elseBranch.get());
        }
    }
}

void SemanticAnalyzer::analyzeSwitchStmt(ASTNode* node) {
    auto* sw = static_cast<SwitchStmtNode*>(node);
    std::string condType = typeChecker.analyzeExpression(sw->condition.get());

    if (condType != "unknown" && condType != "int" && condType != "bigint" && condType != "char") {
        std::cerr << "Bery:Error [Line " << sw->line << "]: Invalid switch condition type '" << condType << "'. Expected int, bigint, or char.\n";
        errors = true;
    }

    loopOrSwitchDepth++;

    for (auto& c : sw->cases) {
        if (c.value) {
            std::string caseType = typeChecker.analyzeExpression(c.value.get());
            if (caseType != "unknown" && condType != "unknown" && caseType != condType) {
                std::cerr << "Bery:Error [Line " << sw->line << "]: Case type '" << caseType << "' does not match switch condition type '" << condType << "'\n";
                errors = true;
            }
        }
        
        symbolTable.pushScope();
        for (auto& s : c.statements) analyzeNode(s.get());
        symbolTable.popScope();
    }

    if (sw->hasDefault) {
        symbolTable.pushScope();
        for (auto& s : sw->defaultBlock) analyzeNode(s.get());
        symbolTable.popScope();
    }

    loopOrSwitchDepth--;
}

void SemanticAnalyzer::analyzeBreakStmt(ASTNode* node) {
    if (loopOrSwitchDepth <= 0) {
        std::cerr << "Bery:Error [Line " << node->line << "]: 'break' used outside of a loop or switch.\n";
        errors = true;
    }
}

void SemanticAnalyzer::analyzeContinueStmt(ASTNode* node) {
    if (loopDepth <= 0){
        std::cerr << "Bery:Error [Line " << node->line << "]: 'continue' used outside of a loop. \n";
        errors = true;
    }
}

void SemanticAnalyzer::analyzeWhileStmt(ASTNode* node){
    auto* whileStmt = static_cast<WhileStmtNode*>(node);
    std::string conditionType = typeChecker.analyzeExpression(whileStmt->condition.get());

    if(conditionType != "bool" && conditionType != "unknown"){
        std::cerr << "Bery:Error [Line " << whileStmt->line << "]: 'while' condition must evaluate to 'bool' \n";
        errors = true;
    }

    loopOrSwitchDepth++;
    loopDepth++;
    analyzeBlock(whileStmt->body.get());
    loopDepth--;
    loopOrSwitchDepth--;
}

void SemanticAnalyzer::analyzeDoWhileStmt(ASTNode* node){
    auto* dowhilestmt = static_cast<DoWhileStmtNode*>(node);
    std::string conditionType = typeChecker.analyzeExpression(dowhilestmt->condition.get());

    if(conditionType != "bool" && conditionType != "unknown"){
        std::cerr << "Bery:Error [Line " << dowhilestmt->line << "]: 'while' condition must evaluate to 'bool' \n";
        errors = true;
    }

    loopOrSwitchDepth++;
    loopDepth++;
    analyzeBlock(dowhilestmt->body.get());
    loopDepth--;
    loopOrSwitchDepth--;
}

void SemanticAnalyzer::analyzeForStmt(ASTNode* node) {
    auto* forStmt = static_cast<ForStmtNode*>(node);
    symbolTable.pushScope();
    
    if (forStmt->init) { 
        analyzeNode(forStmt->init.get());
    }
    
    if (forStmt->condition) {
        std::string condType = typeChecker.analyzeExpression(forStmt->condition.get());
        if (condType != "bool" && condType != "unknown") {
            std::cerr << "Bery:Error [Line " << forStmt->line << "]: Loop condition must evaluate to 'bool'\n";
            errors = true;
        }
    }
    
    if (forStmt->update) {
        typeChecker.analyzeExpression(forStmt->update.get());
    }
    loopDepth++; loopOrSwitchDepth++;
    analyzeBlock(forStmt->body.get()); 
    loopDepth--; loopOrSwitchDepth--;
    symbolTable.popScope();
}

void SemanticAnalyzer::analyzeForInStmt(ASTNode* node) {
    auto* forIn = static_cast<ForInNode*>(node);
    symbolTable.pushScope();

    std::string actualVarType = forIn->varType;
    if (forIn->rangeEnd) {
        std::string startType = typeChecker.analyzeExpression(forIn->iterableOrStart.get());
        std::string endType = typeChecker.analyzeExpression(forIn->rangeEnd.get());
        if (actualVarType == "unknown" || actualVarType == "") {
            actualVarType = (startType != "unknown") ? startType : "int"; 
        }
    } else {
        std::string iterType = typeChecker.analyzeExpression(forIn->iterableOrStart.get());
        
        if (actualVarType == "unknown" || actualVarType == "") {
            if (iterType.find("[]") != std::string::npos) {
                actualVarType = iterType.substr(0, iterType.find("[]"));
            } else if (iterType == "string") {
                actualVarType = "char";
            } else {
                std::cerr << "Bery:Error [Line " << forIn->line << "]: Cannot iterate over type '" << iterType << "'\n";
                errors = true;
                actualVarType = "unknown";
            }
        }
    }

    if (forIn->step) {
        typeChecker.analyzeExpression(forIn->step.get());
    }
    symbolTable.add(forIn->varName, {actualVarType, false, true, forIn->line, "", "", 0});

    loopDepth++; 
    loopOrSwitchDepth++;
    analyzeBlock(forIn->body.get());
    loopDepth--; 
    loopOrSwitchDepth--;

    symbolTable.popScope();
}
