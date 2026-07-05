#include "../lexer/lexer.h"
#include "parser.h"
#include "../AST/ast.h"
#include <complex>
#include <functional>
#include <functional>
#include <iostream>

#include "../semantic/semantic.h"


void Parser::parse() {
    if (tokenPos == 0) {
        ast.startProgramTree();
     }
    Token cToken = currentToken();
    Token nextToken = peekToken();
    if (cToken.type == TokenType::IDENTIFIER && nextToken.type == TokenType::EQUALS) {
        parseAssign();
    } else if (cToken.type == TokenType::TYPE || cToken.type == TokenType::KEYWORD && cToken.value == "const" || cToken.type == TokenType::KEYWORD && cToken.value == "sticky") {
        parseDeclaration();
    } else if (cToken.type == TokenType::IDENTIFIER && nextToken.type != TokenType::EQUALS) {
        expect("Syntax Error: invalid or unexpected token after identifier '" + cToken.value + "'", currentToken().line, currentToken().column);
    }

    if (cToken.type == TokenType::UNKNOWN) {
        expect("Syntax Error: unexpected token '" + cToken.value + "'", currentToken().line, currentToken().column);
    }

    if (cToken.type == TokenType::KEYWORD && nextToken.type == TokenType::LPAREN) {
        parseFunctionCall();
    }
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

    Token valToken = currentToken();
    if (valToken.type != TokenType::NUMBER  && valToken.type != TokenType::NUMBER_DOUBLE  && valToken.type != TokenType::NUMBER_FLOAT && valToken.type != TokenType::STRING && valToken.type != TokenType::BOOLEAN_TRUE &&  valToken.type != TokenType::BOOLEAN_FALSE) {
        expect("Syntax Error: expected a value (number, string, or boolean) after '='", valToken.line, valToken.column);
    }
    if (valToken.type == TokenType::NUMBER) {
        ast.addAssignment(identifier, ast.makeNumber(valToken.value, valToken.line, valToken.column), idToken.line, idToken.column);
    } else if (valToken.type == TokenType::NUMBER_DOUBLE) {
        ast.addAssignment(identifier, ast.makeDouble(valToken.value, valToken.line, valToken.column), idToken.line, idToken.column);
    } else if (valToken.type == TokenType::NUMBER_FLOAT) {
        ast.addAssignment(identifier, ast.makeFloat(valToken.value, valToken.line, valToken.column), idToken.line, idToken.column);
    } else if (valToken.type == TokenType::BOOLEAN_TRUE || valToken.type == TokenType::BOOLEAN_FALSE) {
        ast.addAssignment(identifier, ast.makeBool(valToken.value, valToken.line, valToken.column), idToken.line, idToken.column);
    } else {
        ast.addAssignment(identifier, ast.makeString(valToken.value, valToken.line, valToken.column), idToken.line, idToken.column);
    }
    nextToken();
    if (currentToken().type != TokenType::SEMICOLON) {
        expect("Syntax Error: expected ';' after assignment to '" + identifier + "'", currentToken().line, currentToken().column);
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
    } else if (currentToken().type == TokenType::KEYWORD && currentToken().value != "const" && currentToken().value != "sticky") {
        expect("Syntax Error: unexpected keyword '" + currentToken().value + "'", currentToken().line, currentToken().column);
    }

    Token typeToken = currentToken();
    std::string type = typeToken.value;
    if (typeToken.type != TokenType::TYPE) {
        expect("Syntax Error: expected a type name (e.g., int, string) in declaration", typeToken.line, typeToken.column);
    }
    nextToken();

    Token idToken = currentToken();
    std::string identifier = idToken.value;
    if (idToken.type != TokenType::IDENTIFIER) {
        expect("Syntax Error: expected an identifier after type '" + type + "'", idToken.line, idToken.column);
    }
    nextToken();

    if (currentToken().type != TokenType::EQUALS) {
        expect("Syntax Error: expected '=' after identifier '" + identifier + "'", currentToken().line, currentToken().column);
    }
    nextToken();

    Token valToken = currentToken();
    if (valToken.type != TokenType::NUMBER  && valToken.type != TokenType::NUMBER_DOUBLE  && valToken.type != TokenType::NUMBER_FLOAT && valToken.type != TokenType::STRING && valToken.type != TokenType::BOOLEAN_TRUE &&  valToken.type != TokenType::BOOLEAN_FALSE) {
        expect("Syntax Error: expected an initial value after '=' in declaration of '" + identifier + "'", valToken.line, valToken.column);
    }
    if (valToken.type == TokenType::NUMBER) {
        ast.addDeclaration(identifier, ast.makeNumber(valToken.value, valToken.line, valToken.column), type, isConstant,  stickyUsed,  isSticky, declLine, declCol);
    } else if (valToken.type == TokenType::NUMBER_DOUBLE) {
        ast.addDeclaration(identifier, ast.makeDouble(valToken.value, valToken.line, valToken.column), type, isConstant, stickyUsed,  isSticky, declLine, declCol );
    } else if (valToken.type == TokenType::NUMBER_FLOAT) {
        ast.addDeclaration(identifier, ast.makeFloat(valToken.value, valToken.line, valToken.column), type, isConstant, stickyUsed,  isSticky, declLine, declCol);
    } else if ( valToken.type == TokenType::BOOLEAN_TRUE || valToken.type == TokenType::BOOLEAN_FALSE) {
        ast.addDeclaration(identifier, ast.makeBool(valToken.value, valToken.line, valToken.column), type, isConstant, stickyUsed,  isSticky, declLine, declCol);
    } else if (valToken.type == TokenType::STRING) {
        ast.addDeclaration(identifier, ast.makeString(valToken.value, valToken.line, valToken.column), type, isConstant, stickyUsed,  isSticky, declLine, declCol);
    }
    nextToken();
    if (currentToken().type != TokenType::SEMICOLON) {
        expect("Syntax Error: expected ';' at the end of declaration of '" + identifier + "'", currentToken().line, currentToken().column);
    }

    if (peekToken().type != TokenType::END_OF_FILE) {
        nextToken();
    }
    parse();

}

void Parser::parseFunctionCall() {
    if (currentToken().type != TokenType::KEYWORD) {
        expect("Syntax Error: expected a keyword (like 'shout') to start a function call", currentToken().line, currentToken().column);
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
        Token valToken = currentToken();
        if (valToken.type == TokenType::NUMBER) {
            ast.addFunctionArgument(ast.makeNumber(valToken.value, valToken.line, valToken.column));
        } else if (valToken.type == TokenType::STRING) {
            ast.addFunctionArgument(ast.makeString(valToken.value, valToken.line, valToken.column));
        } else if (valToken.type == TokenType::IDENTIFIER) {
            ast.addFunctionArgument(ast.makeIdentifier(valToken.value, valToken.line, valToken.column));
        } else if (valToken.type == TokenType::NUMBER_DOUBLE) {
            ast.addFunctionArgument(ast.makeDouble(valToken.value, valToken.line, valToken.column));
        } else if (valToken.type == TokenType::NUMBER_FLOAT) {
            ast.addFunctionArgument(ast.makeFloat(valToken.value, valToken.line, valToken.column));
        } else {
            expect("Syntax Error: expected a valid expression as function argument", valToken.line, valToken.column);
        }
        nextToken();

        if (currentToken().type == TokenType::RPAREN) {
            nextToken();
            isEnd = true;
        } else {
            if (currentToken().type == TokenType::COMMA) {
                nextToken();
            } else {
                expect("Syntax Error: expected ',' or ')' between function arguments", currentToken().line, currentToken().column);
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


