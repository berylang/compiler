#pragma once 
#include "node.h"
#include <string>
#include <vector>
#include <memory>


struct ArrayDeclNode: public ASTNode{
    std:: string elementType;
    std:: string name;
    std::vector<int> dimensions;  
    std::vector<std::unique_ptr<ASTNode>> initializers;

    ArrayDeclNode(std::string elementType,std::string name,std::vector<int> dim,
    std::vector<std::unique_ptr<ASTNode>> initializers, int ln): 
    elementType(elementType),name(name), dimensions(dim),
    initializers(std::move(initializers)){
        type=NodeType::ARRAY_DECL;
        line = ln;
    }

};
