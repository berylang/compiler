#include "sema.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/arraydeclare.h"
#include <iostream>
#include "../parser/ast/expressions.h"
#include "../parser/ast/blocknode.h"
#include "../parser/ast/controlflow.h"

SemanticAnalyzer::SemanticAnalyzer(ASTNode* root)
   : root(root), errors(false) {}

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
       analyzeExpression(node);
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
       std::string exprtype = analyzeExpression(decl->value.get());

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
    symbolTable.add(decl->name, {decl->varType, decl->isConst, decl->value != nullptr, decl->line, ""});
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
        std::string exprType = analyzeExpression(initVal.get());

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

    symbolTable.add(decl->name, {decl->elementType + "[]", false, !decl->initializers.empty(), decl->line, ""});
}

bool SemanticAnalyzer::typeMatchesLiteral(const std::string& type, NodeType litType) {
   if (type == "int"    && litType == NodeType::INT_LIT)     return true;
   if (type == "bigint"    && litType == NodeType::INT_LIT)     return true;
   if (type == "float"    && litType == NodeType::DECIMAL_LIT)     return true;
   if (type == "bool" && litType == NodeType::BOOL_LIT) return true;
   if (type == "double" && litType == NodeType::DECIMAL_LIT)  return true;
   if (type == "char"    && litType == NodeType::CHAR_LIT)     return true;
   if (type == "string" && litType == NodeType::STRING_LIT) return true;
   if (type == "string" && litType == NodeType::NULL_LIT) return true;
   return false;
}

