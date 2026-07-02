#include "../codegen.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/literals.h"
#include "../../sema/symboltable.h"
#include "../../parser/ast/functions.h"
#include <iomanip>
#include <sstream>

static std::vector<std::string> splitDots(const std::string& s) {
    std::vector<std::string> parts;
    size_t start =0, pos;
    while ((pos = s.find('.', start)) !=std::string::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos+1;  
    }
    parts.push_back(s.substr(start));
    return parts;
}

std::string CodeGen::emitBinaryOp(const std::string& op, const std::string& llvmT, bool isFloat, const std::string& lReg, const std::string& rReg, std::ostream& out) {
    std::string res = newReg();
    static const std::unordered_map<std::string, std::pair<std::string,std::string>> opMap = {
        {"+",  {"fadd", "add" }},
        {"-",  {"fsub", "sub" }},
        {"*",  {"fmul", "mul" }},
        {"/",  {"fdiv", "sdiv"}},
        {"%",  {"","srem"}},
        {"<<", {"","shl" }},
        {">>", {"","ashr"}},
        {"&",  {"","and" }},
        {"|",  {"","or"  }},
        {"^",  {"","xor" }},
        {"==", {"fcmp oeq", "icmp eq" }},
        {"!=", {"fcmp one", "icmp ne" }},
        {">",  {"fcmp ogt", "icmp sgt"}},
        {"<",  {"fcmp olt", "icmp slt"}},
        {">=", {"fcmp oge", "icmp sge"}},
        {"<=", {"fcmp ole", "icmp sle"}},
    };

    auto it = opMap.find(op);
    if (it == opMap.end()) return "0";

    std::string ins = isFloat ? it->second.first : it->second.second;
    if (ins.empty()) return "0";

    out << "    " << res << " = " << ins << " " << llvmT << " " << lReg << ", " << rReg << "\n";
    return res;
}
std::string CodeGen::genLiteral(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    std::string lt = llvmType(expectedType);
    bool isFloat = (expectedType == "float" || expectedType == "double");

    if (node->type == NodeType::INT_LIT) {
        auto* lit = static_cast<IntLitNode*>(node);
        if (isFloat) {
            std::string reg = newReg();
            out << "    " << reg << " = sitofp i32 " << lit->value << " to " << lt << "\n";
            return reg;
        }
        return std::to_string(lit->value);
    }

    if (node->type == NodeType::DECIMAL_LIT) {
        auto* lit = static_cast<DecimalLitNode*>(node);
        std::ostringstream ss;
        ss << std::scientific << std::setprecision(17) << lit->value;
        return ss.str();
    }

    if (node->type == NodeType::BOOL_LIT) {
        return static_cast<BoolLitNode*>(node)->value ? "1" : "0";
    }

    if (node->type == NodeType::CHAR_LIT) {
        return std::to_string((int)static_cast<CharLitNode*>(node)->value);
    }

    if (node->type == NodeType::STRING_LIT) {
        auto* lit    = static_cast<StringLitNode*>(node);
        std::string escaped = escapeLLVMString(lit->value);
        int len      = lit->value.length() + 1;
        std::string globalName = "@.str." + std::to_string(strCounter++);
        globalStrings << globalName << " = private unnamed_addr constant [" << len << " x i8] c\"" << escaped << "\"\n";
        std::string rawReg = newReg();
        out << "    " << rawReg << " = getelementptr [" << len << " x i8], [" << len << " x i8]* " << globalName << ", i32 0, i32 0\n";
        emitBREDecl("declare i8* @bery_string_from_literal(i8*)", "bery_string_from_literal");
        std::string objReg = newReg();
        out << "    " << objReg << " = call i8* @bery_string_from_literal(i8* " << rawReg << ")\n";
        return objReg;
    }

    return "0";
}

