#pragma once

/*

   Bery Symbol Table,

   It is a scoped key-value store, which stores every declared variable, it's datatype, whether it is constant and 
   whether it's initialized or not.

   Internally it's a stack of hashmaps. 
      Each '{' pushes a new scope
      lookup tranverse stack from top to bottom.
      and each '}' pops it.
   
   @todo : add symboltype for storing variables, classes, and functions in same table.
   @todo : make it global symbol table.
   @todo : add resolvedType for IR generator

*/
#include <string>
#include <unordered_map>
#include <vector>


struct Symbol {
   std::string type;
   bool isConst;
   bool isInitialized;
   int line;
   std::string llvmRegister;
   std::string llvmAllocType;
   int arraySize;
};


class SymbolTable {
public:
   SymbolTable();
   void pushScope();
   void popScope();

   void add(const std::string& name, Symbol sym);
   bool exists(const std::string& name);
   bool existsInCurrentScope(const std::string& name);
   Symbol& get(const std::string& name);


private:
   std::vector<std::unordered_map<std::string, Symbol>> scopes;
};
