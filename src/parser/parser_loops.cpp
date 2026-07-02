
#include "parser.h"
#include "ast/controlflow.h"

/*

    Bery Loops,
    bery supports - 
        1. while loop
        2. do while loop
        3. for loop
        4. for in loop over range
        5. for in loop over iterable (currently only arrays)
    
*/

std::unique_ptr<ASTNode> Parser::parseWhileStmt(){
    advance();
    int line = previous().line;
    consume(TokenType::TOKEN_LPARAN,"Expected '(' after 'while'");
    auto condition = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before while-body");
    auto body = parseBlock();
    return std::make_unique<WhileStmtNode>(std::move(condition), std::move(body), line);
}

std::unique_ptr<ASTNode> Parser::parseDoWhileStmt(){
    advance();
    int line = previous().line;
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before do-while-body");
    auto body = parseBlock();
    consume(TokenType::TOKEN_WHILE, "Expected 'while' condition after do-while-body");
    consume(TokenType::TOKEN_LPARAN,"Expected '(' after 'while'");
    auto condition = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after condition-end");
    return std::make_unique<DoWhileStmtNode>(std::move(condition), std::move(body), line);
}

std::unique_ptr<ASTNode> Parser::parseForStmt() {
    advance();
    int line = previous().line;
    consume(TokenType::TOKEN_LPARAN, "Expected '(' after 'for'");

    bool isForIn = false;
    bool hasExplicitType = false;

    if (peek().type == TokenType::TOKEN_IDENT && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::TOKEN_IN) {
        isForIn = true;
    } 
    else if (isTypeToken(peek().type) && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::TOKEN_IDENT && current + 2 < tokens.size() && tokens[current + 2].type == TokenType::TOKEN_IN) {
        isForIn = true;
        hasExplicitType = true;
    }

    if (isForIn) {
        std::string varType = "unknown";
        if (hasExplicitType) {
            varType = advance().lexeme;
        }
        
        Token varTok = consume(TokenType::TOKEN_IDENT, "Expected identifier");
        advance();

        auto iterableOrStart = parseExpression();
        std::unique_ptr<ASTNode> rangeEnd = nullptr;
        std::unique_ptr<ASTNode> step = nullptr;

        if (check(TokenType::TOKEN_RANGE)) {
            advance();
            rangeEnd = parseExpression();
        }

        if (check(TokenType::TOKEN_COMMA)) {
            advance();
            step = parseExpression();
        }

        consume(TokenType::TOKEN_RPARAN, "Expected ')' after for-in declaration");
        consume(TokenType::TOKEN_LBRACE, "Expected '{' before loop body");
        auto body = parseBlock();

        return std::make_unique<ForInNode>(varType, varTok.lexeme, std::move(iterableOrStart), std::move(rangeEnd), std::move(step), std::move(body), line);
    } 
    else {
        std::unique_ptr<ASTNode> init = nullptr;
        
        if (!check(TokenType::TOKEN_SEMICOLON)) {
            if (isTypeToken(peek().type)) {
                auto decls = parseVarDecl(AccessSpecifier::PUBLIC,false);
                
                if (decls.size() == 1) {
                    init = std::move(decls[0]);
                } else {
                    auto initBlock = std::make_unique<BlockNode>(line);
                    for (auto& d : decls) initBlock->statements.push_back(std::move(d));
                    init = std::move(initBlock);
                }
            } else {
                init = parseExpression(); 
                consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after loop initialization");
            }
        } else {
            consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after loop initialization");
        }

        std::unique_ptr<ASTNode> cond = nullptr;
        if (!check(TokenType::TOKEN_SEMICOLON)) cond = parseExpression();
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after loop condition");

        std::unique_ptr<ASTNode> update = nullptr;
        if (!check(TokenType::TOKEN_RPARAN)) update = parseExpression();
        consume(TokenType::TOKEN_RPARAN, "Expected ')' after loop update");

        consume(TokenType::TOKEN_LBRACE, "Expected '{' before loop body");
        auto body = parseBlock();

        return std::make_unique<ForStmtNode>(std::move(init), std::move(cond), std::move(update), std::move(body), line);
    }
}
