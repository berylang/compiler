#pragma once
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
