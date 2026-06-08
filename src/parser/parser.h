#pragma once
#include "ast/node.h"
#include "../lexer/token.h"
#include <vector>
#include <memory>

class Parser {
    public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ASTNode> parse();
    bool hasErrors();

    private:
    std::vector<Token> tokens;
    int current;
    bool errors;

    Token advance();
    Token peek();
    Token previous();
    bool isAtEnd();
    bool check(TokenType type);
    Token consume(TokenType type, const std::string& msg);
    bool isTypeToken(TokenType t);
    
    std::vector<std::unique_ptr<ASTNode>> parseVarDecl(bool isConst);
    std::unique_ptr<ASTNode> parseLiteral();
    bool isArrayDecl();
    std::unique_ptr<ASTNode> parseArrayDecl();
    
    //Expressions
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parsePrimary();
    std::unique_ptr<ASTNode> parsePostfix();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parseMultiplicative();
};

