#include "typechecker.h"
#include "../parser/ast/expressions.h"
#include "../parser/ast/literals.h"
#include <iostream>

TypeChecker::TypeChecker(SymbolTable& symTable, bool& errorsFlag) 
    : symbolTable(symTable), errors(errorsFlag) {}

bool TypeChecker::typeMatchesLiteral(const std::string& type, NodeType litType) {
   if (type == "int"    && litType == NodeType::INT_LIT)     return true;
   if (type == "bigint" && litType == NodeType::INT_LIT)     return true;
   if (type == "float"  && litType == NodeType::DECIMAL_LIT) return true;
   if (type == "bool"   && litType == NodeType::BOOL_LIT)    return true;
   if (type == "double" && litType == NodeType::DECIMAL_LIT) return true;
   if (type == "char"   && litType == NodeType::CHAR_LIT)    return true;
   if (type == "string" && litType == NodeType::STRING_LIT)  return true;
   if (type == "string" && litType == NodeType::NULL_LIT)    return true;
   return false;
}

std::string TypeChecker::analyzeExpression(ASTNode* node) {
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
                if ((lType == "bigint" && rType == "int") ||(lType == "int" && rType == "bigint")) resolvedType = "bigint";
                else if ((lType == "float" && rType == "int") || (lType == "int" && rType == "float")) resolvedType = "float";
                else if ((lType == "bigint" && rType == "float") || (lType == "float" && rType == "bigint")) resolvedType = "float";
                else if ((lType == "bigint" && rType == "double") || (lType == "double" && rType == "bigint")) resolvedType = "double";
                else if ((lType == "int" && rType == "double") || (lType == "double" && rType == "int")) resolvedType = "double";
                else if ((lType == "float" && rType == "double") || (lType == "double" && rType == "float")) resolvedType = "double";
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
        case NodeType::ASSIGNMENT_EXPR: {
            auto* assign = static_cast<AssignmentExprNode*>(node);
            std::string targetType;
            std::string targetName; 
            
            if (assign->target->type == NodeType::IDENT) {
                auto* ident = static_cast<IdentNode*>(assign->target.get());
                targetName = ident->name;

                if (!symbolTable.exists(ident->name)) {
                    std::cerr << "Bery:Error [Line " << assign->line << "] : Undefined variable '" << ident->name << "'\n";
                    errors = true;
                    return "unknown";
                }
                Symbol& s = symbolTable.get(ident->name);
                if (s.isConst) {
                    std::cerr << "Bery:Error [Line " << assign->line << "] : cannot reassign constant variable '" << ident->name << "'\n";
                    errors = true;
                    return "unknown";
                }
                s.isInitialized = true;
                targetType = s.type;

            } else if (assign->target->type == NodeType::INDEX_EXPR) {
                auto* idxNode = static_cast<IndexExprNode*>(assign->target.get());
                targetName = idxNode->name;

                targetType = analyzeExpression(assign->target.get());
                if (targetType == "unknown") return "unknown";
            } else {
                std::cerr << "Bery:Error [Line " << assign->line << "] : Invalid assignment target\n";
                errors = true;
                return "unknown";
            }

            std::string exptype = analyzeExpression(assign->value.get());
            
            if (exptype != "unknown" && exptype != targetType) {
                if (!(targetType == "float" && exptype == "int") &&  !(targetType == "double" && exptype == "int") &&
                    !(targetType == "bigint" && exptype == "int") &&  !(targetType == "double" && exptype == "float")) {
                    
                    std::cerr << "Bery:Error [Line " << assign->line << "] : Type missmatch for assignment to  '" << targetName << "'. Expected '" << targetType << "', got '" << exptype << "'\n";
                    errors = true;
                    return "unknown";
                }
            }
            return targetType;
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
        case NodeType::INDEX_EXPR: {
            auto* idxNode = static_cast<IndexExprNode*>(node);
            if (!symbolTable.exists(idxNode->name)) {
                std::cerr << "Bery:Error [Line " << idxNode->line << "]: Undefined array '" << idxNode->name << "'\n";
                errors = true; 
                return "unknown";
            }
            Symbol& sym = symbolTable.get(idxNode->name);
            int dimCount = 0;
            size_t pos = sym.type.find('[');
            while (pos != std::string::npos) {
                dimCount++;
                pos = sym.type.find('[', pos + 1);
            }

            if (dimCount == 0) {
                std::cerr << "Bery:Error [Line " << idxNode->line << "]: Variable '" << idxNode->name << "' is not subscriptable\n";
                errors = true; 
                return "unknown";
            }

            if (idxNode->indices.size() > (size_t)dimCount) {
                std::cerr << "Bery:Error [Line " << idxNode->line << "]: Too many indices for array '" << idxNode->name << "'\n";
                errors = true; 
                return "unknown";
            }

            for (auto& index : idxNode->indices) {
                std::string idxType = analyzeExpression(index.get());
                if (idxType != "int" && idxType != "bigint") {
                    std::cerr << "Bery:Error [Line " << idxNode->line << "]: Array index must be an integer\n";
                    errors = true; 
                    return "unknown";
                }
            }

            std::string resultType = sym.type.substr(0, sym.type.find('['));
            for (size_t i = 0; i < dimCount - idxNode->indices.size(); ++i) resultType += "[]";
            return resultType;
        }
        default:
            std::cerr << "Bery:Error [Line " << node->line << "]: Unknown expression \n";
            errors = true;
            return "unknown";
    }
}