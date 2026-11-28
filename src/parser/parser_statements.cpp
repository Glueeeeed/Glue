#include "parser.h"

void Parser::parseBlockStatement(ASTNode* parentBlock) {
    nextToken();
    auto blockNode = std::make_unique<ASTNode>(NodeType::BLOCK);
    while (currentToken().type != TokenType::RBRACE && currentToken().type != TokenType::END_OF_FILE) {
        parseStatement(blockNode.get());
    }
    consume(TokenType::RBRACE, "Syntax Error: expected '}' to close block");
    parentBlock->children.push_back(std::move(blockNode));
}

void Parser::parseImportStatement() {
    nextToken();
    Token token = currentToken();
    std::string importName = token.value;
    token.value = token.value;
    consume(TokenType::IDENTIFIER, "Syntax Error: expected identifier after 'import'");
    consume(TokenType::SEMICOLON, "Syntax Error: expected ';' after import statement");
    ast.addImport(importName);
}

void Parser::parseReturnStatement(ASTNode* parentBlock, const Token& token) {
    int retLine = token.line;
    int retCol = token.column;
    nextToken();

    auto retNode = std::make_unique<ASTNode>(NodeType::RETURN_STATEMENT, "", false, false, false, retLine, retCol);

    if (currentToken().type != TokenType::SEMICOLON) {
        retNode->children.push_back(parseExpression());
    }

    consume(TokenType::SEMICOLON, "expected ';' after return statement");

    parentBlock->children.push_back(std::move(retNode));
}

void Parser::parseDeclarationStatement(ASTNode* parentBlock) {
    if (currentToken().type == TokenType::TYPE && peekToken().type == TokenType::LBRACKET) {
        std::string type = currentToken().value;
        nextToken();
        parentBlock->children.push_back(parseArrayDeclaration(type));
    } else {
        parentBlock->children.push_back(parseDeclaration());
    }
    consume(TokenType::SEMICOLON, "Syntax Error: expected ';' after declaration");
}

void Parser::parseIdentifierStatement(ASTNode* parentBlock, const Token& token) {
    if (peekToken().type == TokenType::EQUALS) {
        parentBlock->children.push_back(parseAssign());
    } else if (peekToken().type == TokenType::LPAREN) {
        parentBlock->children.push_back(parseFunctionCall());
        if (currentToken().type != TokenType::SEMICOLON) {
            expect("Syntax Error: expected ';' after function call", currentToken().filePath, currentToken().line, currentToken().column);
        }
        nextToken();
    } else if (peekToken().type == TokenType::LBRACKET) {
        nextToken();
        auto node = ast.makeIdentifier(token.value, token.line, token.column);
        nextToken();
        auto index = parseExpression();
        consume(TokenType::RBRACKET, "Syntax Error: expected ']' after array index");
        node = ast.makeArrayAccess(std::move(node), std::move(index));
        
        if (currentToken().type == TokenType::EQUALS) {

            nextToken();
            auto value = parseExpression();
            consume(TokenType::SEMICOLON, "Syntax Error: expected ';' after array assignment");
            auto assign = std::make_unique<ASTNode>(NodeType::ASSIGNMENT, "", false, false, false, token.line, token.column);
            assign->children.push_back(std::move(node));
            assign->children.push_back(std::move(value));
            parentBlock->children.push_back(std::move(assign));
        } else {
            expect("Syntax Error: array access must be followed by '=' or ';' in statement context", currentToken().filePath, currentToken().line, currentToken().column);
        }
    } else {
        expect("Syntax Error: unexpected identifier in statement context", token.filePath, token.line, token.column);
    }
}

void Parser::parseIfStatement(ASTNode* parentBlock, const Token& token) {
    int ifLine = token.line;
    int ifCol = token.column;
    nextToken();

    if (currentToken().type != TokenType::LPAREN) {
        expect("Syntax Error: expected '(' after 'if'", currentToken().filePath, currentToken().line, currentToken().column);
    }
    nextToken();

    auto condition = parseExpression();

    if (currentToken().type != TokenType::RPAREN) {
        expect("Syntax Error: expected ')' after 'if' condition", currentToken().filePath, currentToken().line, currentToken().column);
    }
    nextToken();

    auto ifNode = std::make_unique<ASTNode>(NodeType::IF_STATEMENT, "", false, false, false, ifLine, ifCol);
    ifNode->children.push_back(std::move(condition));

    auto thenBlock = std::make_unique<ASTNode>(NodeType::BLOCK);
    parseStatement(thenBlock.get());
    ifNode->children.push_back(std::move(thenBlock));

    if (currentToken().type == TokenType::KEYWORD && currentToken().value == "else") {
        nextToken();
        auto elseBlock = std::make_unique<ASTNode>(NodeType::BLOCK);
        parseStatement(elseBlock.get());
        ifNode->children.push_back(std::move(elseBlock));
    }

    parentBlock->children.push_back(std::move(ifNode));
}

void Parser::parseWhileStatement(ASTNode* parentBlock, const Token& token) {
    int whileLine = token.line;
    int whileCol = token.column;
    nextToken();

    if (currentToken().type != TokenType::LPAREN) {
        expect("Syntax Error: expected '(' after 'while'", currentToken().filePath, currentToken().line, currentToken().column);
    }
    nextToken();

    auto condition = parseExpression();

    if (currentToken().type != TokenType::RPAREN) {
        expect("Syntax Error: expected ')' after 'while' condition", currentToken().filePath, currentToken().line, currentToken().column);
    }
    nextToken();

    auto whileNode = std::make_unique<ASTNode>(NodeType::WHILE_STATEMENT, "", false, false, false, whileLine, whileCol);
    whileNode->children.push_back(std::move(condition));

    auto bodyBlock = std::make_unique<ASTNode>(NodeType::BLOCK);
    parseStatement(bodyBlock.get());
    whileNode->children.push_back(std::move(bodyBlock));

    parentBlock->children.push_back(std::move(whileNode));
}

void Parser::parseCallStatement(ASTNode* parentBlock) {
    parentBlock->children.push_back(parseFunctionCall());
    if (currentToken().type != TokenType::SEMICOLON) {
        expect("Syntax Error: expected ';' after function call", currentToken().filePath, currentToken().line, currentToken().column);
    }
    nextToken();
}


void Parser::parseBreakStatement(ASTNode* parentBlock, const Token& token) {
    int line = token.line;
    int col = token.column;
    nextToken();
    consume(TokenType::SEMICOLON, "Syntax Error: expected ';' after '" + token.value + "'");

    auto breakNode = std::make_unique<ASTNode>(NodeType::BREAK_STATEMENT, token.value, false, false, false, line, col);
    parentBlock->children.push_back(std::move(breakNode));
}

void Parser::parseContinueStatement(ASTNode* parentBlock, const Token& token) {
    int line = token.line;
    int col = token.column;
    nextToken();
    consume(TokenType::SEMICOLON, "Syntax Error: expected ';' after '" + token.value + "'");

    auto continueNode = std::make_unique<ASTNode>(NodeType::CONTINUE_STATEMENT, token.value, false, false, false, line, col);
    parentBlock->children.push_back(std::move(continueNode));
}



