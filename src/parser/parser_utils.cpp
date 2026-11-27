#include <string>
#include <vector>
#include "parser.h"



std::vector<std::pair<std::string, std::string>> Parser::parse_args() {
    std::vector<std::pair<std::string, std::string>> params;
    if (currentToken().type != TokenType::RPAREN) {
        while (true) {
            if (currentToken().type != TokenType::TYPE) {
                expect("Syntax Error: expected parameter type", currentToken().filePath, currentToken().line, currentToken().column);
            }
            std::string paramType = currentToken().value;
            nextToken();

            if (currentToken().type == TokenType::QUESTION) {
                paramType += "?";
                nextToken();
            }

            if (currentToken().type != TokenType::IDENTIFIER) {
                expect("Syntax Error: expected parameter name", currentToken().filePath, currentToken().line, currentToken().column);
            }
            std::string paramName = currentToken().value;
            nextToken();

            params.push_back({paramType, paramName});

            if (currentToken().type == TokenType::COMMA) {
                nextToken();
            } else if (currentToken().type == TokenType::RPAREN) {
                break;
            } else {
                expect("Syntax Error: expected ',' or ')' in parameter list", currentToken().filePath, currentToken().line, currentToken().column);
            }
        }
    }
    return params;
}

    void Parser::consume(TokenType expectedType, const std::string& errMsg) {
        if (currentToken().type != expectedType) {
            expect( errMsg, currentToken().filePath, currentToken().line, currentToken().column);
        }
        nextToken();
    }

std::pair<std::string, std::string> Parser::parseFunctionHeader() {
    Token typeOrIdToken = currentToken();
    std::string returnType = "void";
    std::string funcName;

    if (typeOrIdToken.type == TokenType::TYPE) {
        returnType = typeOrIdToken.value;
        nextToken();

        if (currentToken().type == TokenType::QUESTION) {
            returnType += "?";
            nextToken();
        }

        Token idToken = currentToken();
        if (idToken.type != TokenType::IDENTIFIER) {
            expect("Syntax Error: expected function name", idToken.filePath, idToken.line, idToken.column);
        }
        funcName = idToken.value;
    } else if (typeOrIdToken.type == TokenType::IDENTIFIER) {
        returnType = "void";
        funcName = typeOrIdToken.value;
    } else {
        expect("Syntax Error: expected return type or function name after 'func'", typeOrIdToken.filePath, typeOrIdToken.line, typeOrIdToken.column);
    }

    return {returnType, funcName};
}
