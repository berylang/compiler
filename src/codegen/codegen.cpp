#include "codegen.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/arraydeclare.h"
#include <cstring>
#include <unordered_map>
#include <sstream>
#include <cstdint>
#include <iomanip>
#include "../parser/ast/expressions.h"

CodeGen::CodeGen(ASTNode* root)
   : root(root), regCounter(0), strCounter(0) {}


void CodeGen::generate(const std::string& outputPath) {
   auto* program = static_cast<ProgramNode*>(root);
   std::ostringstream body;
   body << "define i32 @main() {\n";
   body << "entry:\n";

      if (program->runBlock) {
       for (auto& node : program->runBlock->statements) {
           if (node->type == NodeType::VAR_DECL)
               genVarDecl(node.get(), body);
           else if (node->type == NodeType::ARRAY_DECL)
               genArrayDecl(node.get(), body);
       }
   }
   body << "    ret i32 0\n";
   body << "}\n";
   std::ofstream out(outputPath);
   out << "declare double @llvm.pow.f64(double, double)\n";

   for (auto& node : program->globals) {
       if (node->type == NodeType::VAR_DECL) {
           auto* decl = static_cast<VarDeclNode*>(node.get());
           std::string lt = llvmType(decl->varType);
           std::string initVal = "0";
           if (decl->value) {
               if (decl->value->type == NodeType::INT_LIT) {
                initVal = std::to_string(static_cast<IntLitNode*>(decl->value.get())->value);
               } 
               else if (decl->value->type == NodeType::DECIMAL_LIT ){
                   initVal = std::to_string(static_cast<DecimalLitNode*>(decl->value.get())->value);
               }
               else if (decl->value->type == NodeType::BOOL_LIT) {
                initVal = static_cast<BoolLitNode*>(decl->value.get())->value? "1" :"0";
               }else if (decl->value->type == NodeType::CHAR_LIT) {
            initVal = std::to_string(static_cast<CharLitNode*>(decl->value.get())->value);
           } 
           }
           out << "@" << decl->name << " = global " << lt << " " << initVal << "\n";
       } 
       else if (node->type == NodeType::ARRAY_DECL) {
           auto* decl = static_cast<ArrayDeclNode*>(node.get());
           std::string lt = llvmType(decl->elementType);
           int resolvedSize = decl->size >= 0 ? decl->size : (int)decl->initializers.size();
           std::string arrType = "[" + std::to_string(resolvedSize) + " x " + lt + "]";
           
           std::string initVal;
           if (decl->initializers.empty()) {
               initVal = "zeroinitializer";
           } else {
               initVal = "[";
               for (size_t i = 0; i < decl->initializers.size(); ++i) {
                   std::string elemVal = "0";
                   auto* initNode = decl->initializers[i].get();
                   if (initNode->type == NodeType::INT_LIT) {
                       elemVal = std::to_string(static_cast<IntLitNode*>(initNode)->value);
                   } else if (initNode->type == NodeType::DECIMAL_LIT) {
                       elemVal = std::to_string(static_cast<DecimalLitNode*>(initNode)->value);
                   } else if (initNode->type == NodeType::BOOL_LIT) {
                       elemVal = static_cast<BoolLitNode*>(initNode)->value ? "1" : "0";
                   }
                   initVal += lt + " " + elemVal;
                   if (i + 1 < decl->initializers.size()) {
                       initVal += ", ";
                   }
               }
               if (decl->initializers.size() < (size_t)resolvedSize) {
                   for (size_t i = decl->initializers.size(); i < (size_t)resolvedSize; ++i) {
                       initVal += ", " + lt + " 0";
                   }
               }
               initVal += "]";
           }
           out << "@" << decl->name << " = global " << arrType << " " << initVal << "\n";
       }
   }

   out << globalStrings.str() << "\n";
   out << body.str();
}

void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<VarDeclNode*>(node);
   std::string lt = llvmType(decl->varType);
   out << "    %" << decl->name << " = alloca " << lt << "\n";
   if (!decl->value) return;
   std::string valReg = genExpression(decl->value.get(), decl->varType, out);
   out << "    store " << lt << " " << valReg << ", " << lt << "* %" << decl->name << "\n";
}

void CodeGen::genArrayDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<ArrayDeclNode*>(node);
   std::string lt = llvmType(decl->elementType);
   int resolvedSize = decl->size >= 0 ? decl->size : (int)decl->initializers.size();
   std::string arrType = "[" + std::to_string(resolvedSize) + " x " + lt + "]";
   
   out << "    %" << decl->name << " = alloca " << arrType << "\n";
   if (decl->initializers.empty()) return;

   for (size_t i = 0; i < decl->initializers.size(); ++i) {
       std::string valReg = genExpression(decl->initializers[i].get(), decl->elementType, out);
       std::string ptrReg = newReg();
       out << "    " << ptrReg << " = getelementptr " << arrType << ", " << arrType << "* %" << decl->name << ", i32 0, i32 " << i << "\n";
       out << "    store " << lt << " " << valReg << ", " << lt << "* " << ptrReg << "\n";
   }
}

