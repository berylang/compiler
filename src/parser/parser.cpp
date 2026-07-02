/*

    Parser Class

    implementation of the Parser class's utility functions and parse() function.
    it includes - 
        1. parse() - public method
        2. utility functions
        3. parseBlock() and parseStatements()

*/

#include "parser.h"
#include "ast/programnode.h"
#include <stdexcept>
#include <iostream>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0), errors(false) {}

std::unique_ptr<ASTNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>(); 
    
    while (!isAtEnd()) {
        try {
            if (check(TokenType::TOKEN_RUN)) {
                advance();
                int runline = previous().line;
                consume(TokenType::TOKEN_LBRACE, "Expected '{' after run");
                auto runBlock = std::make_unique<RunBlockNode>(runline);
                while (!isAtEnd() && !check(TokenType::TOKEN_RBRACE)) {
                    try {
                        runBlock->statements.push_back(parseStatement());
                    } catch(ParseError& e) {
                        synchronize();
                    }
                }
                consume(TokenType::TOKEN_RBRACE, "Expected '}' after run block");
                program->runBlock = std::move(runBlock);
            } 
            else if (check(TokenType::TOKEN_FUNC)) {
                program->globals.push_back(parseFunctionDef(AccessSpecifier::PUBLIC));
            } else if (check(TokenType::TOKEN_ENUM)) {
                program->globals.push_back(parseEnumDecl());
            } else if (check(TokenType::TOKEN_IMPORT)) {
                program->globals.push_back(parseImportDecl());
            } else if (check(TokenType::TOKEN_EXTERN)) {
                program->globals.push_back(parseExternDecl());
            } else if(check(TokenType::TOKEN_CLASS)) {
                program->globals.push_back(parseClassDecl());
            } else {
                bool isConst = false;
                if (check(TokenType::TOKEN_CONST)) { advance(); isConst = true; }
                if (isTypeToken(peek().type)) {
                    if (!isConst && isArrayDecl()) {
                        auto decl = parseArrayDecl();
                        program->globals.push_back(std::move(decl));
                    } else {
                        auto decls = parseVarDecl(AccessSpecifier::PUBLIC,isConst);
                        for (auto& d : decls) program->globals.push_back(std::move(d));
                    }
                } else if (isClassVarDecl()) { 
                    auto decls = parseVarDecl(AccessSpecifier::PUBLIC, isConst);
                    for (auto&d : decls) program->globals.push_back(std::move(d));
                }else {
                    std::cerr << "Bery:Error [Line " << peek().line << "]: Unexpected token '" << peek().lexeme << "'\n";
                    errors = true;
                    throw ParseError();
                }
            }
        } catch(ParseError& e) {
            synchronize();
        }
    }

    if (!program->runBlock) {
        std::cerr << "Bery:Error: no run{} block found\n";
        errors = true;
    }
    
    return program;
}


Token Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

Token Parser::peek() {
    if (current >= (int)tokens.size()) return tokens.back(); 
    return tokens[current];

}

Token Parser::previous() {
    return tokens[current - 1];
}

bool Parser::isAtEnd() {
    return peek().type == TokenType::TOKEN_EOF;
}

bool Parser::check(TokenType type) { 
    return peek().type == type;
}

Token Parser::consume(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    errors = true;
    std::cerr<< "Bery:Error [Line " << peek().line << "]: " << msg << "\n";
    throw ParseError();
}

bool Parser::isTypeToken(TokenType t) {
    return t == TokenType::TOKEN_INT ||
            t == TokenType::TOKEN_BIGINT ||
           t == TokenType::TOKEN_BOOL ||
           t == TokenType::TOKEN_FLOAT ||
           t == TokenType::TOKEN_DOUBLE ||
           t == TokenType::TOKEN_CHAR ||
           t == TokenType::TOKEN_STRING; 
}

std::unique_ptr<BlockNode> Parser::parseBlock() {
    int line = previous().line;

    auto block = std::make_unique<BlockNode>(line);
    while(!isAtEnd() && !check(TokenType::TOKEN_RBRACE)){
        try {
            block->statements.push_back(parseStatement());
        }
        catch(ParseError& e) {
            synchronize();
        }
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after block");
    return block;
    
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (check(TokenType::TOKEN_IF)) {
        return parseIfStmt();
    }
    if (check(TokenType::TOKEN_ELSE)){
        advance();
        return parseBlock();
    }
    if(check(TokenType::TOKEN_WHILE)){
        return parseWhileStmt();
    }
    if(check(TokenType::TOKEN_DOWHILE)){
        return parseDoWhileStmt();
    }
    if(check(TokenType::TOKEN_FOR)){
        return parseForStmt();
    }
    bool isConst = false;
    if(check(TokenType::TOKEN_CONST)){
        advance();
        isConst = true;
    }
    if (check(TokenType::TOKEN_SWITCH)) {
        return parseSwitchStmt();
    }
    if (check(TokenType::TOKEN_BREAK)) {
        return parseBreakStmt();
    }
    if(check(TokenType::TOKEN_CONTINUE)) {
        return parseContinueStmt();
    }
    if(check(TokenType::TOKEN_PASS)) {
        return parsePassStmt();
    }
    if(check(TokenType::TOKEN_RETURN)){
        return parseReturnStmt();
    }
    if (check(TokenType::TOKEN_ENUM)) {
        return parseEnumDecl();
    }
    

    if(isTypeToken(peek().type) || isClassVarDecl()){
        if(!isConst && isTypeToken(peek().type) && isArrayDecl()) return parseArrayDecl();
        std::vector<std::unique_ptr<ASTNode>>decls = parseVarDecl(AccessSpecifier::PUBLIC,isConst);
        if(decls.size() == 1){
            return std::move(decls[0]);    
        };
        auto multiDeclBlock = std::make_unique<BlockNode>(previous().line);
        
        for(auto& d:decls){
            multiDeclBlock->statements.push_back(std::move(d));
        }
        return multiDeclBlock;
    }
    auto expr = parseExpression();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after expression");
    return expr;
}
