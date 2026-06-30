#pragma once

/*
   
   Bery IR Code Generator,

   it tranverse the AST and emits LLVM IR text directly into an output stream.
   it operates in three passes:
      @global pass - emtting global variables, arrays, functions signatures,extern declarations,
      @functions pass - emits each func body as an LLVM functions.
      @run{} pass - emits @main() which calls 'bery_runtime_startup()', runs the run block statement, and then calss 'bery_runtime_shutdown()'

*/
#include <fstream>
#include <sstream>
#include <string>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "../parser/ast/node.h"
#include "../sema/symboltable.h"
#include "../parser/ast/classes.h"


// @function signature data
struct CodeGenFunctionSignature {
   std::string returnType;
   std::vector<std::string> paramTypes;
};

class CodeGen {
public:
   CodeGen(ASTNode* root, SymbolTable& symTable);
   // @main Codegen function, which traverse the AST after sema
   void generate(const std::string& outputPath);

private:
   // @data - AST, SymbolTable
   ASTNode* root;
   SymbolTable& symTable;
   

   // @register controllers, SSA rules
   int regCounter;
   std::string newReg();

   // @data, and literals
   std::string llvmType(const std::string& berryType);
   std::string extractConstant(ASTNode* node);

   // @strings handling in LLVM IR
   int strCounter = 0;
   std::ostringstream globalStrings;
   std::string escapeLLVMString(const std::string& str);
   
   // @FFI 
   std::unordered_set<std::string> declaredExterns;

   // @controlflow tracking
   std::vector<std::string> breakTracker;
   std::vector<std::string> continueTracker;

   // @functions
   std::unordered_map<std::string, CodeGenFunctionSignature> functions;
   std::string currentFuncReturn;
   void genFuncDef(ASTNode* node, std::ostream& out);
   void genReturnStmt(ASTNode* node, std::ostream& out);
   
   // @garbage collector
   std::stack<int> gcRootScopeStack;
   int gcRootCounter = 0;
   void emitGCPush(const std::string& allocaReg, const std::string& llvmType, std::ostream& out);
   void emitGCPops(int count, std::ostream& out);
   void pushGCScope();
   int popGCScope();
   
   // @BRE
   std::ostringstream breDecls;
   void emitBREDecl(const std::string& decl, const std::string& key);
   std::string genBREPrintCall(ASTNode* node, std::ostream& out);
   
   // @controlflow
   void genBlock(ASTNode* node, std::ostream& out);
   void genStatement(ASTNode* stmt, std::ostream& out);
   void genBreakStmt(ASTNode* node, std::ostream& out);
   void genContinueStmt(ASTNode* node, std::ostream& out);
   
   // @declarations
   void genVarDecl(ASTNode* node, std::ostream& out);
   void genArrayDecl(ASTNode* node, std::ostream& out);
   
   // @conditionals
   void genIfStmt(ASTNode* node, std::ostream& out);
   void genSwitchStmt(ASTNode* node, std::ostream& out);

   // @loops
   void genDoWhileStmt(ASTNode* node, std::ostream& out);
   void genWhileStmt(ASTNode* node, std::ostream& out);
   void genForStmt(ASTNode* node, std::ostream& out);
   void genForInStmt(ASTNode* node, std::ostream& out);
   
   // @todo - remove this after adding 'resoledType' field inside each symbol stored in the Table.
   std::string inferType(ASTNode* node);
   
   // @allocation helpers
   int alignOf(const std::string& llvmT);
   std::string emitAlloca(const std::string& llvmT, std::ostream& out);
   void emitStore(const std::string& llvmT, const std::string& val, const std::string& ptr, std::ostream& out);
   std::string emitLoad(const std::string& llvmT, const std::string& ptr, std::ostream& out);
   std::string emitSext(const std::string& fromT, const std::string& val, const std::string& toT, std::ostream& out);
   std::string emitBoxValue(const std::string& llvmT, const std::string& valReg, std::ostream& out);
   
   // @expression helpers
   std::string genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out);
   std::string genLiteral(ASTNode* node, const std::string& expectedType, std::ostream& out);
   std::string genIdentExpr(ASTNode* node, const std::string& expectedType, std::ostream& out);
   std::string genUnaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& out);
   std::string genBetweenExpr(ASTNode* node, std::ostream& out);
   std::string genBinaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& out);
   std::string genTernaryExpr(ASTNode* node, std::ostream& out);
   std::string genAssignmentExpr(ASTNode* node, std::ostream& out);
   std::string genCastExpr(ASTNode* node, std::ostream& out);
   std::string genIndexExpr(ASTNode* node, std::ostream& out);
   std::string genCallExpr(ASTNode* node, std::ostream& out);

   std::string emitBinaryOp(const std::string& op, const std::string& llvmT, bool isFloat, const std::string& lReg, const std::string& rReg, std::ostream& out);

   // @oop
   struct ClassLayout {
      std::string name;
      std::vector<std::pair<std::string, std::string>> fields;
      std::string llvmStructType; 
      std::unordered_map<std::string, int> fieldIndex;
   };
   std::unordered_map<std::string, ClassLayout> classLayouts;
   void genClassDecl(ASTNode* node);
   
};
