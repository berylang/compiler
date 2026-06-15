#pragma once
#include "ast/node.h"
#include "ast/blocknode.h"
#include "../lexer/token.h"
#include <vector>
#include <memory>
#include <exception>


class ParseError : public std::exception {};
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
    void synchronize();
    
    std::vector<std::unique_ptr<ASTNode>> parseVarDecl(bool isConst);
    std::unique_ptr<ASTNode> parseLiteral();
    bool isArrayDecl();
    std::unique_ptr<ASTNode> parseArrayDecl();
    void parseArrayInitializer(std::vector<std::unique_ptr<ASTNode>>& initializers);
    
    //@expressions
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parsePrimary();
    std::unique_ptr<ASTNode> parsePostfix();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parseMultiplicative();
    std::unique_ptr<ASTNode> parseAdditive();
    std::unique_ptr<ASTNode> parseShift();
    std::unique_ptr<ASTNode> parseBitwise();
    std::unique_ptr<ASTNode> parseBetween();
    std::unique_ptr<ASTNode> parseRelational();
    std::unique_ptr<ASTNode> parseEquality();
    std::unique_ptr<ASTNode> parseLogicalAnd();
    std::unique_ptr<ASTNode> parseLogicalOr();
    std::unique_ptr<ASTNode> parseTernary();
    
    //@blocks
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<BlockNode> parseBlock();
    std::unique_ptr<ASTNode> parseIfStmt();
    std::unique_ptr<ASTNode> parseSwitchStmt();
    std::unique_ptr<ASTNode> parseBreakStmt();


    std::unique_ptr<ASTNode> parseWhileStmt();
    std::unique_ptr<ASTNode> parseDoWhileStmt();

    std::unique_ptr<ASTNode> parseFunctionDef();
    std::unique_ptr<ASTNode> parseCallExpr(const Token& identifierToken);
    std::unique_ptr<ASTNode> parseReturnStmt();
};

