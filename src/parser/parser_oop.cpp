
#include "parser.h"
#include <stdexcept>
#include <iostream>
#include "ast/classes.h"

std::unique_ptr<ASTNode> Parser::parseClassDecl() {
    advance();
    int line = previous().line;
    Token className = consume(TokenType::TOKEN_IDENT, "Expected class name");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' after class name");
    
    auto attrSection = parseAttributeSection();

    std::unique_ptr<MethodSectionNode> methodSection = nullptr;
    if (check(TokenType::TOKEN_METHODS)) {
        methodSection = parseMethodSection();
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after class body");
    return std::make_unique<ClassDefNode>(className.lexeme, std::move(attrSection), std::move(methodSection), line);
}

std::unique_ptr<AttributeSectionNode> Parser::parseAttributeSection() {
    consume(TokenType::TOKEN_ATTRIBUTES, "Exptected 'attributes' section");
    consume(TokenType::TOKEN_LBRACKET, "Expected '[' after 'attributes'");
    Token selfToken = consume(TokenType::TOKEN_IDENT, "Expected self-reference identifier");
    consume(TokenType::TOKEN_RBRACKET, "Expected ']' after self-reference");
    consume(TokenType::TOKEN_DCOLON, "Exptected '::' after attributes seciton verbose");

    std::vector<std::unique_ptr<ASTNode>> attributes;
    while(!isAtEnd()) {

        AccessSpecifier access = AccessSpecifier::PUBLIC;
        if(check(TokenType::TOKEN_PUBLIC)){
            advance();
            access=AccessSpecifier::PUBLIC;
        }
        else if(check(TokenType::TOKEN_PRIVATE)){
            advance();
            access=AccessSpecifier::PRIVATE;
        }
        else if(check(TokenType::TOKEN_PROTECTED)){
            advance();
            access=AccessSpecifier::PROTECTED;
        }
        if(!isTypeToken(peek().type))
            break;
        
        auto vars= parseVarDecl(access, false);
        for (auto& v : vars) {
            attributes.push_back(std::move(v));
        }
    }
    return std::make_unique<AttributeSectionNode>(selfToken.lexeme, std::move(attributes), selfToken.line);
}

std::unique_ptr<MethodSectionNode> Parser::parseMethodSection() {
    int line = peek().line;
    consume(TokenType::TOKEN_METHODS, "Expected 'methods' section");
    consume(TokenType::TOKEN_DCOLON, "Exptected '::' after 'methods");

    std::vector<std::unique_ptr<ASTNode>> methods;
    while(!isAtEnd()){
        AccessSpecifier access = AccessSpecifier::PUBLIC;
        if(check(TokenType::TOKEN_PUBLIC)){
            advance();
            access=AccessSpecifier::PUBLIC;
        }
        else if(check(TokenType::TOKEN_PRIVATE)){
            advance();
            access=AccessSpecifier::PRIVATE;
        }
        else if(check(TokenType::TOKEN_PROTECTED)){
            advance();
            access=AccessSpecifier::PROTECTED;
        }
        if(!check(TokenType::TOKEN_FUNC))
            break;
        methods.push_back(parseFunctionDef(access));
    }

    return std::make_unique<MethodSectionNode>(std::move(methods), line);
}

bool Parser::isClassVarDecl() {
    return peek().type == TokenType::TOKEN_IDENT && current + 1 < (int)tokens.size() && tokens[current + 1].type == TokenType::TOKEN_IDENT;
}