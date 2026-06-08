#pragma once

enum class NodeType {
    PROGRAM, RUN_BLOCK, VAR_DECL, INT_LIT, IDENT, BOOL_LIT, DECIMAL_LIT, CHAR_LIT, UNARY_EXPR, GROUPED_EXPR, ARRAY_DECL
};
struct ASTNode {
    NodeType type;
    virtual ~ASTNode() = default;
};