std::string CodeGen::genIdentExpr(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    auto* ident = static_cast<IdentNode*>(node);

    size_t dot = ident->name.find('.');
    if (dot !=std::string::npos) {
        std::vector<std::string> parts =splitDots(ident->name);

        if (parts.back() == "len") {
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string headType;
            std::string headPtr = genFieldChainAddressing(headParts, out, headType);
            if (headType == "string") {
                emitBREDecl("declare i64 @bery_string_length(i8*)", "bery_string_length");
                std::string strReg = emitLoad("i8*", headPtr, out);
                std::string lenReg = newReg();
                out << "    " << lenReg << " = call i64 @bery_string_length(i8* " << strReg << ")\n";
                return emitSext("i64", lenReg,"i32", out);
            } if (headType.size() > 6 && headType.substr(0, 6) == "array<") {
                emitBREDecl("declare i64 @bery_array_length(i8*)", "bery_array_length");
                std::string arrReg = emitLoad("i8*", headPtr, out);
                std::string lenReg = newReg();
                out << "    " << lenReg << " = call i64 @bery_array_length(i8* " << arrReg << ")\n";
                std::string truncReg = newReg();
                out << "    " << truncReg << " = trunc i64 " << lenReg << " to i32\n";
                return truncReg;
            }
        }

        std::string chainType;
        std::string chainPtr = genFieldChainAddressing(parts, out, chainType);
        std::string flt      = llvmType(chainType);
        std::string valReg   = emitLoad(flt, chainPtr, out);

        bool isParentFloat = (expectedType == "float" || expectedType == "double");
        if (isParentFloat && (chainType == "int" || chainType == "bigint")) {
            std::string castReg = newReg();
            out << "    " << castReg << " = sitofp " << flt << " " << valReg << " to " << llvmType(expectedType) << "\n";
            return castReg;
        }
        return valReg;
    }
    Symbol& sym = symTable.get(ident->name);
    std::string realType = sym.type;
    if (realType.back() == ']')
        realType = realType.substr(0, realType.find('['));

    std::string realLT = llvmType(realType);
    std::string reg    = emitLoad(realLT, sym.llvmRegister, out);

    bool isParentFloat = (expectedType == "float" || expectedType == "double");
    if (isParentFloat && (realType == "int" || realType == "bigint")) {
        std::string castReg = newReg();
        out << "    " << castReg << " = sitofp " << realLT << " " << reg << " to " << llvmType(expectedType) << "\n";
        return castReg;
    }
    return reg;
}

std::string CodeGen::genUnaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    auto* unary = static_cast<UnaryExprNode*>(node);
    std::string lt      = llvmType(expectedType);
    bool isFloat        = (expectedType == "float" || expectedType == "double");

    if (unary->optr == "-" || unary->optr == "!" || unary->optr == "~") {
        std::string opReg = genExpression(unary->operand.get(), expectedType, out);
        std::string reg   = newReg();
        if (unary->optr == "-")
            out << "    " << reg << " = " << (isFloat ? "fsub " : "sub ") << lt << " " << (isFloat ? "0.0" : "0") << ", " << opReg << "\n";
        else if (unary->optr == "!")
            out << "    " << reg << " = xor i1 " << opReg << ", 1\n";
        else
            out << "    " << reg << " = xor " << lt << " " << opReg << ", -1\n";
        return reg;
    }

    if (unary->operand->type != NodeType::IDENT) return "0";
    auto* ident         = static_cast<IdentNode*>(unary->operand.get());
    std::string memPtr  = symTable.get(ident->name).llvmRegister;
    std::string oldReg  = genExpression(unary->operand.get(), expectedType, out);
    std::string newReg_ = newReg();
    std::string one     = isFloat ? "1.0" : "1";
    std::string opIns   = (unary->optr == "++" || unary->optr == "post++")
                            ? (isFloat ? "fadd " : "add ")
                            : (isFloat ? "fsub " : "sub ");
    out << "    " << newReg_ << " = " << opIns << lt << " " << oldReg << ", " << one << "\n";
    emitStore(lt, newReg_, memPtr, out);
    return (unary->optr == "post++" || unary->optr == "post--") ? oldReg : newReg_;
}

