#include "lexer.h"
#include <unordered_map>
#include <stdexcept>
#include <iostream>


static std::unordered_map<std::string, TokenType> keywords = {
    {"int", TokenType::TOKEN_INT},
    {"float", TokenType::TOKEN_FLOAT},
    {"bigint", TokenType::TOKEN_BIGINT},
    {"double", TokenType::TOKEN_DOUBLE},
    {"run", TokenType::TOKEN_RUN},
    {"bool", TokenType::TOKEN_BOOL},
    {"true", TokenType::TOKEN_TRUE},
    {"false", TokenType::TOKEN_FALSE},
    {"char", TokenType::TOKEN_CHAR}

    
};

Lexer::Lexer(const std::string& source) : source(source), current(0), line(1), errors(false) {}

std::vector<Token> Lexer::tokanize() {
    while (!isAtEnd()) {
        skipWhitespaces();
        if (!isAtEnd()) scanToken();
    }
    tokens.push_back({TokenType::TOKEN_EOF, "", line});
    return tokens;
}

void Lexer::scanToken() {
    char c = advance();

    if (isAlpha(c)) {
        scanIdentifierOrKeyword();
        return;
    }

    if (isDigit(c)) {
        scanNumber();
        return;
    }
    switch(c) {
        case '=' : 
            tokens.push_back({TokenType::TOKEN_EQUAL, "=", line});
            break;
        case ';': 
            tokens.push_back({TokenType::TOKEN_SEMICOLON, ";", line});
            break;
        case '{':
            tokens.push_back({TokenType::TOKEN_LBRACE, "{", line});
            break;
        case '}':
            tokens.push_back({TokenType::TOKEN_RBRACE, "}", line});
            break;
        case ',':
            tokens.push_back({TokenType::TOKEN_COMMA, ",", line});
            break;
        case '\'':
            scanCharLit();
            break;
        case '-':
            if(peek()=='-'){
                if(peekNext()=='-'){ 
                    advance();
                    advance();
                    skipComments(false);
                    return;
                }
                else if(peekNext()=='!'){ 
                    advance();
                    advance();
                    skipComments(true);
                    return;
                }
            }
    }
}
//@todo - add TOKEN_DECIMAL_LIT;
void Lexer::scanNumber() {
    int start = current - 1;
    while (!isAtEnd() && isDigit(peek())) advance(); 

    if (!isAtEnd() && peek() == '.' && isDigit(peekNext())){ 
        advance();
        while (!isAtEnd() && isDigit(peek())) advance();

        tokens.push_back({TokenType::TOKEN_DECIMAL_LIT, source.substr(start, current - start), line});
        
    }
    else {
        tokens.push_back({TokenType::TOKEN_INT_LIT, source.substr(start, current - start), line});
    }
}

// @todo - Enhance it later for Errors
void Lexer::scanCharLit() {
    if (errors) return; 

    if (peek() == '\'') { 
        errors = true;
        std::cerr << "Bery:Error:Empty Char Literal\n";
        advance(); 
        return;
    }

    char value = 0;

    if (peek() == '\\') { 
        advance(); 
        if (isAtEnd() || peek() == '\'') { 
            errors = true;
            std::cerr << "Bery:Error:Incomplete Escape Sequence\n";
            return;
        }

        char es = advance();
        switch (es) {
            case 'n':  value = '\n'; break;
            case 't':  value = '\t'; break;
            case 'r':  value = '\r'; break;
            case '\\': value = '\\'; break;
            case '0':  value = '\0'; break;
            case '"':  value = '\"'; break;
            case '\'': value = '\''; break;
            default:
                errors = true;
                std::cerr << "Bery:Error:Invalid Escape Sequence\n";
                return;
        }
    } 
    
    else {
        if (peek() == '\n' || peek() == '\r') {
            errors = true;
            std::cerr << "Bery:Error:Newline in char literal\n";
            return;
        }
        value = advance();
    }

    if (!isAtEnd() && peek() == '\'') {        
        advance(); 
        tokens.push_back({TokenType::TOKEN_CHAR_LIT, std::string(1,value), line});
        return;
    }

    bool foundClosingQuote = false;
    while (!isAtEnd() && peek() != '\'' && peek() != '\n' && peek() != '\r') {
        advance();
    }
    if (!isAtEnd() && peek() == '\'') {
        advance(); 
        foundClosingQuote = true;
    }
    errors = true;

    if (foundClosingQuote) {
        std::cerr << "Bery:Error:Multi-character Char Literal\n";
    } else {
        std::cerr << "Bery:Error:Unclosed Char Literal\n";
    }
}

void Lexer::scanIdentifierOrKeyword() {
    int start = current - 1;
    while (!isAtEnd() && isAlphaNumeric(peek())) advance();
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back({checkKeyword(lexeme), lexeme, line});
}
bool Lexer::isAtEnd() {
    return current >= (int) source.size();
}

void Lexer::skipWhitespaces() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') advance();
        else if (c == '\n') {
            line++;
            advance();
        }
        else break;
    }
}

void Lexer::skipComments(bool isMLC){
    if(isMLC){
        while(!isAtEnd()){
            if(peek()=='!' && peekNext()=='-'){
                advance();
                advance();
                if(peek()=='-'){
                    advance();
                    return;
                }
            }
            if(peek()=='\n'){line++;}
            advance();
        }
        errors=true;
        std::cerr<<"Bery:Error:Unclosed Comments\n";
        
    }
    else{
        while(!isAtEnd() && peek()!='\n'){advance();}

    }
}

TokenType Lexer::checkKeyword(const std::string& lexeme) {
    auto it = keywords.find(lexeme); 
    if (it != keywords.end()) return it->second;
    return TokenType::TOKEN_IDENT;
}
bool Lexer::isAlphaNumeric(char c) {return isAlpha(c) || isDigit(c);}
bool Lexer::isAlpha(char c) {return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');}
bool Lexer::isDigit(char c) {return c >= '0' && c <= '9';}
char Lexer::advance() {return source[current++];}
char Lexer::peek() {return source[current];}
char Lexer::peekNext() {return source[current + 1];}

bool Lexer::hasErrors() {return errors;}