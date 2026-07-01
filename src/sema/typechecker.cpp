#include "typechecker.h"

/*

    Bery Type Checker,
    
    it divides type checking into smaller parts, by spliting them based on their
    node type.

*/

#include "../parser/ast/expressions.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/functions.h"
#include <iostream>
#include <unordered_set>

TypeChecker::TypeChecker(SymbolTable& symTable, std::unordered_map<std::string, FunctionSignature>& funcs, bool& errorsFlag) 
    : symbolTable(symTable), functions(funcs), errors(errorsFlag) {}

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
    switch (node->type) {
        case NodeType::BINARY_EXPR:     return checkBinaryExpr(node);
        case NodeType::UNARY_EXPR:      return checkUnaryExpr(node);
        case NodeType::TERNARY_EXPR:    return checkTernaryExpr(node);
        case NodeType::BETWEEN_EXPR:    return checkBetweenExpr(node);
        case NodeType::CALL_EXPR:       return checkCallExpr(node);
        case NodeType::INDEX_EXPR:      return checkIndexExpr(node);
        case NodeType::ASSIGNMENT_EXPR: return checkAssignmentExpr(node);
        case NodeType::CAST_EXPR:       return checkCastExpr(node);
        case NodeType::IDENT:           return checkIdentifier(node);
        default:                        return checkLiteral(node);
    }
}

std::string TypeChecker::resolveNumericPromotion(const std::string& lType, const std::string& rType) {
    if (lType == rType) return lType;
    if ((lType == "bigint" && rType == "int")    || (lType == "int"    && rType == "bigint")) return "bigint";
    if ((lType == "float"  && rType == "int")    || (lType == "int"    && rType == "float"))  return "float";
    if ((lType == "bigint" && rType == "float")  || (lType == "float"  && rType == "bigint")) return "float";
    if ((lType == "bigint" && rType == "double") || (lType == "double" && rType == "bigint")) return "double";
    if ((lType == "int"    && rType == "double") || (lType == "double" && rType == "int"))    return "double";
    if ((lType == "float"  && rType == "double") || (lType == "double" && rType == "float"))  return "double";
    return "";
}