std::string CodeGen::genBetweenExpr(ASTNode* node, std::ostream& out) {
    auto* bet= static_cast<BetweenExprNode*>(node);
    std::string opLT= llvmType(bet->resolvedType);
    bool isFloat = (bet->resolvedType == "float" || bet->resolvedType == "double");

    std::string tReg = genExpression(bet->value.get(), bet->resolvedType, out);
    std::string lReg = genExpression(bet->lower.get(), bet->resolvedType, out);
    std::string uReg = genExpression(bet->upper.get(), bet->resolvedType, out);

    std::string cmp1 = newReg();
    std::string cmp2 = newReg();
    out << "    " << cmp1 << " = " << (isFloat ? "fcmp oge " : "icmp sge ") << opLT << " " << tReg << ", " << lReg << "\n";
    out << "    " << cmp2 << " = " << (isFloat ? "fcmp ole " : "icmp sle ") << opLT << " " << tReg << ", " << uReg << "\n";

    std::string andReg  = newReg();
    out << "    " << andReg << " = and i1 " << cmp1 << ", " << cmp2 << "\n";

    if (bet->isNegated) {
        std::string notReg = newReg();
        out << "    " << notReg << " = xor i1 " << andReg << ", 1\n";
        return notReg;
    }
    return andReg;
}

std::string CodeGen::genBinaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    auto* binary     = static_cast<BinaryExprNode*>(node);
    std::string opLT = llvmType(binary->resolvedType);
    bool isOpFloat   = (binary->resolvedType == "float" || binary->resolvedType == "double");
    if (binary->resolvedType == "string") {
        std::string lReg = genExpression(binary->left.get(),  "string", out);
        std::string rReg = genExpression(binary->right.get(), "string", out);
        if (binary->optr == "+") {
            emitBREDecl("declare i8* @bery_string_concat(i8*, i8*)", "bery_string_concat");
            std::string res = newReg();
            out << "    " << res << " = call i8* @bery_string_concat(i8* " << lReg << ", i8* " << rReg << ")\n";
            return res;
        }
        emitBREDecl("declare i1 @bery_string_equals(i8*, i8*)", "bery_string_equals");
        std::string eqReg = newReg();
        out << "    " << eqReg << " = call i1 @bery_string_equals(i8* " << lReg << ", i8* " << rReg << ")\n";
        if (binary->optr == "!=") {
            std::string neqReg = newReg();
            out << "    " << neqReg << " = xor i1 " << eqReg << ", 1\n";
            return neqReg;
        }
        return eqReg;
    }
    if (binary->optr == "&&" || binary->optr == "||") {
        std::string resAlloc = emitAlloca("i1", out);
        std::string lReg     = genExpression(binary->left.get(), "bool", out);
        emitStore("i1", lReg, resAlloc, out);
        int id = ++regCounter;
        std::string rightBlk = "logic_right_" + std::to_string(id);
        std::string endBlk   = "logic_end_"   + std::to_string(id);
        if (binary->optr == "&&")
            out << "    br i1 " << lReg << ", label %" << rightBlk << ", label %" << endBlk << "\n";
        else
            out << "    br i1 " << lReg << ", label %" << endBlk << ", label %" << rightBlk << "\n";
        out << "\n" << rightBlk << ":\n";
        std::string rReg = genExpression(binary->right.get(), "bool", out);
        emitStore("i1", rReg, resAlloc, out);
        out << "    br label %" << endBlk << "\n";
        out << "\n" << endBlk << ":\n";
        return emitLoad("i1", resAlloc, out);
    }
    if (binary->optr == "**") {
        std::string lReg = genExpression(binary->left.get(),  binary->resolvedType, out);
        std::string rReg = genExpression(binary->right.get(), binary->resolvedType, out);
        std::string res  = newReg();
        if (isOpFloat) {
            if (binary->resolvedType == "float") {
                std::string ld = newReg(), rd = newReg(), pw = newReg();
                out << "    " << ld  << " = fpext float " << lReg << " to double\n";
                out << "    " << rd  << " = fpext float " << rReg << " to double\n";
                out << "    " << pw  << " = call double @llvm.pow.f64(double " << ld << ", double " << rd << ")\n";
                out << "    " << res << " = fptrunc double " << pw << " to float\n";
            } else {
                out << "    " << res << " = call double @llvm.pow.f64(double " << lReg << ", double " << rReg << ")\n";
            }
        } else {
            std::string ld = newReg(), rd = newReg(), pw = newReg();
            out << "    " << ld  << " = sitofp " << opLT << " " << lReg << " to double\n";
            out << "    " << rd  << " = sitofp " << opLT << " " << rReg << " to double\n";
            out << "    " << pw  << " = call double @llvm.pow.f64(double " << ld << ", double " << rd << ")\n";
            out << "    " << res << " = fptosi double " << pw << " to " << opLT << "\n";
        }
        return res;
    }
    std::string lReg = genExpression(binary->left.get(),  binary->resolvedType, out);
    std::string rReg = genExpression(binary->right.get(), binary->resolvedType, out);
    std::string resReg = emitBinaryOp(binary->optr, opLT, isOpFloat, lReg, rReg, out);
    if (resReg == "0") return "0";
    bool expectFloat = (expectedType == "float" || expectedType == "double");
    bool gotInt= (binary->resolvedType == "int"  || binary->resolvedType == "bigint");
    if (expectFloat && gotInt) {
        std::string promoted = newReg();
        out << "    " << promoted << " = sitofp " << opLT << " " << resReg << " to " << llvmType(expectedType) << "\n";
        return promoted;
    }
    return resReg;
}

