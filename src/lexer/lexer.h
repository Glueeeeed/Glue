#ifndef LEXER_H
#define LEXER_H
#include <string>
#include <vector>

enum struct TokenType {
    TYPE,KEYWORD,IDENTIFIER, GLUE,

    EQUALS, EQUALS_EQUALS, GREATER, GREATER_EQUALS, LESS, LESS_EQUALS, NEGATIVE, NEGATIVE_EQUALS,

    LPAREN, RPAREN, LBRACE, RBRACE,

    DOT,SEMICOLON, COMMA, COLON,

    STRING, NUMBER, NUMBER_FLOAT, NUMBER_DOUBLE, BOND_FALSE, BOND_TRUE, BOND_TORN,

    PLUS, MINUS, MULTIPLY,DIVISION,

    END_OF_FILE,
    UNKNOWN
};



struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};




std::vector<Token> lexer(const char* input);

#endif