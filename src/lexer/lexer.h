#pragma once 
#include <string>
#include <vector>
#include "token.h"

class Lexer {
    public:
    Lexer(const std::string& source);
    std::vector<Token> tokanize();
    bool hasErrors();


    private:

    std::string source;
    int current;
    int line;
    bool errors;
    std::vector<Token> tokens;

    char advance();
    char peek();
    char peekNext();
    bool isAtEnd();
    void skipWhitespaces();
    void skipComments(bool isMLC);
    void scanToken();
    void scanIdentifierOrKeyword();
    void scanNumber(); // eitehr flaot or int
    bool isDigit(char c);
    bool isAlpha(char c);
    bool isAlphaNumeric(char c);
    TokenType checkKeyword(const std::string& lexeme);
};