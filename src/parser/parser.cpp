#include "../lexer/lexer.h"
#include "parser.h"
#include "../AST/ast.h"
#include <complex>
#include <functional>
#include <functional>
#include <iostream>

#include "../semantic/semantic.h"


std::unique_ptr<ASTNode> Parser::parseLiteral() {
    Token token = currentToken();

    if (token.type == TokenType::NUMBER) {
        auto node = ast.makeNumber(token.value, token.line, token.column);
        nextToken();
        return node;
    } else if (token.type == TokenType::NUMBER_DOUBLE) {
        auto node = ast.makeDouble(token.value, token.line, token.column);
        nextToken();
        return node;
    } else if (token.type == TokenType::NUMBER_FLOAT) {
        auto node = ast.makeFloat(token.value, token.line, token.column);
        nextToken();
        return node;
    } else if (token.type == TokenType::STRING) {
        auto node = ast.makeString(token.value, token.line, token.column);
        nextToken();
        return node;
    } else if (token.type == TokenType::BOOLEAN_TRUE || token.type == TokenType::BOOLEAN_FALSE) {
        auto node = ast.makeBool(token.value, token.line, token.column);
        nextToken();
        return node;
    } else if (token.type == TokenType::IDENTIFIER) {
        auto node = ast.makeIdentifier(token.value, token.line, token.column);
        nextToken();
        return node;
    } else if (token.type == TokenType::LPAREN) {
        nextToken();
        auto node = parseExpression();
        if (currentToken().type != TokenType::RPAREN) {
            expect("Syntax Error: expected ')'", currentToken().line, currentToken().column);
        }
        nextToken();
        return node;
    }

    expect("Syntax Error: expected a literal or identifier", token.line, token.column);
    return nullptr;
}

void Parser::parse() {
    if (tokenPos == 0) {
        ast.startProgramTree();
    }
    Token cToken = currentToken();
    Token nextToken = peekToken();
    if (cToken.type == TokenType::IDENTIFIER && nextToken.type == TokenType::EQUALS) {
        parseAssign();
    } else if (cToken.type == TokenType::TYPE || cToken.type == TokenType::KEYWORD && cToken.value == "const" || cToken.
               type == TokenType::KEYWORD && cToken.value == "sticky") {
        parseDeclaration();
    } else if (cToken.type == TokenType::IDENTIFIER && nextToken.type != TokenType::EQUALS) {
        expect("Syntax Error: invalid or unexpected token after identifier '" + cToken.value + "'", currentToken().line,
               currentToken().column);
    }

    if (cToken.type == TokenType::UNKNOWN) {
        expect("Syntax Error: unexpected token '" + cToken.value + "'", currentToken().line, currentToken().column);
    }

    if (cToken.type == TokenType::KEYWORD && nextToken.type == TokenType::LPAREN) {
        parseFunctionCall();
    }
}

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseComparison();
}

std::unique_ptr<ASTNode> Parser::parseAddition() {
    auto left = parseMultiplication();

    while (currentToken().type == TokenType::PLUS || currentToken().type == TokenType::MINUS) {
        Token opToken = currentToken();
        nextToken();

        auto right = parseMultiplication();
        left = ast.makeBinaryOp(opToken.value, std::move(left), std::move(right), opToken.line, opToken.column);
    }

    return left;
}

