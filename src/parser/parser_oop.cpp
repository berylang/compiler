
#include "parser.h"
#include <stdexcept>
#include <iostream>
#include "ast/classes.h"

std::unique_ptr<ASTNode> Parser::parseClassDecl() {
    advance();
    int line = previous().line;
    Token className = consume(TokenType::TOKEN_IDENT, "Expected class name");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' after class name");
    consume(TokenType::TOKEN_ATTRIBUTES, "Expected 'attributes' section");
    consume(TokenType::TOKEN_LBRACKET, "Expected '[' after attributes");

    Token selfToken = consume(TokenType::TOKEN_IDENT, "Expected self reference");

    consume(TokenType::TOKEN_RBRACKET, "Expected ']' after self reference");
    consume(TokenType::TOKEN_DCOLON, "Expected '::' after attributes section");

    std::vector<std::unique_ptr<ASTNode>> attributes;

    if (!isTypeToken(peek().type)) {
        std::cerr << "Bery:Error [Line " << peek().line<< "]: Class must contain at least one attribute\n";
        errors = true;
        throw ParseError();
    }

    while (!isAtEnd() && isTypeToken(peek().type)) {
        auto vars = parseVarDecl(false);
        for (auto &v : vars) {
            attributes.push_back(std::move(v));
        }
    }
    auto attributeSection =std::make_unique<AttributeSectionNode>(selfToken.lexeme,std::move(attributes),selfToken.line);
    std::unique_ptr<MethodSectionNode> methodSection = nullptr;

    if (check(TokenType::TOKEN_METHODS)) {
        advance();
        int methodsLine = previous().line;

        consume(TokenType::TOKEN_DCOLON, "Expected '::' after methods");
        std::vector<std::unique_ptr<ASTNode>> methods;

        while (!isAtEnd() && check(TokenType::TOKEN_FUNC)) {
            methods.push_back(parseFunctionDef());
        }
        methodSection =std::make_unique<MethodSectionNode>(std::move(methods), methodsLine);
    }

    consume(TokenType::TOKEN_RBRACE, "Expected '}' after class definition");

    return std::make_unique<ClassDefNode>(className.lexeme,std::move(attributeSection),std::move(methodSection),line);
}