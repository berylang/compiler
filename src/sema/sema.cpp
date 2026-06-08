#include "sema.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include <iostream>
#include "../parser/ast/expressions.h"

SemanticAnalyzer::SemanticAnalyzer(ASTNode* root)
   : root(root), errors(false) {}

void SemanticAnalyzer::analyze() {
   auto* program = static_cast<ProgramNode*>(root);

   for (auto& node : program->globals)
       analyzeNode(node.get());
   if (program->runBlock)
       for (auto& node : program->runBlock->statements)
           analyzeNode(node.get());
}

void SemanticAnalyzer::analyzeNode(ASTNode* node) {
   if (node->type == NodeType::VAR_DECL)
       analyzeVarDecl(node);
}


void SemanticAnalyzer::analyzeVarDecl(ASTNode* node) {
   auto* decl = static_cast<VarDeclNode*>(node);
   if (symbolTable.exists(decl->name)) {
       std::cerr << "bery: error: '" << decl->name << "' already declared.\n";
       errors = true;
       return;
   }
   if (decl->isConst && !decl->value) {
       std::cerr << "bery: error: const '" << decl->name << "' must be initialized.\n";
       errors = true;
       return;
   }
   if (decl->value) {
       std::string exprtype = analyzeExpression(decl->value.get());
       if(exprtype!="unknown" && exprtype!=decl->varType){
        if(!(decl->varType == "float" && exprtype == "int")&&
         !(decl->varType == "double" && exprtype == "int") &&
         !(decl->varType == "bigint" && exprtype == "int")&&
         !(decl->varType == "double" && exprtype == "float")&&
         !(decl->varType == "float" && exprtype == "double")){
            std::cerr<<"Bery:Error: Type mismatch for "<<decl->name<<" . Expected '"<<decl->varType<<"', got '"<<exprtype<<"' \n";
            errors = true;
            return;
        }
       }
   }
   symbolTable.add(decl->name, {decl->varType, decl->isConst, decl->value != nullptr, 0});
}
bool SemanticAnalyzer::typeMatchesLiteral(const std::string& type, NodeType litType) {
   if (type == "int"    && litType == NodeType::INT_LIT)     return true;
   if (type == "bigint"    && litType == NodeType::INT_LIT)     return true;
   if (type == "float"    && litType == NodeType::DECIMAL_LIT)     return true;
   if (type == "bool" && litType == NodeType::BOOL_LIT) return true;
   if (type == "double" && litType == NodeType::DECIMAL_LIT)  return true;
   if (type == "char"    && litType == NodeType::CHAR_LIT)     return true;
   return false;
}

std::string SemanticAnalyzer::analyzeExpression(ASTNode* node){
    //@change: When none datatype is supported change unknown to none
    if(!node) return "unknown";
    switch(node->type){
        case NodeType::INT_LIT: return "int";
        case NodeType::DECIMAL_LIT: return "double";
        case NodeType::BOOL_LIT: return "bool";
        case NodeType::CHAR_LIT: return "char";
        case NodeType::IDENT:{
            auto* ident = static_cast<IdentNode*>(node);
            if(!symbolTable.exists(ident->name)){
                std::cerr<<"Bery:Error: Undefined Variable '"<<ident->name<<"'\n";
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
                    std::cerr<<"Bery:Error: Identifier requried as operand of increment or decrement optr\n";
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

            if(lType != rType){
                std::cerr<<"Bery:Error: Type mismatch in binary expression\n";
                errors=true;
                return "unknown";
            }
            return lType;
        }
        default:
            std::cerr<<"Bery:Error: Unknown expression \n";
            errors = true;
            return "unknown";
    }

}

bool SemanticAnalyzer::hasErrors() { return errors; }