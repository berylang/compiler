#pragma once

enum class NodeType {
    PROGRAM, RUN_BLOCK, VAR_DECL, INT_LIT, IDENT, BOOL_LIT, DECIMAL_LIT, CHAR_LIT
};

struct ASTNode {
    NodeType type;
    virtual ~ASTNode() = default;
};

