#include "ast.h"
#include <iostream>

void AST::startProgramTree() {
    root = std::make_unique<ASTNode>(NodeType::PROGRAM);
}

std::unique_ptr<ASTNode> AST::makeString(const std::string& value) {
    return std::make_unique<ASTNode>(NodeType::STRING, value);
}

std::unique_ptr<ASTNode> AST::makeNumber(const std::string& value1) {
    return std::make_unique<ASTNode>(NodeType::NUMBER, value1);
}

std::unique_ptr<ASTNode> AST::makeDouble(const std::string& value1) {
    return std::make_unique<ASTNode>(NodeType::NUMBER_DOUBLE, value1);
}

std::unique_ptr<ASTNode> AST::makeBool(const std::string& value1) {
    return std::make_unique<ASTNode>(NodeType::BOND, value1);
}
std::unique_ptr<ASTNode> AST::makeFloat(const std::string& value1) {
    return std::make_unique<ASTNode>(NodeType::NUMBER_FLOAT, value1);
}

std::unique_ptr<ASTNode> AST::makeIdentifier(const std::string& name) {
    return std::make_unique<ASTNode>(NodeType::IDENTIFIER, name);
}


std::unique_ptr<ASTNode> AST::makeDeclaration(const std::string& name, bool isConst, bool stickyUsed, bool isSticky ) {
    return std::make_unique<ASTNode>(NodeType::IDENTIFIER, name, isConst, isSticky, stickyUsed);
}

std::unique_ptr<ASTNode> AST::makeBinaryOp(const std::string& op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right) {
    auto node = std::make_unique<ASTNode>(NodeType::BINARY_OPERATION, op);
    node->children.push_back(std::move(left));
    node->children.push_back(std::move(right));
    return node;
}

std::unique_ptr<ASTNode> AST::makeType(const std::string &name) {
    return std::make_unique<ASTNode>(NodeType::TYPE, name);
}


void AST::addChild(std::unique_ptr<ASTNode> child) {
    root->children.push_back(std::move(child));
}

void AST::addAssignment(const std::string& var, std::unique_ptr<ASTNode> expr) {
    auto assign = std::make_unique<ASTNode>(NodeType::ASSIGNMENT);
    assign->children.push_back(makeIdentifier(var));
    assign->children.push_back(std::move(expr));
    addChild(std::move(assign));
}

void AST::addDeclaration(const std::string& var, std::unique_ptr<ASTNode> expr, std::string type, bool isConst, bool stickyUsed, bool isSticky) {
    auto assign = std::make_unique<ASTNode>(NodeType::DECLARATION);
    assign->children.push_back(makeDeclaration(var, isConst,  stickyUsed,  isSticky));
    assign->children.push_back(makeType(type));
    assign->children.push_back(std::move(expr));
    addChild(std::move(assign));
}

void AST::addFunctionCall(std::string &name, std::vector<std::unique_ptr<ASTNode>> args) {
    auto call = std::make_unique<ASTNode>(NodeType::FUNCTION_CALL, name);
    for (auto &arg : args) {
        call->children.push_back(std::move(arg));
    }
    addChild(std::move(call));
}

void AST::addFunctionArgument(std::unique_ptr<ASTNode> arg) {
    args.push_back(std::move(arg));
}


// Supporting method

void AST::printAST(const ASTNode* node, int indent) {
    for (int i = 0; i < indent; i++) std::cout << "  ";

    std::cout << "[";

    switch (node->type) {
        case NodeType::PROGRAM: std::cout << "PROGRAM"; break;
        case NodeType::EXPRESSION: std::cout << "EXPRESSION"; break;
        case NodeType::ASSIGNMENT: std::cout << "ASSIGNMENT"; break;
        case NodeType::FUNCTION_CALL: std::cout << "FUNCTION_CALL"; break;
        case NodeType::IDENTIFIER: std::cout << "IDENTIFIER"; break;
        case NodeType::DECLARATION: std::cout << "DECLARATION"; break;
        case NodeType::STRING: std::cout << "STRING"; break;
        case NodeType::TYPE: std::cout << "TYPE"; break;
        case NodeType::NUMBER: std::cout << "NUMBER"; break;
        case NodeType::NUMBER_DOUBLE: std::cout << "NUMBER_DOUBLE"; break;
        case NodeType::NUMBER_FLOAT: std::cout << "NUMBER_FLOAT"; break;
        case NodeType::BINARY_OPERATION: std::cout << "BINARY_OPERATION"; break;
        case NodeType::BOND: std::cout << "BOND"; break;
    }

    if (!node->value.empty())
        std::cout << ": " << node->value;

    std::cout << "]" << std::endl;

    for (const auto& child : node->children) {
        printAST(child.get(), indent + 1);
    }
}


