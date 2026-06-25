#include "../codegen.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/literals.h"
#include "../../sema/symboltable.h"
#include "../../parser/ast/functions.h"
#include <iomanip>
#include <sstream>

std::string CodeGen::inferType(ASTNode* node) {
    if (!node) return "void";
    switch (node->type) {
        case NodeType::INT_LIT: return "int";
        case NodeType::DECIMAL_LIT: return "double";
        case NodeType::BOOL_LIT:return "bool";
        case NodeType::CHAR_LIT:  return "char";
        case NodeType::STRING_LIT:return "string";
        case NodeType::IDENT: {
            auto* ident = static_cast<IdentNode*>(node);
            return symTable.get(ident->name).type;
        }
        case NodeType::CAST_EXPR: {
            auto* cast = static_cast<CastExprNode*>(node);
            return cast->targetType;
        }
        case NodeType::CALL_EXPR: {
            auto* call = static_cast<CallExprNode*>(node);
            if (functions.count(call->callee))
                return functions[call->callee].returnType;
            return "void";
        }
        case NodeType::BINARY_EXPR: {
            auto* bin = static_cast<BinaryExprNode*>(node);
            std::string lt = inferType(bin->left.get());
            std::string rt = inferType(bin->right.get());
            if (lt == "double" || rt == "double") return "double";
            if (lt == "float"  || rt == "float")  return "float";
            if (lt == "bigint" || rt == "bigint") return "bigint";
            return lt;
        }
        case NodeType::TERNARY_EXPR: {
            auto* tern = static_cast<TernaryExprNode*>(node);
            return inferType(tern->trueExpr.get());
        }
        case NodeType::GROUPED_EXPR: {
            auto* grp = static_cast<GroupedExprNode*>(node);
            return inferType(grp->expression.get());
        }
        default: return "int";
    }
}

