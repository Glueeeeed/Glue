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
    } else if (cToken.type == TokenType::TYPE || cToken.type == TokenType::KEYWORD && cToken.value == "glue" || cToken.type == TokenType::KEYWORD && cToken.value == "sticky") {
        parseDeclaration();
    } else if (cToken.type == TokenType::IDENTIFIER && nextToken.type != TokenType::EQUALS) {
        expect("SyntaxError:  invalid or unexpected token", currentToken().line, currentToken().column);
    }

    std::stringstream stream;
    stream << "'" + cToken.value + "'";
    if (cToken.type == TokenType::UNKNOWN) {
        expect("SyntaxError: unexpected token " + stream.str(), currentToken().line, currentToken().column);
    }

    if (cToken.type == TokenType::KEYWORD && nextToken.type == TokenType::LPAREN) {
        parseFunctionCall();
    }
}


void Parser::parseAssign() {
     std::string identifier = currentToken().value;
    if (currentToken().type != TokenType::IDENTIFIER) {
        expect("SyntaxError: expected token 'IDENTIFIER'", currentToken().line, currentToken().column);
    }
    nextToken();

    if (currentToken().type != TokenType::EQUALS) {
        expect("SyntaxError: expected token '='", currentToken().line, currentToken().column);
    }
    nextToken();

    if (currentToken().type != TokenType::NUMBER  && currentToken().type != TokenType::NUMBER_DOUBLE  && currentToken().type != TokenType::NUMBER_FLOAT && currentToken().type != TokenType::STRING && currentToken().type != TokenType::BOND_TRUE &&  currentToken().type != TokenType::BOND_FALSE && currentToken().type != TokenType::BOND_TORN) {
        expect("SyntaxError: expected token 'VALUE'", currentToken().line, currentToken().column);
    }
    if (currentToken().type == TokenType::NUMBER) {
        ast.addAssignment(identifier, ast.makeNumber(currentToken().value));
    } else if (currentToken().type == TokenType::NUMBER_DOUBLE) {
        ast.addAssignment(identifier, ast.makeDouble(currentToken().value));
    } else if (currentToken().type == TokenType::NUMBER_FLOAT) {
        ast.addAssignment(identifier, ast.makeFloat(currentToken().value));
    } else if (currentToken().type == TokenType::BOND_TRUE || currentToken().type == TokenType::BOND_FALSE || currentToken().type == TokenType::BOND_TORN) {
        ast.addAssignment(identifier, ast.makeBool(currentToken().value));
    } else {
        ast.addAssignment(identifier, ast.makeString(currentToken().value));
    }
    nextToken();
    if (currentToken().type != TokenType::SEMICOLON) {
        expect("SyntaxError: expected token ';'", currentToken().line, currentToken().column);
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

    if (currentToken().type == TokenType::KEYWORD && currentToken().value == "glue") {
        isConstant = true;
        nextToken();
    } else if (currentToken().type == TokenType::KEYWORD && currentToken().value == "sticky") {
        isSticky = true;
        nextToken();
    } else if (currentToken().type == TokenType::KEYWORD && currentToken().value != "glue" && currentToken().value != "sticky") {
        expect("SyntaxError: unexpected keyword '" + currentToken().value + "'", currentToken().line, currentToken().column);
    }

    std::string type = currentToken().value;
    if (currentToken().type != TokenType::TYPE) {
        expect("SyntaxError: expected token 'TYPE'", currentToken().line, currentToken().column);
    }
    nextToken();

    std::string identifier = currentToken().value;
    if (currentToken().type != TokenType::IDENTIFIER) {
        expect("SyntaxError: expected token 'IDENTIFIER'", currentToken().line, currentToken().column);
    }
    nextToken();

    if (currentToken().type != TokenType::EQUALS) {
        expect("SyntaxError: expected token '='", currentToken().line, currentToken().column);
    }
    nextToken();

    if (currentToken().type != TokenType::NUMBER  && currentToken().type != TokenType::NUMBER_DOUBLE  && currentToken().type != TokenType::NUMBER_FLOAT && currentToken().type != TokenType::STRING && currentToken().type != TokenType::BOND_TRUE &&  currentToken().type != TokenType::BOND_FALSE && currentToken().type != TokenType::BOND_TORN) {
        expect("SyntaxError: expected token 'VALUE'", currentToken().line, currentToken().column);
    }
    if (currentToken().type == TokenType::NUMBER) {
        ast.addDeclaration(identifier, ast.makeNumber(currentToken().value), type, isConstant,  stickyUsed,  isSticky);
    } else if (currentToken().type == TokenType::NUMBER_DOUBLE) {
        ast.addDeclaration(identifier, ast.makeDouble(currentToken().value), type, isConstant, stickyUsed,  isSticky );
    } else if (currentToken().type == TokenType::NUMBER_FLOAT) {
        ast.addDeclaration(identifier, ast.makeFloat(currentToken().value), type, isConstant, stickyUsed,  isSticky);
    } else if ( currentToken().type == TokenType::BOND_TRUE || currentToken().type == TokenType::BOND_FALSE || currentToken().type == TokenType::BOND_TORN ) {
        ast.addDeclaration(identifier, ast.makeBool(currentToken().value), type, isConstant, stickyUsed,  isSticky);
    } else if (currentToken().type == TokenType::STRING) {
        ast.addDeclaration(identifier, ast.makeString(currentToken().value), type, isConstant, stickyUsed,  isSticky);
    }
    nextToken();
    if (currentToken().type != TokenType::SEMICOLON) {
        expect("SyntaxError: expected token ';'", currentToken().line, currentToken().column);
    }

    if (peekToken().type != TokenType::END_OF_FILE) {
        nextToken();
    }
    parse();

}

void Parser::parseFunctionCall() {
    if (currentToken().type != TokenType::KEYWORD) {
        expect("SyntaxError: expected token 'KEYWORD'", currentToken().line, currentToken().column);
    } else {
        nextToken();
    }


    if (currentToken().type != TokenType::LPAREN) {
        expect("SyntaxError: expected token '('", currentToken().line, currentToken().column);
    } else {
        nextToken();
    }

    parseArgument();

    if (currentToken().type != TokenType::SEMICOLON) {
        expect("SyntaxError: expected token ';'", currentToken().line, currentToken().column);
    }

    if (peekToken().type != TokenType::END_OF_FILE) {
        nextToken();
    }

    parse();
}

void Parser::parseArgument() {
    bool isEnd = false;
    while (!isEnd) {
        if (currentToken().type == TokenType::NUMBER) {
            ast.addFunctionArgument(ast.makeNumber(currentToken().value));
        } else if (currentToken().type == TokenType::STRING) {
            ast.addFunctionArgument(ast.makeString(currentToken().value));
        } else if (currentToken().type == TokenType::IDENTIFIER) {
            ast.addFunctionArgument(ast.makeIdentifier(currentToken().value));
        } else if (currentToken().type == TokenType::NUMBER_DOUBLE) {
            ast.addFunctionArgument(ast.makeDouble(currentToken().value));
        } else if (currentToken().type == TokenType::NUMBER_FLOAT) {
            ast.addFunctionArgument(ast.makeFloat(currentToken().value));
        } else {
            expect("SyntaxError: expected expression", currentToken().line, currentToken().column);
        }
        nextToken();

        if (currentToken().type == TokenType::RPAREN) {
            nextToken();
            isEnd = true;
        } else {
            if (currentToken().type == TokenType::COMMA) {
                nextToken();
            } else {
                expect("SyntaxError: expected ',' or ')'", currentToken().line, currentToken().column);
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


