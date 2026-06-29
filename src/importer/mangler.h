#pragma once

/*

    Bery Name Mangler,

    It mangles the name for the IR generation, 
    as at LLVM level dot access such as 'math.add()' is invalid,

    so mangler converts it into '"math.add()"' as complete name.
*/

#include <string>
#include <unordered_set>
#include "../parser/ast/node.h"

class ASTNameMangler {
private:
    std::string prefix;
    std::unordered_set<std::string>& globals;

public:
    ASTNameMangler(std::string p, std::unordered_set<std::string>& g);
    void mangle(ASTNode* node);
};