std::string CodeGen::genTernaryExpr(ASTNode* node, std::ostream& out) {
    auto* tern= static_cast<TernaryExprNode*>(node);
    std::string llvmRT = llvmType(tern->resolvedType);
    std::string resAlloc = emitAlloca(llvmRT, out);
    std::string condReg = genExpression(tern->condition.get(), "bool", out);

    int id = ++regCounter;
    std::string trueBlk = "tern_true_"  + std::to_string(id);
    std::string falseBlk = "tern_false_" + std::to_string(id);
    std::string endBlk  = "tern_end_"   + std::to_string(id);

    out << "    br i1 " << condReg << ", label %" << trueBlk << ", label %" << falseBlk << "\n";

    out << "\n" << trueBlk << ":\n";
    std::string tReg = genExpression(tern->trueExpr.get(), tern->resolvedType, out);
    emitStore(llvmRT, tReg, resAlloc, out);
    out << "    br label %" << endBlk << "\n";

    out << "\n" << falseBlk << ":\n";
    std::string fReg = genExpression(tern->falseExpr.get(), tern->resolvedType, out);
    emitStore(llvmRT, fReg, resAlloc, out);
    out << "    br label %" << endBlk << "\n";

    out << "\n" << endBlk << ":\n";
    return emitLoad(llvmRT, resAlloc, out);
}

