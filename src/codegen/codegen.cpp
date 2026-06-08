#include "codegen.h"
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include <cstring>
#include <unordered_map>
#include <sstream>
#include <cstdint>
#include <iomanip>
#include "../parser/ast/expressions.h"

CodeGen::CodeGen(ASTNode* root)
   : root(root), regCounter(0) {}

void CodeGen::generate(const std::string& outputPath) {
   auto* program = static_cast<ProgramNode*>(root);
   std::ostringstream body;
   body << "define i32 @main() {\n";
   body << "entry:\n";

   if (program->runBlock) {
       for (auto& node : program->runBlock->statements) {
           if (node->type == NodeType::VAR_DECL)
               genVarDecl(node.get(), body);
       }
   }
   body << "    ret i32 0\n";
   body << "}\n";
   std::ofstream out(outputPath);

   for (auto& node : program->globals) {
       if (node->type != NodeType::VAR_DECL) continue;
       auto* decl = static_cast<VarDeclNode*>(node.get());
       std::string lt = llvmType(decl->varType);
       std::string initVal = "0";
       if (decl->value) {
           if (decl->value->type == NodeType::INT_LIT) {
            initVal = std::to_string(static_cast<IntLitNode*>(decl->value.get())->value);
           } 
           else if (decl->value->type == NodeType::DECIMAL_LIT ){
            double val = static_cast<DecimalLitNode*>(decl->value.get())->value;
            std::ostringstream ss;
            ss << std::showpoint << val;   
            initVal = ss.str();
           }
           else if (decl->value->type == NodeType::BOOL_LIT) {
            initVal = static_cast<BoolLitNode*>(decl->value.get())->value? "1" :"0";
           }
           else if (decl->value->type == NodeType::CHAR_LIT) {
            initVal = std::to_string(static_cast<CharLitNode*>(decl->value.get())->value);
           } 

       }
       out << "@" << decl->name << " = global " << lt << " " << initVal << "\n";
   }
   out << "\n" << body.str();
}

void CodeGen::genVarDecl(ASTNode* node, std::ostream& out) {
   auto* decl = static_cast<VarDeclNode*>(node);
   std::string lt = llvmType(decl->varType);
   out << "    %" << decl->name << " = alloca " << lt << "\n";
   if (!decl->value) return;
   std::string valReg = genExpression(decl->value.get(), decl->varType, out);
   out << "    store " << lt << " " << valReg << ", " << lt << "* %" << decl->name << "\n";
}

std::string CodeGen::genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out) {
   if(!node){return "0";}
   std::string lt = llvmType(expectedType);
   bool isFloat = (expectedType=="float" || expectedType=="double");
   if (node->type == NodeType::INT_LIT) {
        std::string reg = newReg();
       auto* lit = static_cast<IntLitNode*>(node);
       out << "    " << reg << " = add " << lt << " 0, " << lit->value << "\n";
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
   
   return "0";
}

std::string CodeGen::llvmType(const std::string& t) {
   if (t == "int") return "i32";
   if (t == "bigint") return "i64";
   if (t == "bool") return "i1";
   if (t == "float") return "float";
   if (t == "double") return "double";
   if (t == "char") return "i8";
   return "i32";
}
std::string CodeGen::newReg() {
   return "%" + std::to_string(++regCounter);
}