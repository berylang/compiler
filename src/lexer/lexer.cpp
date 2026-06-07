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
    {"false", TokenType::TOKEN_FALSE}
    
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