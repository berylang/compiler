#pragma once 

/*

LEXER DEFINITION

This is header file served for Lexer's definition.
it contains every helper functions which eventually helps the 'tokanize()' method.

*/

#include <string>
#include <vector>
#include "token.h"

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokanize();

    // @todo : change it to the Error Handler after it's implementation as independent unit of compiler.
    bool hasErrors();

private:
    // @tokens data
    // @todo : add columns inside the token, after error reporter is fully built
    std::string source;
    int current;
    int line;
    bool errors;
    std::vector<Token> tokens;

    // @pointers inside the source
    char advance();
    char peek();
    char peekNext();
    bool isAtEnd();

    // @skip unnessesory
    void skipWhitespaces();
    void skipComments(bool isMLC);

    // @actual scanning
    void scanToken();
    void scanIdentifierOrKeyword();
    void scanNumber(); 
    void scanCharLit(); 
    void scanStringLit();

    // @identification of alpha-numeric data
    bool isDigit(char c);
    bool isAlpha(char c);
    bool isAlphaNumeric(char c);

    // @distinguish between tokens
    TokenType checkKeyword(const std::string& lexeme);
};