std::string CodeGen::genAssignmentExpr(ASTNode* node, std::ostream& out) {
    auto* assign        = static_cast<AssignmentExprNode*>(node);
    std::string targetLT, memPtr, targetBerryType;

    if (assign->target->type == NodeType::IDENT) {
        auto* ident = static_cast<IdentNode*>(assign->target.get());
        size_t dot = ident->name.find('.');

        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(ident->name);
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string chainType;
            std::string chainPtr = genFieldChainAddressing(headParts, out, chainType);
            ClassLayout& layout = classLayouts.at(chainType);
            int fieldIdx= layout.fieldIndex.at(parts.back());
            targetLT= llvmType(layout.fields[fieldIdx].first);
            targetBerryType = layout.fields[fieldIdx].first;

            std::string objReg = emitLoad(layout.llvmStructType + "*",chainPtr, out);
            memPtr = newReg();
            out << "    " << memPtr <<" = getelementptr inbounds "<< layout.llvmStructType << ", "<< layout.llvmStructType << "* " << objReg << ", i32 0, i32 " << fieldIdx << "\n";
        } else { 
            Symbol& sym= symTable.get(ident->name);
            targetLT= llvmType(sym.type);
            memPtr  = sym.llvmRegister;
            targetBerryType = sym.type;

            if (sym.type == "string" && assign->op == "=") {
                emitBREDecl("declare i8* @bery_string_copy(i8*)", "bery_string_copy");
                std::string srcReg = genExpression(assign->value.get(), "string", out);
                std::string copyReg = newReg();
                out << "    " << copyReg << " = call i8* @bery_string_copy(i8* " << srcReg << ")\n";
                emitStore("i8*", copyReg, memPtr, out);
                return copyReg;
            }
        }

    } else if (assign->target->type == NodeType::INDEX_EXPR) {
        auto* idxNode   = static_cast<IndexExprNode*>(assign->target.get());
        Symbol& sym     = symTable.get(idxNode->name);

        if (sym.type.size() > 6 && sym.type.substr(0, 6) == "array<") {
            std::string elemType = sym.type.substr(6, sym.type.size() - 7);
            std::string lt= llvmType(elemType);
            emitBREDecl("declare void @bery_array_set(i8*, i64, i8*)", "bery_array_set");
            std::string arrReg = emitLoad("i8*", sym.llvmRegister, out);
            std::string idxReg = genExpression(idxNode->indices[0].get(), "int", out);
            std::string idxExt = emitSext("i32", idxReg, "i64", out);
            std::string valReg = genExpression(assign->value.get(), elemType, out);
            std::string castReg= emitBoxValue(lt, valReg, out);
            out << "    call void @bery_array_set(i8* " << arrReg << ", i64 " << idxExt << ", i8* " << castReg << ")\n";
            return valReg;
        }

        std::string baseType = sym.type.substr(0, sym.type.find('['));
        targetLT = llvmType(baseType);
        targetBerryType      = baseType;
        std::string arrType  = sym.llvmAllocType;
        std::vector<std::string> idxRegs;
        for (auto& idx : idxNode->indices)
            idxRegs.push_back(genExpression(idx.get(), "int", out));
        memPtr = newReg();
        out << "    " << memPtr << " = getelementptr " << arrType << ", " << arrType << "* " << sym.llvmRegister << ", i32 0";
        for (auto& r : idxRegs) out << ", i32 " << r;
        out << "\n";
    }

    std::string valReg = genExpression(assign->value.get(), targetBerryType, out);

    if (assign->op == "=") {
        emitStore(targetLT, valReg, memPtr, out);
        return valReg;
    }
    
    bool isFloat= (targetBerryType == "float" || targetBerryType == "double");
    std::string curVal  = emitLoad(targetLT, memPtr, out);
    std::string resReg;

    if (assign->op == "**=") {
        resReg = newReg();
        if (isFloat) {
            if (targetBerryType == "float") {
                std::string ld = newReg(), rd = newReg(), pw = newReg();
                out << "    " << ld << " = fpext float " << curVal << " to double\n";
                out << "    " << rd << " = fpext float " << valReg << " to double\n";
                out << "    " << pw << " = call double @llvm.pow.f64(double " << ld << ", double " << rd << ")\n";
                out << "    " << resReg << " = fptrunc double " << pw << " to float\n";
            } else {
                out << "    " << resReg << " = call double @llvm.pow.f64(double " << curVal << ", double " << valReg << ")\n";
            }
        } else {
            std::string ld = newReg(), rd = newReg(), pw = newReg();
            out << "    " << ld << " = sitofp " << targetLT << " " << curVal << " to double\n";
            out << "    " << rd << " = sitofp " << targetLT << " " << valReg << " to double\n";
            out << "    " << pw << " = call double @llvm.pow.f64(double " << ld << ", double " << rd << ")\n";
            out << "    " << resReg << " = fptosi double " << pw << " to " << targetLT << "\n";
        }
    } else {
        std::string baseOp = assign->op.substr(0, assign->op.size() - 1);
        resReg = emitBinaryOp(baseOp, targetLT, isFloat, curVal, valReg, out);
    }

    emitStore(targetLT, resReg, memPtr, out);
    return resReg;
}

std::string CodeGen::genCastExpr(ASTNode* node, std::ostream& out) {
    auto* castNode      = static_cast<CastExprNode*>(node);
    std::string srcReg  = genExpression(castNode->expr.get(), castNode->srcType, out);
    std::string sType   = castNode->srcType;
    std::string tType   = castNode->targetType;
    if (sType == tType) return srcReg;

    std::string sLLVM   = llvmType(sType);
    std::string tLLVM   = llvmType(tType);
    std::string resReg  = newReg();
    bool srcFloat       = (sType == "float" || sType == "double");
    bool tgtFloat       = (tType == "float" || tType == "double");

    if (srcFloat && !tgtFloat)
        out << "    " << resReg << " = fptosi " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
    else if (!srcFloat && tgtFloat)
        out << "    " << resReg << " = sitofp " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
    else if (srcFloat && tgtFloat) {
        if (sType == "float")
            out << "    " << resReg << " = fpext float " << srcReg << " to double\n";
        else
            out << "    " << resReg << " = fptrunc double " << srcReg << " to float\n";
    } else {
        int sW = (sType == "bigint") ? 64 : (sType == "int" ? 32 : (sType == "char" ? 8 : 1));
        int tW = (tType == "bigint") ? 64 : (tType == "int" ? 32 : (tType == "char" ? 8 : 1));
        if (sW > tW)
            out << "    " << resReg << " = trunc " << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
        else if (sW < tW)
            out << "    " << resReg << " = " << (sType == "bool" ? "zext " : "sext ") << sLLVM << " " << srcReg << " to " << tLLVM << "\n";
        else
            return srcReg;
    }
    return resReg;
}