std::string TypeChecker::checkBinaryExpr(ASTNode* node) {
    auto* binary = static_cast<BinaryExprNode*>(node);
    std::string lType = analyzeExpression(binary->left.get());
    std::string rType = analyzeExpression(binary->right.get());

    if (lType == "string" && rType == "string") {
        if (binary->optr == "+") {
            binary->resolvedType = "string";
            return binary->resolvedType;
        }
        if (binary->optr == "==" || binary->optr == "!=") {
            binary->resolvedType = "bool";
            return binary->resolvedType;
        }
    }

    if (binary->optr == "&&" || binary->optr == "||") {
        if (lType != "bool" || rType != "bool") {
            std::cerr << "Bery:Error [Line " << binary->line << "]: Logical operator '" << binary->optr << "' cannot be used on type '" << lType << "' and '" << rType << "'\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        binary->resolvedType = "bool";
        return binary->resolvedType;
    }

    std::string resolved = resolveNumericPromotion(lType, rType);
    if (resolved.empty()) {
        std::cerr << "Bery:Error [Line " << binary->line << "]: Type mismatch in binary expression '" << lType << "' and '" << rType << "'\n";
        errors = true;
        binary->resolvedType = "unknown";
        return binary->resolvedType;
    }

    if (binary->optr == "==" || binary->optr == "!=" ||
        binary->optr == ">"  || binary->optr == ">=" ||
        binary->optr == "<"  || binary->optr == "<=") {
        if (binary->optr != "==" && binary->optr != "!=") {
            if (lType == "string" || lType == "bool" || rType == "string" || rType == "bool") {
                std::cerr << "Bery:Error [Line " << binary->line << "]: Relational operator '" << binary->optr << "' cannot be used on type '" << lType << "' and '" << rType << "'\n";
                errors = true;
                binary->resolvedType = "unknown";
                return binary->resolvedType;
            }
        }
        binary->resolvedType = "bool";
        return binary->resolvedType;
    }

    if (binary->optr == "<<" || binary->optr == ">>") {
        if (rType != "int" && rType != "bigint") {
            std::cerr << "Bery:Error [Line " << binary->line << "]: Right operand of shift must be an integer type\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        if (resolved != "int" && resolved != "bigint") {
            std::cerr << "Bery:Error [Line " << binary->line << "]: Left operand of shift must be an integer type\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        binary->resolvedType = resolved;
        return binary->resolvedType;
    }

    if (binary->optr == "&" || binary->optr == "^" || binary->optr == "|") {
        if ((lType != "int" && lType != "bigint") || (rType != "int" && rType != "bigint")) {
            std::cerr << "Bery:Error [Line " << binary->line << "]: Bitwise operators require integer operands\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        binary->resolvedType = resolved;
        return binary->resolvedType;
    }
    binary->resolvedType = resolved;
    return binary->resolvedType;
}

std::string TypeChecker::checkTernaryExpr(ASTNode* node) {
    auto* tern = static_cast<TernaryExprNode*>(node);
    std::string condType = analyzeExpression(tern->condition.get());
    if (condType != "bool") {
        std::cerr << "Bery:Error [Line " << tern->line << "]: Ternary condition must be 'bool', got '" << condType << "'\n";
        errors = true;
        tern->resolvedType = "unknown";
        return tern->resolvedType;
    }

    std::string tType = analyzeExpression(tern->trueExpr.get());
    std::string fType = analyzeExpression(tern->falseExpr.get());

    std::string resolved = resolveNumericPromotion(tType, fType);
    if (resolved.empty()) {
        std::cerr << "Bery:Error [Line " << tern->line << "]: Ternary branch type mismatch ('" << tType << "' vs '" << fType << "')\n";
        errors = true;
        tern->resolvedType = "unknown";
        return tern->resolvedType;
    }

    tern->resolvedType = resolved;
    return tern->resolvedType;
}

std::string TypeChecker::checkUnaryExpr(ASTNode* node) {
    auto* unary = static_cast<UnaryExprNode*>(node);
    std::string optype = analyzeExpression(unary->operand.get());
    if(unary->optr=="++"||unary->optr=="--"||unary->optr=="post++"||unary->optr=="post--"){
        if(unary->operand->type != NodeType::IDENT){
            std::cerr<<"Bery:Error [Line "<< unary->line <<"]: Identifier requried as operand of increment or decrement operator\n";
            errors = true;
            unary->resolvedType = "unknown";
            return unary->resolvedType;
        }
    }
    if(unary->optr == "!"){
        unary->resolvedType = "bool";
        return unary->resolvedType;
    }
    unary->resolvedType = optype;
    return unary->resolvedType;
}

std::string TypeChecker::checkBetweenExpr(ASTNode* node) {
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
        between->resolvedType = "unknown";
        return between->resolvedType;
    }

    std::string dominentType = "int";
    if (valueType == "double" || lowerType=="double" || upperType=="double") dominentType= "double";
    else if (valueType == "float" || lowerType=="float" || upperType=="float") dominentType = "float";
    else if (valueType == "bigint" || lowerType=="bigint" || upperType=="bigint") dominentType = "bigint";

    between->resolvedType = dominentType;
    return between->resolvedType;
}

std::string TypeChecker::checkCallExpr(ASTNode* node) {
    auto* call = static_cast<CallExprNode*>(node);

    static const std::unordered_set<std::string> builtinIO = {
        "print", "println",
        "inputInt", "inputBigInt", "inputFloat",
        "inputDouble", "inputBool", "inputChar", "inputString"
    };

    static const std::unordered_map<std::string, std::string> inputTypes = {
        {"inputInt", "int"}, {"inputBigInt", "bigint"}, {"inputFloat", "float"},
        {"inputDouble", "double"}, {"inputBool", "bool"}, {"inputChar", "char"}, {"inputString", "string"}
    };

    if (builtinIO.count(call->callee)) {
        if (call->callee == "print" && call->arguments.size() != 1) {
            std::cerr << "Bery:Error [Line " << call->line << "]: print() expects exactly 1 argument\n";
            errors = true;
        }
        if (call->callee == "println" && call->arguments.size() > 1) {
            std::cerr << "Bery:Error [Line " << call->line << "]: println() expects 0 or 1 argument\n";
            errors = true;
        }
        if (call->callee != "print" && call->callee != "println" && call->arguments.size() != 1) {
            std::cerr << "Bery:Error [Line " << call->line << "]: " << call->callee << "() expects exactly 1 argument\n";
            errors = true;
        }
        auto it = inputTypes.find(call->callee);
        call->resolvedType = (it != inputTypes.end()) ? it->second : "void";
        return call->resolvedType;
    }
    size_t dot = call->callee.find('.');
    if (dot != std::string::npos) {
        std::string objName = call->callee.substr(0, dot);
        std::string method  = call->callee.substr(dot + 1);

        if (!symbolTable.exists(objName)) {
            std::cerr << "Bery:Error [Line " << call->line << "]: Undefined variable '" << objName << "'\n";
            errors = true;
            call->resolvedType = "unknown";
            return call->resolvedType;
        }

        std::string objType = symbolTable.get(objName).type;

        if (objType == "string") {
            if (method == "substr" || method == "copy") { 
                call->resolvedType = "string"; 
                return call->resolvedType; 
            }
            if (method == "len")                        { 
                call->resolvedType = "int";    
                return call->resolvedType; 
            }
        }

        if (objType.size() > 6 && objType.substr(0, 6) == "array<") {
            std::string elemType = objType.substr(6, objType.size() - 7);
            if (method == "push" || method == "pop" || method == "insert" || method == "remove") {
                call->resolvedType = "void"; 
                return call->resolvedType;
            }
            if (method == "len") { 
                call->resolvedType = "int";
                return call->resolvedType; 
                }
            if (method == "get") { 
                call->resolvedType = elemType; 
                return call->resolvedType; 
            }
        }

        std::cerr << "Bery:Error [Line " << call->line << "]: Unknown method '" << method << "' on type '" << objType << "'\n";
        errors = true;
        call->resolvedType = "unknown";
        return call->resolvedType;
    }

    if (functions.find(call->callee) == functions.end()) {
        std::cerr << "Bery:Error [Line " << call->line << "]: Undefined function '" << call->callee << "'\n";
        errors = true;
        call->resolvedType = "unknown";
        return call->resolvedType;
    }

    FunctionSignature& sig = functions[call->callee];
    if (sig.paramTypes.size() != call->arguments.size()) {
        std::cerr << "Bery:Error [Line " << call->line << "]: Function '" << call->callee << "' expects "
                   << sig.paramTypes.size() << " arguments, got " << call->arguments.size() << "\n";
        errors = true;
        call->resolvedType = "unknown";
        return call->resolvedType;
    }

    for (size_t i = 0; i < call->arguments.size(); ++i) {
        std::string argType = analyzeExpression(call->arguments[i].get());
        if (argType != "unknown" && argType != sig.paramTypes[i]) {
            if (!(sig.paramTypes[i] == "float"  && argType == "int") &&
                !(sig.paramTypes[i] == "double" && argType == "float") &&
                !(sig.paramTypes[i] == "double" && argType == "int") &&
                !(sig.paramTypes[i] == "bigint" && argType == "int")) {
                std::cerr << "Bery:Error [Line " << call->line << "]: Type mismatch in argument " << i+1
                           << " of '" << call->callee << "'. Expected '" << sig.paramTypes[i]
                           << "', got '" << argType << "'\n";
                errors = true;
            }
        }
    }

    call->resolvedType = sig.returnType;
    return call->resolvedType;
}

std::string TypeChecker::checkIndexExpr(ASTNode* node) {
    auto* idxNode = static_cast<IndexExprNode*>(node);
    if (!symbolTable.exists(idxNode->name)) {
        std::cerr << "Bery:Error [Line " << idxNode->line << "]: Undefined array '" << idxNode->name << "'\n";
        errors = true;
        idxNode->resolvedType = "unknown";
        return idxNode->resolvedType;
    }
    Symbol& sym = symbolTable.get(idxNode->name);
    if (sym.type.size() > 6 && sym.type.substr(0, 6) == "array<") {
        std::string elemType = sym.type.substr(6, sym.type.size() - 7);
        idxNode->resolvedType = elemType;
        return idxNode->resolvedType;
    }

    int dimCount = 0;
    size_t pos = sym.type.find('[');
    while (pos != std::string::npos) {
        dimCount++;
        pos = sym.type.find('[', pos + 1);
    }

    if (dimCount == 0) {
        std::cerr << "Bery:Error [Line " << idxNode->line << "]: Variable '" << idxNode->name << "' is not subscriptable\n";
        errors = true;
        idxNode->resolvedType = "unknown";
        return idxNode->resolvedType;
    }

    if (idxNode->indices.size() > (size_t)dimCount) {
        std::cerr << "Bery:Error [Line " << idxNode->line << "]: Too many indices for array '" << idxNode->name << "'\n";
        errors = true;
        idxNode->resolvedType = "unknown";
        return idxNode->resolvedType;
    }
    for (auto& index : idxNode->indices) analyzeExpression(index.get());
    std::string baseType = sym.type.substr(0, sym.type.find('['));
    idxNode->resolvedType = baseType;
    return idxNode->resolvedType;
}

std::string TypeChecker::checkAssignmentExpr(ASTNode* node) {
    auto* assign = static_cast<AssignmentExprNode*>(node);
    std::string targetName = "";
    std::string targetType = analyzeExpression(assign->target.get());
    std::string valueType = analyzeExpression(assign->value.get());

    if (assign->op != "=") {
        if (targetType != "int" && targetType != "float" && targetType != "double" && targetType != "bigint") {
            std::cerr << "Bery:Error [Line " << assign->line << "]: Cannot use compound assignment '" << assign->op << "' on type '" << targetType << "'\n";
            errors = true;
        }
    } 
    
    if (assign->target->type == NodeType::IDENT) {
        auto* ident = static_cast<IdentNode*>(assign->target.get());
        targetName = ident->name;

        if (!symbolTable.exists(ident->name)) {
            std::cerr << "Bery:Error [Line " << assign->line << "] : Undefined variable '" << ident->name << "'\n";
            errors = true;
            ident->resolvedType = "unknown";
            return ident->resolvedType;
        }
        Symbol& s = symbolTable.get(ident->name);
        if (s.isConst) {
            std::cerr << "Bery:Error [Line " << assign->line << "] : cannot reassign constant variable '" << ident->name << "'\n";
            errors = true;
            ident->resolvedType = "unknown";
            return ident->resolvedType;
        }
        s.isInitialized = true;
        targetType = s.type;

    } else if (assign->target->type == NodeType::INDEX_EXPR) {
        auto* idxNode = static_cast<IndexExprNode*>(assign->target.get());
        if (!symbolTable.exists(idxNode->name)) {
            std::cerr << "Bery:Error [Line " << idxNode->line << "]: Undefined array '" << idxNode->name << "'\n";
            errors = true;
            assign->resolvedType = "unknown";
            return assign->resolvedType;
        }
        Symbol& sym = symbolTable.get(idxNode->name);
        if (sym.type.substr(0, 6) == "array<") {
            analyzeExpression(assign->value.get());
            assign->resolvedType = "void";
            return assign->resolvedType;
        }
        targetName = idxNode->name;

        targetType = analyzeExpression(assign->target.get());
        if (targetType == "unknown") {
            assign->resolvedType = "unknown";
            return assign->resolvedType;
        }
    } else {
        std::cerr << "Bery:Error [Line " << assign->line << "] : Invalid assignment target\n";
        errors = true;
        assign->resolvedType = "unknown";
        return assign->resolvedType;
    }

    std::string exptype = analyzeExpression(assign->value.get());
    
    if (exptype != "unknown" && exptype != targetType) {
        if (!(targetType == "float" && exptype == "int") &&  !(targetType == "double" && exptype == "int") &&
            !(targetType == "bigint" && exptype == "int") &&  !(targetType == "double" && exptype == "float")) {
            
            std::cerr << "Bery:Error [Line " << assign->line << "] : Type missmatch for assignment to  '" << targetName << "'. Expected '" << targetType << "', got '" << exptype << "'\n";
            errors = true;
            assign->resolvedType = "unknown";
            return assign->resolvedType;
        }
    }
    assign->resolvedType = targetType;
    return assign->resolvedType;
}

std::string TypeChecker::checkCastExpr(ASTNode* node) {
    auto* castNode = static_cast<CastExprNode*>(node);
    std::string srcType = analyzeExpression(castNode->expr.get());
    castNode->srcType = srcType; 

    auto isPrimitive = [](const std::string& t) {
        return t == "int" || t == "bigint" || t == "float" || t == "double" || t == "char" || t == "bool";
    };

    if (!isPrimitive(srcType) || !isPrimitive(castNode->targetType)) {
        std::cerr << "Bery:Error [Line " << castNode->line << "]: Invalid cast from '" << srcType << "' to '"  << castNode->targetType   << "'.\n";
        errors = true;
        castNode->resolvedType = "unknown";
        return castNode->resolvedType;
    }
    castNode->resolvedType = castNode->targetType;
    return castNode->resolvedType;
}

std::string TypeChecker::checkIdentifier(ASTNode* node) {
    auto* ident = static_cast<IdentNode*>(node);
    size_t dot = ident->name.find('.');
    if (dot != std::string::npos) {
        std::string objName = ident->name.substr(0, dot);
        std::string propName = ident->name.substr(dot+1);
        if (!symbolTable.exists(objName)) {
            std::cerr << "Bery: Error: [Line " << ident->line << "]: Undefined variable '" << objName << "'\n";
            errors = true;
            node->resolvedType = "unknown";
            return node->resolvedType;
        }
        if(propName == "len") { ident->resolvedType = "int"; return ident->resolvedType; }
        std::cerr << "Bery:Error [Line " << ident->line << "]: Unknown property '" << propName << "'\n";
        errors = true;
        node->resolvedType = "unknown";
        return node->resolvedType;
    }
    if(!symbolTable.exists(ident->name)){
        std::cerr<<"Bery:Error [Line "<< ident->line <<"]: Undefined Variable '"<<ident->name<<"'\n";
        errors=true;
        node->resolvedType = "unknown";
        return node->resolvedType;
    }
    ident->resolvedType = symbolTable.get(ident->name).type;
    return ident->resolvedType;
}

std::string TypeChecker::checkLiteral(ASTNode* node) {
    switch (node->type) {
        case NodeType::INT_LIT:     
            node->resolvedType = "int";
            return node->resolvedType;
        case NodeType::DECIMAL_LIT:     
            node->resolvedType = "float";
            return node->resolvedType;
        case NodeType::CHAR_LIT:     
            node->resolvedType = "char";
            return node->resolvedType;
        case NodeType::BOOL_LIT:     
            node->resolvedType = "bool";
            return node->resolvedType;
        case NodeType::STRING_LIT:     
            node->resolvedType = "string";
            return node->resolvedType;  
        case NodeType::NULL_LIT:     
            node->resolvedType = "null";
            return node->resolvedType;
        default:
            std::cerr << "Bery:Error [Line " << node->line << "]: Unknown literal type\n";
            errors = true;
            node->resolvedType = "unknown";
            return node->resolvedType;
    }
}