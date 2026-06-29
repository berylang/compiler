#include "parser.h"
#include "ast/functions.h"

/*

    Bery Functions,
    which are uniquely defined by new sytanx :

        func <func_name>([param_list]) -> return_type {
            // body
        }
    
    calling functions - 
        
        <func_name>([arg_list]);
    
    
*/

std::unique_ptr<ASTNode> Parser::parseFunctionDef() {
    advance(); 
    int line = previous().line;
    Token nameToken = consume(TokenType::TOKEN_IDENT, "Expected function name");
    consume(TokenType::TOKEN_LPARAN, "Expected '(' after function name");

    std::vector<std::pair<std::string, std::string>> params;
    if (!check(TokenType::TOKEN_RPARAN)) {
        do {
            Token typeTok = advance();
            Token nameTok = consume(TokenType::TOKEN_IDENT, "Expected parameter name");
            params.push_back({typeTok.lexeme, nameTok.lexeme});
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after parameters");

    std::string returnType = "void"; 
    if (check(TokenType::TOKEN_ARROW)) {
        advance();
        returnType = advance().lexeme;
    }

    consume(TokenType::TOKEN_LBRACE, "Expected '{' before function body");
    auto body = parseBlock();
    return std::make_unique<FunctionDefNode>(nameToken.lexeme, std::move(params), returnType, std::move(body), line);
}

std::unique_ptr<ASTNode> Parser::parseCallExpr(const Token& identifierToken) {
    consume(TokenType::TOKEN_LPARAN, "Expected '(' in function call.");
    
    std::vector<std::unique_ptr<ASTNode>> arguments;
    if (!check(TokenType::TOKEN_RPARAN)) {
        do {
            arguments.push_back(parseExpression());
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after arguments");
    
    return std::make_unique<CallExprNode>(identifierToken.lexeme, std::move(arguments), identifierToken.line);
}

std::unique_ptr<ASTNode> Parser::parseReturnStmt() {
    advance(); 
    int line = previous().line;
    std::unique_ptr<ASTNode> value = nullptr;
    if (!check(TokenType::TOKEN_SEMICOLON)) {
        value = parseExpression();
    }
    
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after return statement");
    return std::make_unique<ReturnStmtNode>(std::move(value), line);
}
