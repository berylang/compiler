#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include "../parser/ast/node.h"
#include "../parser/ast/programnode.h"

class Importer {
public:
    void resolveImports(ProgramNode* mainProgram, const std::string& basePath);

private:
    std::unordered_set<std::string> importedFiles;
    void loadModule(const std::string& modName, const std::string& fullPath, const std::string& basePath, std::vector<std::unique_ptr<ASTNode>>& outGlobals);
};