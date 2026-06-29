#include "parser.h"


/*
    Error Recovery (Panic Mode)

    when parser hits a syntax error it throws ParseError() which unwinds the call stack instantly. 
    parse() catches it and calls synchronize() funciton, which skips tokens 
    until it finds a safe resync point (like ; or }), 
    
    then continues parsing the rest of the file that's why one error doesn't stop whole parse.
    thus we get multiple syntax errors reports in single pass.

*/
bool Parser::hasErrors() {return errors;}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::TOKEN_SEMICOLON) return;
        switch (peek().type) {
            case TokenType::TOKEN_INT:
            case TokenType::TOKEN_FLOAT:
            case TokenType::TOKEN_BIGINT:
            case TokenType::TOKEN_DOUBLE:
            case TokenType::TOKEN_STRING:
            case TokenType::TOKEN_BOOL:
            case TokenType::TOKEN_CHAR:
            case TokenType::TOKEN_CONST:
            case TokenType::TOKEN_RUN:
                return;
            default:
                break; 
        }
        advance();
    }
}
