#pragma once
#include <string>

enum class TokenType {
    TOKEN_INT, TOKEN_IDENT, TOKEN_EQUAL, TOKEN_INT_LIT, TOKEN_SEMICOLON,
    TOKEN_EOF, TOKEN_RUN, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_CONST, TOKEN_COMMA, TOKEN_BOOL,
    TOKEN_TRUE, TOKEN_FALSE ,
    TOKEN_FLOAT, TOKEN_DECIMAL_LIT,
    TOKEN_BIGINT,
    TOKEN_DOUBLE,
    TOKEN_CHAR, TOKEN_CHAR_LIT

};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
};
