#include "../codegen.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/literals.h"
#include "../../sema/symboltable.h"
#include <iomanip>
#include <sstream>


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
        std::string realType = symTable.get(ident->name).type; 
        if (realType.back() == ']') {
            realType = realType.substr(0, realType.find('['));
        }
        
        std::string realLT = llvmType(realType);
        std::string reg = newReg();
        
        out << "    " << reg << " = load " << realLT << ", " << realLT << "* %"<< ident->name << "\n";
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
        out << "    store " << lt << " " << newvalreg << ", " << lt << "* %" << ident->name << "\n";
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

   return "0";
}