std::string CodeGen::genIndexExpr(ASTNode* node, std::ostream& out) {
    auto* idx           = static_cast<IndexExprNode*>(node);
    Symbol& sym         = symTable.get(idx->name);

    if (sym.type.size() > 6 && sym.type.substr(0, 6) == "array<") {
        std::string elemType = sym.type.substr(6, sym.type.size() - 7);
        std::string lt= llvmType(elemType);
        emitBREDecl("declare i8* @bery_array_get(i8*, i64)", "bery_array_get");
        std::string arrReg = emitLoad("i8*", sym.llvmRegister, out);
        std::string idxReg = genExpression(idx->indices[0].get(), "int", out);
        std::string idxExt = emitSext("i32", idxReg, "i64", out);
        std::string rawReg = newReg();
        out << "    " << rawReg<< " = call i8* @bery_array_get(i8* " <<arrReg <<", i64 " << idxExt << ")\n";
        std::string castReg  = newReg();
        out << "    " << castReg << " = bitcast i8* " << rawReg << " to " << lt << "*\n";
        return emitLoad(lt, castReg, out);
    }

    std::string baseType= sym.type.substr(0, sym.type.find('['));
    std::string lt= llvmType(baseType);
    std::string arrType = sym.llvmAllocType;
    std::vector<std::string> idxRegs;
    for (auto& i : idx->indices)idxRegs.push_back(genExpression(i.get(), "int", out));

    std::string ptrReg = newReg();
    out << "    " << ptrReg << " = getelementptr " << arrType << ", " << arrType << "* " << sym.llvmRegister << ", i32 0";
    for (auto& r : idxRegs) out << ", i32 " << r;
    out << "\n";
    return emitLoad(lt, ptrReg, out);
}

