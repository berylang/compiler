#pragma once
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