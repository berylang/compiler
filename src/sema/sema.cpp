#include "sema.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/arraydeclare.h"
#include <iostream>
#include "../parser/ast/blocknode.h"
#include "../parser/ast/controlflow.h"

SemanticAnalyzer::SemanticAnalyzer(ASTNode* root)
   : root(root), errors(false), typeChecker(symbolTable, errors) {}

void SemanticAnalyzer::analyze() {
    auto* program = static_cast<ProgramNode*>(root);

    for (auto& node : program->globals)
        analyzeNode(node.get());
    if (program->runBlock) {
        symbolTable.pushScope();
        for (auto& node : program->runBlock->statements)
            analyzeNode(node.get());
        symbolTable.popScope();
    }
}

void SemanticAnalyzer::analyzeNode(ASTNode* node) {
    if (node->type == NodeType::VAR_DECL)
        analyzeVarDecl(node);
    else if (node->type == NodeType::ARRAY_DECL)
        analyzeArrayDecl(node);
    else if (node->type == NodeType::ASSIGNMENT_EXPR) 
       typeChecker.analyzeExpression(node);
    else if(node->type == NodeType::IF_STMT)
        analyzeIfStmt(node);
    else if (node->type == NodeType::WHILE_STMT)
        analyzeWhileStmt(node);
} 

void SemanticAnalyzer::analyzeVarDecl(ASTNode* node) {
   auto* decl = static_cast<VarDeclNode*>(node);
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
    symbolTable.add(decl->name, {decl->varType, decl->isConst, decl->value != nullptr, decl->line, "", ""});
}

void SemanticAnalyzer::analyzeArrayDecl(ASTNode* node) {
   auto* decl = static_cast<ArrayDeclNode*>(node);

   if (symbolTable.existsInCurrentScope(decl->name)) {
       std::cerr << "Bery:Error [Line "<< decl->line <<"]: '" << decl->name << "' already declared in this scope.\n";
       errors = true;
       return;
   }
    if (decl->size == 0) {
       std::cerr << "Bery:Error [Line "<< decl->line <<"]: '" << decl->name << "' size must be greater than 0.\n";
       errors = true;
       return;
   }
    if (decl->size < 0 && decl->initializers.empty()) {
       std::cerr << "Bery:Error [Line "<< decl->line <<"]: '" << decl->name << "' must have an initializer list.\n";
       errors = true;
       return;
   }
    if (decl->size >= 0 && decl->initializers.size() > (size_t)decl->size) {
       std::cerr << "Bery:Error [Line "<< decl->line <<"]: initializer list count exceeds array size for '" << decl->name << "'. It should be in Declared size\n";
       errors = true;
       return;
   }
    for (auto& initVal : decl->initializers) {
        std::string exprType = typeChecker.analyzeExpression(initVal.get());

        if (exprType != "unknown" && exprType != decl->elementType) {
            if (exprType == "null") {
                if (decl->elementType != "string") {
                    std::cerr << "Bery:Error [Line "<< decl->line <<"]: Cannot assign 'null' to non-reference type '" << decl->elementType << "'\n";
                    errors = true;
                    return;
                }
                continue;
            }
            if (!(decl->elementType == "float" && exprType == "int") &&
                !(decl->elementType == "float" && exprType == "double") &&
                !(decl->elementType == "double" && exprType == "int") &&
                !(decl->elementType == "double" && exprType == "float") &&
                !(decl->elementType == "bigint" && exprType == "int")) {
                    std::cerr << "Bery:Error [Line "<< decl->line <<"]: Type mismatch in array initialization for '"<<decl->name<<"' . Expected '"<<decl->elementType<<"', got '"<<exprType<<"' \n";
                    errors = true;
                    return;
                }
        }
   }

    symbolTable.add(decl->name, {decl->elementType + "[]", false, !decl->initializers.empty(), decl->line, "", ""});
}

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

void SemanticAnalyzer::analyzeWhileStmt(ASTNode* node){
    auto* whileStmt = static_cast<WhileStmtNode*>(node);
    std::string conditionType = typeChecker.analyzeExpression(whileStmt->condition.get());

    if(conditionType != "bool" && conditionType != "unknown"){
        std::cerr << "Bery:Error [Line " << whileStmt->line << "]: 'while' condition must evaluate to 'bool' \n";
        errors = true;
    }

    loopOrSwitchDepth++;
    analyzeBlock(whileStmt->body.get());
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
    analyzeBlock(dowhilestmt->body.get());
    loopOrSwitchDepth--;
}

bool SemanticAnalyzer::hasErrors() { return errors; }
