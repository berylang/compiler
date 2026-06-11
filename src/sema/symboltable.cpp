#include "symboltable.h"
#include <stdexcept>


void SymbolTable::add(const std::string& name, Symbol sym) {
   table[name] = sym;
}


bool SymbolTable::exists(const std::string& name) {
   return table.find(name) != table.end();
}


Symbol SymbolTable::get(const std::string& name) {
   if (!exists(name))
       throw std::runtime_error("Undefined: " + name);
   return table[name];
}
