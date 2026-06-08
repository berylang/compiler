
#include "parser.h"
#include "ast/programnode.h"
#include "ast/literals.h"
#include "ast/vardecl.h"
#include "ast/arraydeclare.h"
#include <stdexcept>
#include <iostream>
#include "ast/expressions.h"

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0), errors(false) {}

std::unique_ptr<ASTNode> Parser::parse() {
   auto program = std::make_unique<ProgramNode>();
   while (!isAtEnd() && !check(TokenType::TOKEN_RUN)) {
       bool isConst = false;
       if (check(TokenType::TOKEN_CONST)) { advance(); isConst = true; }
        if (isTypeToken(peek().type)) {
           if (!isConst && isArrayDecl()) {
               auto decl = parseArrayDecl();
               program->globals.push_back(std::move(decl));
           } else {
               auto decls = parseVarDecl(isConst);
               for (auto& d : decls)
                   program->globals.push_back(std::move(d));
           }
       }else {
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
                if (!isConst && isArrayDecl()) {
                    auto decl = parseArrayDecl();
                    runBlock->statements.push_back(std::move(decl));
                } else {
                    auto decls = parseVarDecl(isConst);
                    for (auto& d : decls)
                        runBlock->statements.push_back(std::move(d));
                }
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
            value = parseExpression();
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
        case TokenType::TOKEN_CHAR_LIT:
            advance();
            return std::make_unique<CharLitNode>(t.lexeme[0]);
        default:
            errors = true;
            throw std::runtime_error("Expected literal at line " + std::to_string(t.line));
    }
}

std::unique_ptr<ASTNode> Parser::parseExpression(){
    //@change: When you implement top level expression change the call here
    return parseMultiplicative();
}

std::unique_ptr<ASTNode> Parser::parsePrimary(){
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
        case TokenType::TOKEN_CHAR_LIT:
            advance();
            return std::make_unique<CharLitNode>(t.lexeme[0]);
        case TokenType::TOKEN_IDENT:{
            advance();
            return std::make_unique<IdentNode>(t.lexeme,"");
        }
        case TokenType::TOKEN_LPARAN:{
            advance();
            auto expression = parseExpression();
            consume(TokenType::TOKEN_RPARAN, "Expected ')' after expression");
            return std::make_unique<GroupedExprNode>(std::move(expression));
        }
        default:
            errors = true;
            throw std::runtime_error("Expected valid expression at line " + std::to_string(t.line));
    }

}

std::unique_ptr<ASTNode> Parser::parsePostfix(){
    auto expr = parsePrimary();
    if(check(TokenType::TOKEN_INC) || check(TokenType::TOKEN_DEC)){
        advance();
        std::string optr = "post"+previous().lexeme;

        return std::make_unique<UnaryExprNode>(optr, std::move(expr));
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseUnary(){
    if(check(TokenType::TOKEN_BANG) ||
        check(TokenType::TOKEN_TILDE)||
        check(TokenType::TOKEN_INC)||
        check(TokenType::TOKEN_DEC)||
        check(TokenType::TOKEN_MINUS)){
            advance();
            std::string optr = previous().lexeme;
            auto operand = parsePostfix();
            return std::make_unique<UnaryExprNode>(optr, std::move(operand));
        }
    return parsePostfix();
}

std::unique_ptr<ASTNode> Parser::parseMultiplicative(){

    auto left = parseUnary();
    
    while(check(TokenType::TOKEN_STAR) ||
        check(TokenType::TOKEN_FSLASH)||
        check(TokenType::TOKEN_PERCENT)||
        check(TokenType::TOKEN_POWER)){
            
        std::string optr = peek().lexeme;
        advance();

        auto right = parseUnary();

        left = std::make_unique<BinaryExprNode>(
            optr,
            std::move(left),
            std::move(right)
        );
    }
    return left;
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
           t == TokenType::TOKEN_DOUBLE ||
           t == TokenType::TOKEN_CHAR;
}

bool Parser::isArrayDecl() {
    if (isTypeToken(peek().type)) {
        if (current + 2 < tokens.size() &&
            tokens[current + 1].type == TokenType::TOKEN_IDENT &&
            tokens[current + 2].type == TokenType::TOKEN_LBRACKET) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<ASTNode> Parser::parseArrayDecl() {
    Token typeToken = consume(peek().type, "Expected type");
    std::string elementType = typeToken.lexeme;

    Token nameToken = consume(TokenType::TOKEN_IDENT, "Expected identifier");
    std::string name = nameToken.lexeme;

    consume(TokenType::TOKEN_LBRACKET, "Expected '['");

    int size = -1;
    if (check(TokenType::TOKEN_INT_LIT)) {
        Token sizeToken = advance();
        size = std::stoi(sizeToken.lexeme);
    }

    consume(TokenType::TOKEN_RBRACKET, "Expected ']'");

    std::vector<std::unique_ptr<ASTNode>> initializers;
    if (check(TokenType::TOKEN_EQUAL)) {
        advance(); 
        consume(TokenType::TOKEN_LBRACE, "Expected '{'");
        
        if (!check(TokenType::TOKEN_RBRACE)) {
            do {
                initializers.push_back(parseExpression());
            } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
        }
        
        consume(TokenType::TOKEN_RBRACE, "Expected '}'");
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';'");

    return std::make_unique<ArrayDeclNode>(elementType, name, size, std::move(initializers));
}

bool Parser::hasErrors() {return errors;}