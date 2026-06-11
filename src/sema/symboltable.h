#pragma once
#include <string>
#include <unordered_map>


struct Symbol {
   std::string type;
   bool isConst;
   bool isInitialized;
   int line;
};


class SymbolTable {
public:
   void add(const std::string& name, Symbol sym);
   bool exists(const std::string& name);
   Symbol get(const std::string& name);


private:
   std::unordered_map<std::string, Symbol> table;
};