std::string CodeGen::genCallExpr(ASTNode* node, std::ostream& out) {
    auto* call          = static_cast<CallExprNode*>(node);

    static const std::unordered_set<std::string> breFns = {
        "print", "println", "inputInt", "inputBigInt", "inputFloat",
        "inputDouble", "inputBool", "inputChar", "inputString"
    };
    if (breFns.count(call->callee))
        return genBREPrintCall(node, out);

    size_t dot = call->callee.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(call->callee);
        std::string method =   parts.back();
        std::vector<std::string> headParts(parts.begin(),parts.end()-1);

        std::string  objType;
        std::string objPtr = genFieldChainAddressing(headParts, out, objType);

        if (objType =="string") {
            std::string strReg = emitLoad("i8*", objPtr, out);
            if (method == "len") {
                emitBREDecl("declare i64 @bery_string_length(i8*)", "bery_string_length");
                std::string lenReg = newReg();
                out << "    " <<lenReg << " = call i64 @bery_string_length(i8* " << strReg << ")\n";
                std::string truncReg = newReg();
                out << "    " << truncReg <<" = trunc i64 " << lenReg << " to i32\n";
                return truncReg;
            }
            if (method == "copy") {
                emitBREDecl("declare i8* @bery_string_copy(i8*)", "bery_string_copy");
                std::string copyReg = newReg();
                out << "    " << copyReg << " = call i8* @bery_string_copy(i8* " << strReg << ")\n";
                return copyReg;
            }
            if (method == "substr") {
                emitBREDecl("declare i8* @bery_string_substring(i8*, i64, i64)", "bery_string_substring");
                std::string s0 = genExpression(call->arguments[0].get(), "int", out);
                std::string s1 = genExpression(call->arguments[1].get(), "int", out);
                std::string e0 = emitSext("i32", s0, "i64", out);
                std::string e1 = emitSext("i32", s1, "i64", out);
                std::string res = newReg();
                out << "    " << res << " = call i8* @bery_string_substring(i8* " << strReg << ", i64 " << e0 << ", i64 " << e1 << ")\n";
                return res;
            }
        }

        if (objType.size() > 6 && objType.substr(0, 6) == "array<") {
            std::string elemType = objType.substr(6, objType.size() - 7);
            std::string lt= llvmType(elemType);
            std::string arrReg   = emitLoad("i8*", objPtr, out);

            if (method == "len") {
                emitBREDecl("declare i64 @bery_array_length(i8*)", "bery_array_length");
                std::string lenReg = newReg();
                out << "    " << lenReg << " = call i64 @bery_array_length(i8* " << arrReg << ")\n";
                std::string truncReg = newReg();
                out << "    " << truncReg << " = trunc i64 " << lenReg << " to i32\n";
                return truncReg;
            }
            if (method == "push") {
                emitBREDecl("declare void @bery_array_push(i8*, i8*)", "bery_array_push");
                std::string valReg = genExpression(call->arguments[0].get(), elemType, out);
                std::string castReg = emitBoxValue(lt, valReg, out);
                out << "    call void @bery_array_push(i8* " << arrReg << ", i8* " << castReg << ")\n";
                return "0";
            }
            if (method == "pop") {
                emitBREDecl("declare i8* @bery_array_pop(i8*)", "bery_array_pop");
                std::string rawReg= newReg();
                out << "    " << rawReg << " = call i8* @bery_array_pop(i8* " << arrReg << ")\n";
                std::string castReg = newReg();
                out << "    " << castReg << " = bitcast i8* " << rawReg << " to " << lt << "*\n";
                return emitLoad(lt,castReg, out);
            }
            if (method == "insert") {
                emitBREDecl("declare void @bery_array_insert(i8*, i64, i8*)", "bery_array_insert");
                std::string idxReg = genExpression(call->arguments[0].get(), "int", out);
                std::string idxExt = emitSext("i32", idxReg, "i64", out);
                std::string valReg = genExpression(call->arguments[1].get(), elemType, out);
                std::string castReg= emitBoxValue(lt, valReg, out);
                out << "    call void @bery_array_insert(i8* " << arrReg << ", i64 " << idxExt << ", i8* " << castReg << ")\n";
                return "0";
            }
            if (method == "remove") {
                emitBREDecl("declare void @bery_array_remove(i8*, i64)", "bery_array_remove");
                std::string idxReg = genExpression(call->arguments[0].get(), "int", out);
                std::string idxExt = emitSext("i32", idxReg, "i64", out);
                out << "    call void @bery_array_remove(i8* " << arrReg << ", i64 " << idxExt << ")\n";
                return "0";
            }
        }

        if (classLayouts.count(objType)) {
            std::string mangled = objType + "_" + method;
            if (functions.count(mangled)) {
                CodeGenFunctionSignature& sig = functions[mangled];
                std::string receiverReg = emitLoad(classLayouts.at(objType).llvmStructType + "*", objPtr, out);

                std::string argsStr = classLayouts.at(objType).llvmStructType + "* " + receiverReg;
                for (size_t i = 0; i < call->arguments.size(); ++i) {
                    std::string argReg = genExpression(call->arguments[i].get(), sig.paramTypes[i + 1], out);
                    argsStr += ", " + llvmType(sig.paramTypes[i + 1]) + " " + argReg;
                }

                if (sig.returnType.empty() || sig.returnType == "void") {
                    out << "    call void @" << mangled << "(" << argsStr << ")\n";
                    return "0";
                }
                std::string resReg = newReg();
                out << "    " << resReg << " = call " << llvmType(sig.returnType) << " @" << mangled << "(" << argsStr << ")\n";
                return resReg;
            }
        }
        return "0";
    }
    if (functions.find(call->callee) == functions.end()) return "0";
    CodeGenFunctionSignature& sig = functions[call->callee];

    std::string argsStr;
    for (size_t i = 0; i < call->arguments.size(); ++i) {
        std::string argReg = genExpression(call->arguments[i].get(), sig.paramTypes[i], out);
        argsStr += llvmType(sig.paramTypes[i]) + " " + argReg;
        if (i + 1 < call->arguments.size()) argsStr += ", ";
    }

    if (sig.returnType == "void") {
        out << "    call void @" << call->callee << "(" << argsStr << ")\n";
        return "0";
    }
    std::string resReg = newReg();
    out << "    " << resReg << " = call " << llvmType(sig.returnType) << " @" << call->callee << "(" << argsStr << ")\n";
    return resReg;
}