std::string CodeGen::genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out) {
   if(!node){return "0";}
   if (node->type == NodeType::NULL_LIT) {return "null";}
   std::string lt = llvmType(expectedType);
   bool isFloat = (expectedType=="float" || expectedType=="double");
   if (node->type == NodeType::INT_LIT) {
        std::string reg = newReg();
       auto* lit = static_cast<IntLitNode*>(node);

       if(isFloat){
            out << "    " << reg << " = fadd " << lt << " 0.0, " << lit->value << ".0\n";
       }else{
            out << "    " << reg << " = add " << lt << " 0, " << lit->value << "\n";
       }
       
       return reg;
   }

   if (node->type == NodeType::DECIMAL_LIT) {
        std::string reg = newReg();
       auto* lit = static_cast<DecimalLitNode*>(node);
       std::ostringstream ss;
       ss << std::showpoint << lit->value;
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

   if(node->type == NodeType::NULL_LIT){
    return "null";
   }
   if(node->type == NodeType::IDENT){
    std::string reg = newReg();
    auto* ident = static_cast<IdentNode*>(node);
    out << "    " << reg << " = load " << lt << ", " << lt << "* %"<< ident->name << "\n";
    return reg;
   }
   if(node->type == NodeType::GROUPED_EXPR){
    auto* group = static_cast<GroupedExprNode*>(node);
    return genExpression(group->expression.get(), expectedType, out);
   }
   if(node->type== NodeType::UNARY_EXPR){
    auto* unary = static_cast<UnaryExprNode*>(node);
    if(unary->optr == "-" || unary->optr == "!" || unary->optr == "~"){
        std::string reg = newReg();
        std::string opreg = genExpression(unary->operand.get(), expectedType, out);
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
    if(unary->optr == "--" || unary->optr == "++" || unary->optr == "post++" || unary->optr == "post--"){
        auto* ident = static_cast<IdentNode*>(unary->operand.get());
        std::string oldvalreg = genExpression(unary->operand.get(), expectedType, out);
        std::string newvalreg = newReg();
        std::string one = isFloat?"1.0":"1";
        std::string opins;
        if(unary->optr == "++" || unary->optr == "post++"){
            opins = isFloat?"fadd ":"add ";
        }
        else if(unary->optr == "--" || unary->optr == "post--"){
            opins = isFloat?"fsub ":"sub ";
        }
        out << "    " << newvalreg << " = " << opins << lt << " " << oldvalreg << ", " << one << "\n";
        out << "    store " << lt << " " << newvalreg << ", " << lt << "* %" << ident->name << "\n";
        if(unary->optr == "post++" || unary->optr == "post--"){
            return oldvalreg;
        }
        else{
            return newvalreg;
        }
    }
   }

    if(node->type == NodeType::BINARY_EXPR){
        auto* binary = static_cast<BinaryExprNode*>(node);

        std::string leftReg = genExpression(binary->left.get(), expectedType, out);
        std::string rightReg = genExpression(binary->right.get(), expectedType, out);
        std::string resultReg = newReg();

        if(isFloat){
            if(binary->optr == "+"){
                out << "    " << resultReg << " = fadd " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "-"){
                out << "    " << resultReg << " = fsub " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "*"){
                out << "    " << resultReg << " = fmul " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "/"){
                out << "    " << resultReg << " = fdiv " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "**"){
                if(expectedType == "float"){
                    out << "    " << resultReg << " = call float @llvm.pow.f32(float " << leftReg << ", float " << rightReg << ")\n";
                }else{
                    out << "    " << resultReg << " = call double @llvm.pow.f64(double " << leftReg << ", double " << rightReg << ")\n";
                }
            }else{
                return "0";
            }
        }else{
            if(binary->optr == "+"){
                out << "    " << resultReg << " = add " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "-"){
                out << "    " << resultReg << " = sub " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "*"){
                out << "    " << resultReg << " = mul " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "/"){
                out << "    " << resultReg << " = sdiv " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "%"){
                out << "    " << resultReg << " = srem " << lt << " " << leftReg << ", " << rightReg << "\n";
            }else if(binary->optr == "**"){
                std::string leftDouble = newReg();
                std::string rightDouble = newReg();

                out << "    " << leftDouble << " = sitofp " << lt << " " << leftReg << " to double\n";
                out << "    " << rightDouble << " = sitofp " << lt << " " << rightReg << " to double\n";

                std::string powReg = newReg();

                out << "    " << powReg << " = call double @llvm.pow.f64(double " << leftDouble << ", double " << rightDouble << ")\n";
                out << "    " << resultReg << " = fptosi double " << powReg << " to " << lt << "\n";
            }else{
                return "0";
            }
        }

        return resultReg;
    }

   return "0";
}

std::string CodeGen::llvmType(const std::string& t) {
   if (t == "int") return "i32";
   if (t == "bigint") return "i64";
   if (t == "bool") return "i1";
   if (t == "float") return "float";
   if (t == "double") return "double";
   if (t == "char") return "i8";
   if (t == "string") return "i8*";
   return "i32";
}

std::string CodeGen::escapeLLVMString(const std::string& str) {
    std::ostringstream escaped;

    for (char c : str) {
        if (c == '\n') escaped << "\\0A";
        else if (c == '\t') escaped << "\\09";
        else if (c == '\r') escaped << "\\0D";
        else if (c == '\\') escaped << "\\5C";
        else if (c == '"') escaped << "\\22";
        else if (c < 32 || c > 126) {
            escaped << "\\" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)(unsigned char) c;
        } else {
            escaped << c;
        }
    }
    
    escaped << "\\00";
    return escaped.str();
}
std::string CodeGen::newReg() {
   return "%" + std::to_string(++regCounter);
}