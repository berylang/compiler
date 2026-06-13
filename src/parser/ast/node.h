#pragma once

enum class NodeType {
    PROGRAM, RUN_BLOCK, VAR_DECL, INT_LIT, IDENT, BOOL_LIT, DECIMAL_LIT, CHAR_LIT,
    STRING_LIT,NULL_LIT,
    UNARY_EXPR, GROUPED_EXPR, ARRAY_DECL, BINARY_EXPR, BETWEEN_EXPR, TERNARY_EXPR,
    BLOCK, IF_STMT,
    ASSIGNMENT_EXPR
};
struct ASTNode {
    NodeType type;
    int line;
    virtual ~ASTNode() = default;
};