std::string SemanticAnalyzer::analyzeExpression(ASTNode* node){
    if(!node) return "unknown";
    switch(node->type){
        case NodeType::INT_LIT: return "int";
        case NodeType::DECIMAL_LIT: return "double";
        case NodeType::BOOL_LIT: return "bool";
        case NodeType::CHAR_LIT: return "char";
        case NodeType::STRING_LIT: return "string";
        case NodeType::NULL_LIT: return "null";
        case NodeType::IDENT:{
            auto* ident = static_cast<IdentNode*>(node);
            if(!symbolTable.exists(ident->name)){
                std::cerr<<"Bery:Error [Line "<< ident->line <<"]: Undefined Variable '"<<ident->name<<"'\n";
                errors=true;
                return "unknown";
            }
            return symbolTable.get(ident->name).type;

        }
        case NodeType::GROUPED_EXPR:{
            auto* group = static_cast<GroupedExprNode*>(node);
            return analyzeExpression(group->expression.get());
        }
        case NodeType::UNARY_EXPR:{
            auto* unary = static_cast<UnaryExprNode*>(node);
            std::string optype = analyzeExpression(unary->operand.get());
            if(unary->optr=="++"||unary->optr=="--"||unary->optr=="post++"||unary->optr=="post--"){
                if(unary->operand->type != NodeType::IDENT){
                    std::cerr<<"Bery:Error [Line "<< unary->line <<"]: Identifier requried as operand of increment or decrement operator\n";
                    errors = true;
                    return "unknown";
                }
            }
            if(unary->optr == "!"){
                return "bool";
            }
            return optype;

        }
        case NodeType::BINARY_EXPR:{
            auto* binary = static_cast<BinaryExprNode*>(node);

            std::string lType = analyzeExpression(binary->left.get());
            std::string rType = analyzeExpression(binary->right.get());
            

            std::string resolvedType = lType;
            if(lType != rType){
                if ((lType == "bigint" && rType == "int") ||(lType == "int" && rType == "bigint")) {
                    resolvedType = "bigint";
                }
                else if ((lType == "float" && rType == "int") || (lType == "int" && rType == "float")) {
                    resolvedType = "float";
                }
                else if ((lType == "bigint" && rType == "float") || (lType == "float" && rType == "bigint")) {
                    resolvedType = "float";
                }
                else if ((lType == "bigint" && rType == "double") || (lType == "double" && rType == "bigint")) {
                    resolvedType = "double";
                }
                else if ((lType == "int" && rType == "double") || (lType == "double" && rType == "int")) {
                    resolvedType = "double";
                }
                else if ((lType == "float" && rType == "double") || (lType == "double" && rType == "float")) {
                    resolvedType = "double";
                }
                else {
                    std::cerr<<"Bery:Error [Line "<< binary->line <<"]: Type mismatch in binary expression '" << lType << "' and '" << rType <<"\n";
                    errors=true;
                    return "unknown";
                }
            }

            binary->opType = resolvedType;

            if(binary->optr== "&&" || binary->optr== "||"){
                if(lType!="bool" || rType!="bool"){    
                    std::cerr<<"Bery:Error [Line "<< binary->line <<"]: Logical Operator '"<<binary->optr<<"' cannot be used on type '"<<lType<<"' and '"<<rType<<"'\n";
                    errors=true;
                    return "unknown";
                }
                binary->opType = "bool";
                return "bool";
            }
            if( binary->optr=="==" || binary->optr=="!=" ||
                binary->optr==">"  || binary->optr==">=" ||
                binary->optr=="<"  || binary->optr=="<=" ){
                if(binary->optr!= "==" && binary->optr!= "!="){
                    if(lType=="string" || lType=="bool" || rType=="string" || rType=="bool"){
                        std::cerr<<"Bery:Error [Line "<< binary->line <<"]: Relational Operator '"<<binary->optr<<"' cannot be used on type '"<<lType<<"' and '"<<rType<<"'\n";
                        errors=true;
                        return "unknown";
                    }
                }
                return "bool";
            }
            
            if(binary->optr=="<<" || binary->optr==">>"){
                if(rType!="int" && rType!="bigint"){
                    std::cerr<<"Bery:Error [Line "<< binary->line <<"]: Right operand of shift operators should be in integer type\n";
                    errors=true;
                    return "unknown";
                }
                if(resolvedType != "int" && resolvedType != "bigint"){
                    std::cerr << "Bery:Error [Line "<< binary->line <<"]: Left operand of shift must be an integer type\n";
                    errors = true;
                    return "unknown";
                }
                return resolvedType;
            }
            if(binary->optr=="&" || binary->optr=="^" || binary->optr=="|"){
                if((lType!="int" && lType!="bigint") || (rType!="int" && rType!="bigint")){
                    std::cerr<<"Bery:Error [Line "<< binary->line <<"]: Bitwise operators require integer operands\n";
                    errors=true;
                    return "unknown";
                }
                return resolvedType;
            }
            return resolvedType;
        }
        case NodeType::BETWEEN_EXPR:{
            auto* between = static_cast<BetweenExprNode*>(node);
            std::string valueType = analyzeExpression(between->value.get());
            std::string lowerType = analyzeExpression(between->lower.get());
            std::string upperType = analyzeExpression(between->upper.get());

            auto validType = [](const std::string& type){
                return type == "int" || type == "bigint" || type == "float" || type == "double" || type == "char";
            };

            if(!validType(valueType) || !validType(lowerType) || !validType(upperType)){
                std::cerr<<"Bery:Error [Line "<< between->line <<"]: Between operator supports only int, bigint, float, double and char\n";
                errors = true;
                return "unknown";
            }

            std::string dominentType = "int";
            if (valueType == "double" || lowerType=="double" || upperType=="double") dominentType= "double";
            else if (valueType == "float" || lowerType=="float" || upperType=="float") dominentType = "float";
            else if (valueType == "bigint" || lowerType=="bigint" || upperType=="bigint") dominentType = "bigint";

            between->opType = dominentType;
            return "bool";
        }
        case NodeType::TERNARY_EXPR: {
            auto* tern = static_cast<TernaryExprNode*>(node);
            std::string condType = analyzeExpression(tern->condition.get());
            if (condType != "bool") {
                std::cerr << "Bery:Error [Line "<< tern->line <<"]: ternary condition must be 'bool', got '" << condType << "'\n";
                errors = true;
                return "unknown";
            }
            std::string tType = analyzeExpression(tern->trueExpr.get());
            std::string fType = analyzeExpression(tern->falseExpr.get());

            std::string finalType = tType;
            if (tType != fType) {
                if ((tType == "bigint" && fType == "int") || (tType == "int" && fType == "bigint")) finalType = "bigint";
                else if ((tType == "float" && fType == "int") || (tType == "int" && fType == "float")) finalType = "float";
                else if ((tType == "bigint" && fType == "float") || (tType == "float" && fType == "bigint")) finalType = "float";
                else if ((tType == "bigint" && fType == "double") || (tType == "double" && fType == "bigint")) finalType = "double";
                else if ((tType == "int" && fType == "double") || (tType == "double" && fType == "int")) finalType = "double";
                else if ((tType == "float" && fType == "double") || (tType == "double" && fType == "float")) finalType = "double";
                else {
                    std::cerr << "Bery:Error [Line "<< tern->line <<"]: Ternary branch type mismatch ('" << tType << "' vs '" << fType << "')\n";
                    errors = true;
                    return "unknown";
                }
            }
            tern->resolvedType = finalType;
            return finalType;
        }
        case NodeType::ASSIGNMENT_EXPR:{
            auto* assign= static_cast<AssignmentExprNode*>(node);
            if(!symbolTable.exists(assign->name)){
                std::cerr<<"Bery:Error [Line "<<assign->line<<"] : Undefined variable '"<<assign->name<<"'\n";
                errors=true;
                return "unknown";
            }
            Symbol& s=symbolTable.get(assign->name);
            if(s.isConst){
                std::cerr<<"Bery:Error [Line "<<assign->line<<"] : cannot reassigned constant variable '"<<assign->name<<"'\n";
                errors=true;
                return "unknown";
            }
            std::string exptype= analyzeExpression(assign->value.get());
            if(exptype!="unknown" && exptype!=s.type ){
                if(!(s.type=="float"&& exptype=="int")&&!(s.type=="double"&& exptype=="int")
                &&!(s.type=="bigint"&& exptype=="int")&&!(s.type=="double"&& exptype=="float")){
                 std::cerr<<"Bery:Error [Line "<<assign->line<<"] : Type missmatch for assignment to  '"<<assign->name<<"'. Expected '"<<s.type<<"', got '"<<exptype<<"'\n";
                errors=true;
                return "unknown";
                }
            }
            s.isInitialized=true;
            return s.type;
        }
        case NodeType::CAST_EXPR: {
            auto* castNode = static_cast<CastExprNode*>(node);
            std::string srcType = analyzeExpression(castNode->expr.get());
            castNode->srcType = srcType; 

            auto isPrimitive = [](const std::string& t) {
                return t == "int" || t == "bigint" || t == "float" || t == "double" || t == "char" || t == "bool";
            };

            if (!isPrimitive(srcType) || !isPrimitive(castNode->targetType)) {
                std::cerr << "Bery:Error [Line " << castNode->line << "]: Invalid cast from '" << srcType << "' to '"  << castNode->targetType   << "'.\n";
                errors = true;
                return "unknown";
            }
            return castNode->targetType;
        }
        default:
            std::cerr << "Bery:Error [Line " << node->line << "]: Unknown expression \n";
            errors = true;
            return "unknown";
    }

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
    std::string conditionType = analyzeExpression(ifStmt->conditions.get());

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
    std::string condType = analyzeExpression(sw->condition.get());

    if (condType != "unknown" && condType != "int" && condType != "bigint" && condType != "char") {
        std::cerr << "Bery:Error [Line " << sw->line << "]: Invalid switch condition type '" << condType << "'. Expected int, bigint, or char.\n";
        errors = true;
    }

    loopOrSwitchDepth++;

    for (auto& c : sw->cases) {
        if (c.value) {
            std::string caseType = analyzeExpression(c.value.get());
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
    std::string conditionType = analyzeExpression(whileStmt->condition.get());

    if(conditionType != "bool" && conditionType != "unknown"){
        std::cerr << "Bery:Error [Line " << whileStmt->line << "]: 'while' condition must evaluate to 'bool' \n";
        errors = true;
    }

    analyzeBlock(whileStmt->body.get());
}

bool SemanticAnalyzer::hasErrors() { return errors; }