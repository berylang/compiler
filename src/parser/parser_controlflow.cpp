#include "parser.h"
/*

    Parsing special controlflow statements
    which are - 
        1. break statement (switch, loops)
        2. continue statement (loops)
        3. pass statement (any block)
*/

#include "ast/controlflow.h"


std::unique_ptr<ASTNode> Parser::parseBreakStmt() {
    int line = previous().line;
    advance();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after 'break'");
    return std::make_unique<BreakStmtNode>(line);
}

std::unique_ptr<ASTNode> Parser::parseContinueStmt() {
    int line = previous().line;
    advance();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after 'continue'");
    return std::make_unique<ContinueStmtNode>(line);
}

std::unique_ptr<ASTNode> Parser::parsePassStmt() {
    int line = previous().line;
    advance();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after 'pass'");
    return std::make_unique<PassStmtNode>(line);
}


