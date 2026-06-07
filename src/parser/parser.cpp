
#include "parser.h"
#include "ast/programnode.h"
#include "ast/literals.h"
#include "ast/vardecl.h"
#include <stdexcept>
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0), errors(false) {}

std::unique_ptr<ASTNode> Parser::parse() {
   auto program = std::make_unique<ProgramNode>();
   while (!isAtEnd() && !check(TokenType::TOKEN_RUN)) {
       bool isConst = false;
       if (check(TokenType::TOKEN_CONST)) { advance(); isConst = true; }
       if (isTypeToken(peek().type)) {
           auto decls = parseVarDecl(isConst);
           for (auto& d : decls)
               program->globals.push_back(std::move(d));
       } else {
           std::cerr << "bery: unexpected token '" << peek().lexeme << "' at line " << peek().line << "\n";
           errors = true;
           advance();
       }
   }

   if (check(TokenType::TOKEN_RUN)) {
       advance();
       consume(TokenType::TOKEN_LBRACE, "Expected '{' after run");
       auto runBlock = std::make_unique<RunBlockNode>();
       while (!isAtEnd() && !check(TokenType::TOKEN_RBRACE)) {
           bool isConst = false;
           if (check(TokenType::TOKEN_CONST)) { advance(); isConst = true; }
           if (isTypeToken(peek().type)) {
               auto decls = parseVarDecl(isConst);
               for (auto& d : decls)
                   runBlock->statements.push_back(std::move(d));
           } else {
               while (!isAtEnd() && !check(TokenType::TOKEN_SEMICOLON)
                                 && !check(TokenType::TOKEN_RBRACE))
                   advance();
               if (check(TokenType::TOKEN_SEMICOLON)) advance();
           }
       }
       consume(TokenType::TOKEN_RBRACE, "Expected '}' after run block");
       program->runBlock = std::move(runBlock);
   } else {
       std::cerr << "bery: error: no run{} block found\n";
       errors = true;
   }
   return program;
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseVarDecl(bool isConst) {
    std::vector<std::unique_ptr<ASTNode>> decls;
    Token typeToken = advance();
    std::string varType = typeToken.lexeme;
    do {

        Token name = consume(TokenType::TOKEN_IDENT, "Expected identifier");
        std::unique_ptr<ASTNode> value = nullptr;

        if (check(TokenType::TOKEN_EQUAL)) {
            advance();
            value = parseLiteral();
        }

        decls.push_back(std::make_unique<VarDeclNode>(varType, name.lexeme, std::move(value), isConst));


    } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';'");
    return decls;
}
std::unique_ptr<ASTNode> Parser::parseLiteral() {
    Token t = peek();

    switch(t.type) {
        case TokenType::TOKEN_INT_LIT: 
            advance();
            return std::make_unique<IntLitNode>(std::stoll(t.lexeme));
        
        case TokenType::TOKEN_DECIMAL_LIT: 
            advance();
            return std::make_unique<DecimalLitNode>(std::stod(t.lexeme));
        
        case TokenType::TOKEN_TRUE:
            advance();
            return std::make_unique<BoolLitNode>(true);
        case TokenType::TOKEN_FALSE:
            advance();
            return std::make_unique<BoolLitNode>(false);
        default:
            errors = true;
            throw std::runtime_error("Expected literal at line " + std::to_string(t.line));
    }
}
Token Parser::advance() {return tokens[current++];}

Token Parser::peek() {return tokens[current];}

Token Parser::previous() {return tokens[current - 1];}

bool Parser::isAtEnd() {return peek().type == TokenType::TOKEN_EOF;}

bool Parser::check(TokenType type) { return peek().type == type;}

Token Parser::consume(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    errors = true;
    throw std::runtime_error(msg + " at line " + std::to_string(peek().line));
}

bool Parser::isTypeToken(TokenType t) {
    return t == TokenType::TOKEN_INT ||
            t == TokenType::TOKEN_BIGINT ||
           t == TokenType::TOKEN_BOOL ||
           t == TokenType::TOKEN_FLOAT ||
           t == TokenType::TOKEN_DOUBLE;
}

bool Parser::hasErrors() {return errors;}