std::string CodeGen::genBREPrintCall(ASTNode* node, std::ostream& out) {
    auto* call = static_cast<CallExprNode*>(node);
    const std::string& callee = call->callee;

    auto emitPrint = [&](ASTNode* arg) {
        std::string argType = inferType(arg);
        std::string sym, llvmT;
        if      (argType == "int")    { sym = "__bery_print_int";    llvmT = "i32"; }
        else if (argType == "bigint") { sym = "__bery_print_bigint"; llvmT = "i64"; }
        else if (argType == "float")  { sym = "__bery_print_float";  llvmT = "float"; }
        else if (argType == "double") { sym = "__bery_print_double"; llvmT = "double"; }
        else if (argType == "bool")   { sym = "__bery_print_bool";   llvmT = "i1"; }
        else if (argType == "char")   { sym = "__bery_print_char";   llvmT = "i8"; }
        else if (argType == "string") { 
            bool isLiteral = (call->arguments[0]->type == NodeType::STRING_LIT);
            if (isLiteral) {
                sym = "__bery_print_cstr"; llvmT = "i8*";
            } else {
                sym = "__bery_print_string"; llvmT = "i8*";
            }
        }
        else                          { sym = "__bery_print_int";    llvmT = "i32"; }

        emitBREDecl("declare void @" + sym + "(" + llvmT + ")", sym);
        std::string argReg = genExpression(arg, argType, out);
        out << "    call void @" << sym << "(" << llvmT << " " << argReg << ")\n";
    };

    if (callee == "print") {
        emitPrint(call->arguments[0].get());
        return "0";
    }

    if (callee == "println") {
        if (!call->arguments.empty()) {
            emitPrint(call->arguments[0].get());
        }
        emitBREDecl("declare void @bery_println()", "bery_println");
        out << "    call void @bery_println()\n";
        return "0";
    }
    struct InputMapping {
        const char* callee;
        const char* sym;
        const char* llvmRet;
    };
    static const InputMapping inputMap[] = {
        {"inputInt",    "__bery_input_int",    "i32"},
        {"inputBigInt", "__bery_input_bigint", "i64"},
        {"inputFloat",  "__bery_input_float",  "float"},
        {"inputDouble", "__bery_input_double", "double"},
        {"inputBool",   "__bery_input_bool",   "i1"},
        {"inputChar",   "__bery_input_char",   "i8"},
        {"inputString", "bery_input",          "i8*"},
    };

    for (auto& m : inputMap) {
        if (callee == m.callee) {
            emitBREDecl(
                std::string("declare ") + m.llvmRet + " @" + m.sym + "(i8*)",
                m.sym
            );
            std::string promptReg = genExpression(call->arguments[0].get(), "string", out);
            std::string resReg = newReg();
            out << "    " << resReg << " = call " << m.llvmRet
                << " @" << m.sym << "(i8* " << promptReg << ")\n";
            return resReg;
        }
    }

    return "0";
}
std::string CodeGen::genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    if(!node){return "0";}
    if (node->type == NodeType::NULL_LIT) {return "null";}
    std::string lt = llvmType(expectedType);
    bool isFloat = (expectedType=="float" || expectedType=="double");
    if (node->type == NodeType::INT_LIT) {
        auto* lit = static_cast<IntLitNode*>(node);
        if (isFloat) {
            std::string tmp = newReg();
            std::string reg = newReg();
            out << "    " << tmp << " = add i32 0, " << lit->value << "\n";
            out << "    " << reg << " = sitofp i32 " << tmp << " to " << lt << "\n";
            return reg;
        } else {
            std::string reg = newReg();
            out << "    " << reg << " = add " << lt << " 0, " << lit->value << "\n";
            return reg;
        }
    }
    if (node->type == NodeType::DECIMAL_LIT) {
        auto* lit = static_cast<DecimalLitNode*>(node);
        std::ostringstream ss;
        ss << std::scientific << std::setprecision(17) << lit->value;
        std::string reg = newReg();
        out << "    " << reg << " = fadd " << lt << " 0.0, " << ss.str() << "\n";
        return reg;
    }
    if (node->type == NodeType::BOOL_LIT) {
            std::string reg = newReg();
        auto* lit = static_cast<BoolLitNode*>(node);
        out << "    " << reg << " = add i1 0, " << (lit->value? 1:0) << "\n";
        return reg;
    }
    if (node->type == NodeType::CHAR_LIT) {
            std::string reg = newReg();
        auto* lit = static_cast<CharLitNode*>(node);
        out << "    " << reg << " = add " << lt << " 0, " << (int)lit->value << "\n"; //for ascii
        return reg;
    }
    if (node->type == NodeType::STRING_LIT) {
            auto* lit = static_cast<StringLitNode*>(node);
            std::string strVal = lit->value;
            std::string escapedStr = escapeLLVMString(strVal);

            int strlen = strVal.length() + 1;
            std::string globalName = "@.str." + std::to_string(strCounter++);

            globalStrings << globalName << " = private unnamed_addr constant [" << strlen << " x i8] c\"" <<escapedStr << "\"\n";
            std::string reg = newReg();
            out << "    " << reg << " = getelementptr [" << strlen << " x i8], [" << strlen << " x i8]* " << globalName << ", i32 0, i32 0\n";

            return reg;
    }
    if(node->type == NodeType::IDENT){
        auto* ident = static_cast<IdentNode*>(node);
        Symbol& sym = symTable.get(ident->name);
        std::string realType = sym.type;
        if (realType.back() == ']') {
            realType = realType.substr(0, realType.find('['));
        }
        
        std::string realLT = llvmType(realType);
        std::string reg = newReg();
        std::string memoryPtr = sym.llvmRegister;
        out << "    " << reg << " = load " << realLT << ", " << realLT << "* " << memoryPtr << "\n";
        bool isParentFloat = (expectedType == "float" || expectedType == "double");
        if (isParentFloat && (realType == "int" || realType == "bigint")) {
            std::string castReg = newReg();
            out << "    " << castReg << " = sitofp " << realLT << " " << reg << " to " << llvmType(expectedType) << "\n";
            return castReg;
        }
        
        return reg;
    }
    if(node->type == NodeType::GROUPED_EXPR){
        auto* group = static_cast<GroupedExprNode*>(node);
        return genExpression(group->expression.get(), expectedType, out);
    }
    if(node->type== NodeType::UNARY_EXPR){
        auto* unary = static_cast<UnaryExprNode*>(node);
        if(unary->optr == "-" || unary->optr == "!" || unary->optr == "~"){
            std::string opreg = genExpression(unary->operand.get(), expectedType, out);
            std::string reg = newReg();
            
            if(unary->optr == "-"){
                out << "    " << reg << " = " << (isFloat?"fsub ":"sub ") << lt << " " << (isFloat?"0.0":"0") << ", " << opreg << "\n";
            }
            else if(unary->optr == "!"){
                out << "    " << reg << " = xor i1 " << opreg << ", 1\n";

            }
            else if(unary->optr == "~"){
                out << "    " << reg << " = xor " << lt << " " << opreg << ", -1\n";
            }
            return reg;

        }
        if (unary->optr == "--" || unary->optr == "++" || unary->optr == "post++" || unary->optr == "post--") {
            if (unary->operand->type != NodeType::IDENT) return "0";
            auto* ident = static_cast<IdentNode*>(unary->operand.get());

            std::string memoryPtr = symTable.get(ident->name).llvmRegister;
            std::string oldvalreg = genExpression(unary->operand.get(), expectedType, out);
            std::string newvalreg = newReg();
            std::string one = isFloat ? "1.0" : "1";
            std::string opins;
            if (unary->optr == "++" || unary->optr == "post++") {
                opins = isFloat ? "fadd " : "add ";
            } else {
                opins = isFloat ? "fsub " : "sub ";
            }
            out << "    " << newvalreg << " = " << opins << lt << " " << oldvalreg << ", " << one << "\n";
            out << "    store " << lt << " " << newvalreg << ", " << lt << "* " << memoryPtr << "\n";
            if (unary->optr == "post++" || unary->optr == "post--") {
                return oldvalreg;
            } else {
                return newvalreg;
            }
        }
    }
    if (node->type == NodeType::BETWEEN_EXPR) {
        auto* bet = static_cast<BetweenExprNode*>(node);

        std::string operatorLLVMtype = llvmType(bet->opType);
        bool isFloatingPoint = (bet->opType == "float" || bet->opType == "double");

        std::string tReg = genExpression(bet->value.get(), bet->opType, out);
        std::string lReg = genExpression(bet->lower.get(), bet->opType, out);
        std::string uReg = genExpression(bet->upper.get(), bet->opType, out);

        std::string comparison1 = newReg();
        if (isFloatingPoint) {
            out << "    " << comparison1 << " = fcmp oge " << operatorLLVMtype << " "<< tReg << ", " << lReg << "\n";
        } else {out << "    " << comparison1 << " = icmp sge " << operatorLLVMtype << " "<< tReg << ", " << lReg << "\n";
        }
        std::string comparison2 = newReg(); 
        if (isFloatingPoint) {
            out << "    " << comparison2 << " = fcmp ole " << operatorLLVMtype << " "<< tReg << ", " << uReg << "\n"; // <--- FIXED THIS LINE
        } else {
            out << "    " << comparison2 << " = icmp sle " << operatorLLVMtype << " "<< tReg << ", " << uReg << "\n"; // <--- FIXED THIS LINE
        }

        std::string andReg = newReg();
        out << "    " << andReg << " = and i1 " << comparison1 << ", " << comparison2 << "\n"; // <--- FIXED THIS LINE

        if (bet->isNegated) {
            std::string notBetweenReg = newReg();
            out << "    " << notBetweenReg << " = xor i1 " << andReg << ", 1\n";
            return notBetweenReg; 
        }
        return andReg;
    }
    if(node->type == NodeType::BINARY_EXPR){
        auto* binary = static_cast<BinaryExprNode*>(node);

        if (binary->optr == "&&" || binary->optr == "||") {
            std::string resultAllocation = newReg();
            out << "    " << resultAllocation << " = alloca i1\n";

            std::string leftReg = genExpression(binary->left.get(), "bool", out);
            out << "    store i1 " << leftReg << ", i1* " << resultAllocation << "\n";

            int blockId = ++regCounter;
            std::string rightBlock = "logic_right_" + std::to_string(blockId);
            std::string endBlock = "logic_end_" + std::to_string(blockId);

            if (binary->optr == "&&") {
                out << "    br i1 " << leftReg << ", label %" << rightBlock << ", label %" << endBlock <<"\n";
            } else  {
                out << "    br i1 " << leftReg << ", label %" << endBlock << ", label %" << rightBlock <<"\n";
            }

            out << "\n" << rightBlock << ":\n";
            std::string rightReg = genExpression(binary->right.get(), "bool", out);
            out << "    store i1 " << rightReg << ", i1* " << resultAllocation << "\n";
            out << "    br label %" << endBlock <<"\n";

            out << "\n" << endBlock << ":\n";
            std::string finalReg = newReg();
            out << "    " << finalReg << " = load i1, i1* " << resultAllocation << "\n";

            return finalReg;
        }
        std::string opLT = llvmType(binary->opType);
        bool isOpFloat = (binary->opType == "float" || binary->opType == "double");

        std::string leftReg = genExpression(binary->left.get(), binary->opType, out);
        std::string rightReg = genExpression(binary->right.get(), binary->opType, out);
        std::string resultReg = newReg();

        if(isOpFloat){
            if(binary->optr == "+"){
                out << "    " << resultReg << " = fadd " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "-"){
                out << "    " << resultReg << " = fsub " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "*"){
                out << "    " << resultReg << " = fmul " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "/"){
                out << "    " << resultReg << " = fdiv " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "**"){
                if(expectedType == "float"){
                    std::string leftDouble = newReg();
                    std::string rightDouble = newReg();
                    out << "    " << leftDouble << " = fpext float " << leftReg << " to double\n";
                    out << "    " << rightDouble << " = fpext float " << rightReg << " to double\n";
                    
                    std::string powReg = newReg();
                    out << "    " << powReg << " = call double @llvm.pow.f64(double " << leftDouble << ", double " << rightDouble << ")\n";
                    out << "    " << resultReg << " = fptrunc double " << powReg << " to float\n";
                }else{
                    out << "    " << resultReg << " = call double @llvm.pow.f64(double " << leftReg << ", double " << rightReg << ")\n";
                }
            }else if (binary->optr == "==") {
                out << "    " << resultReg << " = fcmp oeq " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "!=") {
                out << "    " << resultReg << " = fcmp one " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == ">") {
                out << "    " << resultReg << " = fcmp ogt " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "<") {
                out << "    " << resultReg << " = fcmp olt " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == ">=") {
                out << "    " << resultReg << " = fcmp oge " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "<=") {
                out << "    " << resultReg << " = fcmp ole " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }
            else{
                return "0";
            }
        }else{
            if(binary->optr == "+"){
                out << "    " << resultReg << " = add " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "-"){
                out << "    " << resultReg << " = sub " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "*"){
                out << "    " << resultReg << " = mul " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "/"){
                out << "    " << resultReg << " = sdiv " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "%"){
                out << "    " << resultReg << " = srem " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "**"){
                std::string leftDouble = newReg();
                std::string rightDouble = newReg();

                out << "    " << leftDouble << " = sitofp " << opLT << " " << leftReg << " to double\n";
                out << "    " << rightDouble << " = sitofp " << opLT << " " << rightReg << " to double\n";

                std::string powReg = newReg();

                out << "    " << powReg << " = call double @llvm.pow.f64(double " << leftDouble << ", double " << rightDouble << ")\n";
                out << "    " << resultReg << " = fptosi double " << powReg << " to " << opLT << "\n";
            } else if (binary->optr == "<<") {
                out << "    " << resultReg << " = shl " << opLT << " " << leftReg << ", " << rightReg << "\n"; 
            } else if (binary->optr == ">>") {
                out << "    " << resultReg << " = ashr "<< opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "&") {
                out << "    " << resultReg << " = and " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "|") {
                out << "    " << resultReg << " = or "  << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "^") {
                out << "    " << resultReg << " = xor " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }else if (binary->optr == "==") {
                out << "    " << resultReg << " = icmp eq " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "!=") {
                out << "    " << resultReg << " = icmp ne " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == ">") {
                out << "    " << resultReg << " = icmp sgt " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "<") {
                out << "    " << resultReg << " = icmp slt " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == ">=") {
                out << "    " << resultReg << " = icmp sge " << opLT << " " << leftReg << ", " << rightReg << "\n";
            } else if (binary->optr == "<=") {
                out << "    " << resultReg << " = icmp sle " << opLT << " " << leftReg << ", " << rightReg << "\n";
            }
            else{
                return "0";
            }
        }

        std::string finalReg = resultReg;
        bool expectFloat = (expectedType == "float" || expectedType == "double");
        bool gotInt = (binary->opType == "int" || binary->opType == "bigint");

        if (expectFloat && gotInt) {
            finalReg = newReg();
            out << "    " << finalReg << " = sitofp " << llvmType(binary->opType) << " " << resultReg << " to " << llvmType(expectedType) << "\n";
        }

        return finalReg;
    }
    if (node->type == NodeType::TERNARY_EXPR) {
       auto* tern = static_cast<TernaryExprNode*>(node);
       std::string llvmResType = llvmType(tern->resolvedType);
       std::string resultAlloc = newReg();
       out << "    " << resultAlloc << " = alloca " << llvmResType << "\n";

       std::string condReg = genExpression(tern->condition.get(), "bool", out);
       int blockId = ++regCounter;
       std::string trueBlock = "tern_true_" + std::to_string(blockId);
       std::string falseBlock = "tern_false_" + std::to_string(blockId);
       std::string endBlock = "tern_end_" + std::to_string(blockId);

       out << "    br i1 " << condReg << ", label %" << trueBlock << ", label %" << falseBlock << "\n";

       out << "\n" << trueBlock << ":\n";
       std::string tReg = genExpression(tern->trueExpr.get(), tern->resolvedType, out);
       out << "    store " << llvmResType << " " << tReg << ", " << llvmResType << "* " << resultAlloc << "\n";
       out << "    br label %" << endBlock << "\n";


       out << "\n" << falseBlock << ":\n";
       std::string fReg = genExpression(tern->falseExpr.get(), tern->resolvedType, out);
       out << "    store " << llvmResType << " " << fReg << ", " << llvmResType << "* " << resultAlloc << "\n";
       out << "    br label %" << endBlock << "\n";

       out << "\n" << endBlock << ":\n";
       std::string finalReg = newReg();
       out << "    " << finalReg << " = load " << llvmResType << ", " << llvmResType << "* " << resultAlloc << "\n";
       return finalReg;
    }
    if (node->type == NodeType::ASSIGNMENT_EXPR) {
        auto* assign = static_cast<AssignmentExprNode*>(node);
        std::string targetLT;
        std::string memoryPtr;
        std::string expectedType;

        if (assign->target->type == NodeType::IDENT) {
            auto* ident = static_cast<IdentNode*>(assign->target.get());
            Symbol& sym = symTable.get(ident->name);
            targetLT = llvmType(sym.type);
            memoryPtr = sym.llvmRegister;
            expectedType = sym.type;
        } else if (assign->target->type == NodeType::INDEX_EXPR) {
            auto* idxNode = static_cast<IndexExprNode*>(assign->target.get());
            Symbol& sym = symTable.get(idxNode->name);
            if (sym.type.size() > 6 && sym.type.substr(0, 6) == "array<") {
                std::string elemType = sym.type.substr(6, sym.type.size() - 7);
                std::string lt = llvmType(elemType);
                emitBREDecl("declare void @bery_array_set(i8*, i64, i8*)", "bery_array_set");
                std::string arrReg = newReg();
                out << "    " << arrReg << " = load i8*, i8** " << sym.llvmRegister << "\n";

                std::string idxReg = genExpression(idxNode->indices[0].get(), "int", out);
                std::string idxExt = newReg();
                out << "    " << idxExt << " = sext i32 " << idxReg << " to i64\n";
                std::string valReg = genExpression(assign->value.get(), elemType, out);

                std::string slotReg = newReg();
                out << "    " << slotReg << " = alloca " << lt << "\n";
                out << "    store " << lt << " " << valReg << ", " << lt << "* " << slotReg << "\n";
                std::string castReg = newReg();
                out << "    " << castReg << " = bitcast " << lt << "* " << slotReg << " to i8*\n";
                out << "    call void @bery_array_set(i8* " << arrReg << ", i64 " << idxExt << ", i8* " << castReg << ")\n";
                return valReg;
            }
            
            std::string baseType = sym.type.substr(0, sym.type.find('['));
            targetLT = llvmType(baseType);
            expectedType = baseType;
            std::string arrType = sym.llvmAllocType;
            std::vector<std::string> indexRegisters;
            for (auto& idx : idxNode->indices) {
                indexRegisters.push_back(genExpression(idx.get(), "int", out));
            }

            memoryPtr = newReg();
            out << "    " << memoryPtr << " = getelementptr " << arrType << ", " << arrType << "* " << sym.llvmRegister << ", i32 0";
            for (const std::string& reg : indexRegisters) {
                out << ", i32 " << reg;
            }
            out << "\n";
        }
        std::string valReg = genExpression(assign->value.get(), expectedType, out);
        if (assign->op == "=") {
            out << "    store " << targetLT << " " << valReg << ", " << targetLT << "* " << memoryPtr << "\n";
            return valReg;
        } else {
            std::string currentVal = newReg();
            out << "    " << currentVal << " = load " << targetLT << ", " << targetLT << "* " << memoryPtr << "\n";
            
            std::string resultReg = newReg();
            bool isFloat = (expectedType == "float" || expectedType == "double");

            if (isFloat) {
                if (assign->op == "+=") out << "    " << resultReg << " = fadd " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "-=") out << "    " << resultReg << " = fsub " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "*=") out << "    " << resultReg << " = fmul " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "/=") out << "    " << resultReg << " = fdiv " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "**=") {
                    std::string leftDouble = newReg();
                    std::string rightDouble = newReg();
                    if (expectedType == "float") {
                        out << "    " << leftDouble << " = fpext float " << currentVal << " to double\n";
                        out << "    " << rightDouble << " = fpext float " << valReg << " to double\n";
                        std::string powReg = newReg();
                        out << "    " << powReg << " = call double @llvm.pow.f64(double " << leftDouble << ", double " << rightDouble << ")\n";
                        out << "    " << resultReg << " = fptrunc double " << powReg << " to float\n";
                    } else {
                        out << "    " << resultReg << " = call double @llvm.pow.f64(double " << currentVal << ", double " << valReg << ")\n";
                    }
                }
            } else {
                if (assign->op == "+=") out << "    " << resultReg << " = add " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "-=") out << "    " << resultReg << " = sub " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "*=") out << "    " << resultReg << " = mul " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "/=") out << "    " << resultReg << " = sdiv " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "%=") out << "    " << resultReg << " = srem " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "<<=") out << "    " << resultReg << " = shl " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == ">>=") out << "    " << resultReg << " = ashr " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "&=") out << "    " << resultReg << " = and " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "|=") out << "    " << resultReg << " = or " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "^=") out << "    " << resultReg << " = xor " << targetLT << " " << currentVal << ", " << valReg << "\n";
                else if (assign->op == "**=") {
                    std::string leftDouble = newReg();
                    std::string rightDouble = newReg();
                    out << "    " << leftDouble << " = sitofp " << targetLT << " " << currentVal << " to double\n";
                    out << "    " << rightDouble << " = sitofp " << targetLT << " " << valReg << " to double\n";
                    std::string powReg = newReg();
                    out << "    " << powReg << " = call double @llvm.pow.f64(double " << leftDouble << ", double " << rightDouble << ")\n";
                    out << "    " << resultReg << " = fptosi double " << powReg << " to " << targetLT << "\n";
                }
            }
            
            out << "    store " << targetLT << " " << resultReg << ", " << targetLT << "* " << memoryPtr << "\n";
            return resultReg;
        }
    }
    if (node->type == NodeType::CAST_EXPR) {
        auto* castNode = static_cast<CastExprNode*>(node);
        std::string srcReg = genExpression(castNode->expr.get(), castNode->srcType, out);
        
        std::string sType = castNode->srcType;
        std::string tType = castNode->targetType;

        if (sType == tType) return srcReg;

        std::string sLLVM = llvmType(sType);
        std::string tLLVM = llvmType(tType);
        std::string resReg = newReg();

        bool srcIsFloat = (sType == "float" || sType == "double");
        bool tgtIsFloat = (tType == "float" || tType == "double");

        if (srcIsFloat && !tgtIsFloat) {
            out << "    " << resReg << " = fptosi " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
        } 
        else if (!srcIsFloat && tgtIsFloat) {
            out << "    " << resReg << " = sitofp " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
        } 
        else if (srcIsFloat && tgtIsFloat) {
            if (sType == "float" && tType == "double")
                out << "    " << resReg << " = fpext float " << srcReg << " to double\n";
            else
                out << "    " << resReg << " = fptrunc double " << srcReg << " to float\n";
        }
        else {
            int sWidth = (sType == "bigint") ? 64 : (sType == "int" ? 32 : (sType == "char" ? 8 : 1));
            int tWidth = (tType == "bigint") ? 64 : (tType == "int" ? 32 : (tType == "char" ? 8 : 1));

            if (sWidth > tWidth) {
                out << "    " << resReg << " = trunc " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
            } else if (sWidth < tWidth) {
                if (sType == "bool") {
                    out << "    " << resReg << " = zext " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
                } else {
                    out << "    " << resReg << " = sext " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
                }
            } else {
                return srcReg; 
            }
        }
        return resReg;
    }
    if (node->type == NodeType::INDEX_EXPR) {
        auto* idx = static_cast<IndexExprNode*>(node);
        Symbol& sym = symTable.get(idx->name);
        if (sym.type.substr(0, 6) == "array<") {
            std::string elemType = sym.type.substr(6, sym.type.size() - 7);
            std::string lt = llvmType(elemType);
            emitBREDecl("declare i8* @bery_array_get(i8*, i64)", "bery_array_get");
            std::string arrReg = newReg();
            out << "    " << arrReg << " = load i8*, i8** " << sym.llvmRegister << "\n";
            std::string idxReg = genExpression(idx->indices[0].get(), "int", out);
            std::string idxExt = newReg();
            out << "    " << idxExt << " = sext i32 " << idxReg << " to i64\n";
            std::string rawReg = newReg();
            out << "    " << rawReg << " = call i8* @bery_array_get(i8* " << arrReg << ", i64 " << idxExt << ")\n";
            std::string castReg = newReg();
            out << "    " << castReg << " = bitcast i8* " << rawReg << " to " << lt << "*\n";

            std::string valReg = newReg();
            out << "    " << valReg << " = load " << lt << ", " << lt << "* " << castReg << "\n";
            return valReg;
        }
        auto* idxNode = static_cast<IndexExprNode*>(node);
        
        std::string baseType = sym.type.substr(0, sym.type.find('['));
        std::string lt = llvmType(baseType);
        std::string arrType = sym.llvmAllocType;
        std::vector<std::string> indexRegisters;
        for (auto& idx : idxNode->indices) {
            indexRegisters.push_back(genExpression(idx.get(), "int", out));
        }

        std::string ptrReg = newReg();
        out << "    " << ptrReg << " = getelementptr " << arrType << ", " << arrType << "* " << sym.llvmRegister << ", i32 0";

        for (const std::string& reg : indexRegisters) {
            out << ", i32 " << reg;
        }
        out << "\n";
        
        std::string valReg = newReg();
        out << "    " << valReg << " = load " << lt << ", " << lt << "* " << ptrReg << "\n";
        return valReg;
    }
    if (node->type == NodeType::CALL_EXPR) {
        auto* call = static_cast<CallExprNode*>(node);

        static const std::unordered_set<std::string> brePrintFns = {
            "print", "println", "inputInt", "inputBigInt", "inputFloat",
            "inputDouble", "inputBool", "inputChar", "inputString"
        };
        if (brePrintFns.count(call->callee)) {
            return genBREPrintCall(node, out);
        }
        CodeGenFunctionSignature& sig = functions[call->callee];
        
        std::string argsStr = "";
        for (size_t i = 0; i < call->arguments.size(); ++i) {
            std::string argReg = genExpression(call->arguments[i].get(), sig.paramTypes[i], out);
            argsStr += llvmType(sig.paramTypes[i]) + " " + argReg;
            if (i + 1 < call->arguments.size()) argsStr += ", ";
        }
        
        if (sig.returnType == "void") {
            out << "    call void @" << call->callee << "(" << argsStr << ")\n";
            return "0";
        } else {
            std::string resReg = newReg();
            out << "    " << resReg << " = call " << llvmType(sig.returnType) << " @" << call->callee << "(" << argsStr << ")\n";
            return resReg;
        }
    }
   return "0";
}
