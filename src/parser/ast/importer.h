#pragma once
#include "node.h"
#include <string>

struct ImportNode : public ASTNode {
    std::string moduleName;
    std::string path;

    ImportNode(std::string mod, std::string p, int ln)
        : moduleName(mod), path(p) {
        type = NodeType::IMPORT_STMT;
        line = ln;
    }
};