std::string CodeGen::genNewExpr(ASTNode* node, std::ostream& out) {
    auto* newExpr = static_cast<NewExprNode*>(node);
    auto it = classLayouts.find(newExpr->className);
    if (it == classLayouts.end()) return "null";
    ClassLayout& layout = it->second;

    emitBREDecl("declare i8* @bery_alloc(i64, i32)", "bery_alloc");
    std::string typeIdReg = emitLoad("i32", "@" + newExpr->className + "_typeid", out);
    std::string rawReg = newReg();
    out << "    " << rawReg << " = call i8* @bery_alloc(i64 " << layout.instanceSize<< ", i32 " << typeIdReg << ")\n";
    std::string objReg = newReg();
    out << "    " << objReg << " = bitcast i8* " << rawReg << " to " << layout.llvmStructType << "*\n";

    for (size_t i = 0; i < layout.fields.size(); ++i) {
        std::string flt = llvmType(layout.fields[i].first);
        std::string gepReg = newReg();
        out << "    " << gepReg << " = getelementptr inbounds " << layout.llvmStructType << ", "<< layout.llvmStructType << "* " << objReg << ", i32 0, i32 " << i << "\n";
        bool isPtr = !flt.empty() && flt.back() == '*';
        std::string zeroVal = (flt == "float" || flt == "double") ? "0.0" : (isPtr ? "null" : "0");
        out << "    store " << flt << " " << zeroVal << ", " << flt << "* " << gepReg << "\n";
    }
    return objReg;
}

std::string CodeGen::genFieldChainAddressing(const std::vector<std::string>& parts, std::ostream& out, std::string& outType) {
    Symbol& base = symTable.get(parts[0]);
    std::string curPtr = base.llvmRegister;
    std::string curType = base.type;
    for (size_t i = 1; i < parts.size(); ++i) {
        ClassLayout& layout = classLayouts.at(curType);
        std::string objReg= emitLoad(layout.llvmStructType + "*", curPtr, out);
        int fieldIdx= layout.fieldIndex.at(parts[i]);
        std::string gepReg  = newReg();
        out << "    " << gepReg << " = getelementptr inbounds " << layout.llvmStructType << ", "<< layout.llvmStructType << "* " << objReg << ", i32 0, i32 " << fieldIdx << "\n";
        curPtr  = gepReg;
        curType = layout.fields[fieldIdx].first;
    }
    outType = curType;
    return curPtr;
}

std::string CodeGen::genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    if (!node) return "0";
    switch (node->type) {
        case NodeType::NULL_LIT:        return "null";
        case NodeType::INT_LIT:
        case NodeType::DECIMAL_LIT:
        case NodeType::BOOL_LIT:
        case NodeType::CHAR_LIT:
        case NodeType::STRING_LIT:      return genLiteral(node, expectedType, out);
        case NodeType::IDENT:           return genIdentExpr(node, expectedType, out);
        case NodeType::GROUPED_EXPR:    return genExpression(static_cast<GroupedExprNode*>(node)->expression.get(), expectedType, out);
        case NodeType::UNARY_EXPR:      return genUnaryExpr(node, expectedType, out);
        case NodeType::BETWEEN_EXPR:    return genBetweenExpr(node, out);
        case NodeType::BINARY_EXPR:     return genBinaryExpr(node, expectedType, out);
        case NodeType::TERNARY_EXPR:    return genTernaryExpr(node, out);
        case NodeType::ASSIGNMENT_EXPR: return genAssignmentExpr(node, out);
        case NodeType::CAST_EXPR:       return genCastExpr(node, out);
        case NodeType::INDEX_EXPR:      return genIndexExpr(node, out);
        case NodeType::CALL_EXPR:       return genCallExpr(node, out);
        case NodeType::NEW_EXPR:        return genNewExpr(node, out);
        default:                        return "0";
    }
}