std::unique_ptr<ASTNode> Parser::parseComparison() {
    auto left = parseAddition();

    while (currentToken().type == TokenType::EQUALS_EQUALS || currentToken().type == TokenType::NOT_EQUALS ||currentToken().type == TokenType::GREATER ||currentToken().type == TokenType::GREATER_EQUALS ||currentToken().type == TokenType::LESS ||currentToken().type == TokenType::LESS_EQUALS) {
        Token opToken = currentToken();
        nextToken();

        auto right = parseAddition();
        left = ast.makeBinaryOp(opToken.value, std::move(left), std::move(right), opToken.line, opToken.column);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseMultiplication() {
    auto left = parseLiteral();

    while (currentToken().type == TokenType::MULTIPLY || currentToken().type == TokenType::DIVISION) {
        Token opToken = currentToken();
        nextToken();

        auto right = parseLiteral();
        left = ast.makeBinaryOp(opToken.value, std::move(left), std::move(right), opToken.line, opToken.column);
    }

    return left;
}


void Parser::parseAssign() {
    Token idToken = currentToken();
    std::string identifier = idToken.value;
    if (idToken.type != TokenType::IDENTIFIER) {
        expect("Syntax Error: expected an identifier before '" + idToken.value + "'", idToken.line, idToken.column);
    }
    nextToken();

    Token eqToken = currentToken();
    if (eqToken.type != TokenType::EQUALS) {
        expect("Syntax Error: expected '=' after identifier '" + identifier + "'", eqToken.line, eqToken.column);
    }
    nextToken();

    auto expr = parseExpression();
    ast.addAssignment(identifier, std::move(expr), idToken.line, idToken.column);

    if (currentToken().type != TokenType::SEMICOLON) {
        expect("Syntax Error: expected ';' after assignment to '" + identifier + "'", currentToken().line,
               currentToken().column);
    }
    
    if (peekToken().type != TokenType::END_OF_FILE) {
        nextToken();
    }
    parse();
}

void Parser::parseDeclaration() {
    bool isConstant = false;
    bool isSticky = false;
    bool stickyUsed = false;
    int declLine = currentToken().line;
    int declCol = currentToken().column;

    if (currentToken().type == TokenType::KEYWORD && currentToken().value == "const") {
        isConstant = true;
        nextToken();
    } else if (currentToken().type == TokenType::KEYWORD && currentToken().value == "sticky") {
        isSticky = true;
        nextToken();
    } else if (currentToken().type == TokenType::KEYWORD && currentToken().value != "const" && currentToken().value !=
               "sticky") {
        expect("Syntax Error: unexpected keyword '" + currentToken().value + "'", currentToken().line,
               currentToken().column);
    }

    Token typeToken = currentToken();
    std::string type = typeToken.value;
    if (typeToken.type != TokenType::TYPE) {
        expect("Syntax Error: expected a type name (e.g., int, string) in declaration", typeToken.line,
               typeToken.column);
    }
    nextToken();

    Token idToken = currentToken();
    std::string identifier = idToken.value;
    if (idToken.type != TokenType::IDENTIFIER) {
        expect("Syntax Error: expected an identifier after type '" + type + "'", idToken.line, idToken.column);
    }
    nextToken();

    if (currentToken().type != TokenType::EQUALS) {
        expect("Syntax Error: expected '=' after identifier '" + identifier + "'", currentToken().line,
               currentToken().column);
    }
    nextToken();

    auto expr = parseExpression();
    ast.addDeclaration(identifier, std::move(expr), type, isConstant, stickyUsed, isSticky, declLine, declCol);

    if (currentToken().type != TokenType::SEMICOLON) {
        expect("Syntax Error: expected ';' at the end of declaration of '" + identifier + "'", currentToken().line,
               currentToken().column);
    }

    if (peekToken().type != TokenType::END_OF_FILE) {
        nextToken();
    }
    parse();
}

void Parser::parseFunctionCall() {
    if (currentToken().type != TokenType::KEYWORD) {
        expect("Syntax Error: expected a keyword (like 'shout') to start a function call", currentToken().line,
               currentToken().column);
    } else {
        nextToken();
    }


    if (currentToken().type != TokenType::LPAREN) {
        expect("Syntax Error: expected '(' before function arguments", currentToken().line, currentToken().column);
    } else {
        nextToken();
    }

    parseArgument();

    if (currentToken().type != TokenType::SEMICOLON) {
        expect("Syntax Error: expected ';' after function call", currentToken().line, currentToken().column);
    }

    if (peekToken().type != TokenType::END_OF_FILE) {
        nextToken();
    }

    parse();
}

void Parser::parseArgument() {
    bool isEnd = false;
    while (!isEnd) {
        auto expr = parseExpression();
        ast.addFunctionArgument(std::move(expr));

        if (currentToken().type == TokenType::RPAREN) {
            nextToken();
            isEnd = true;
        } else {
            if (currentToken().type == TokenType::COMMA) {
                nextToken();
            } else {
                expect("Syntax Error: expected ',' or ')' between function arguments", currentToken().line,
                       currentToken().column);
            }
        }
    }

    std::string function = "shout";
    ast.addFunctionCall(function, std::move(ast.args));
}


Token Parser::currentToken() const {
    if (tokenPos >= tokens.size()) {
        throw std::out_of_range("Parser: no more tokens!");
    }

    return tokens[tokenPos];
}

Token Parser::peekToken() const {
    if (tokenPos >= tokens.size()) {
        throw std::out_of_range("Parser: no more tokens!");
    }

    return tokens[tokenPos + 1];
}

Token Parser::nextToken() {
    if (tokenPos >= tokens.size()) {
        throw std::out_of_range("Parser: no more tokens!");
    }

    ++tokenPos;
    return tokens[tokenPos];
}

void Parser::printASTCall() {
    // ast.printAST(ast.getRoot()); // FOR DEBUG
    semantic.analyse(ast.getRoot());
    codegen.generateCode(ast.getRoot());
}


void Parser::expect(std::string msg, int line, int column) {
    std::stringstream stream;
    stream << msg << " at " << line << ":" << column << std::endl;
    throw ParseError(stream.str());
}
