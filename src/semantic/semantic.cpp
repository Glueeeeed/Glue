#include "semantic.h"
#include "../parser/parser.h"
#include <iostream>
#include <sstream>

std::string nodeTypeToString(NodeType t) {
    switch (t) {
        case NodeType::NUMBER: return "'int'";
        case NodeType::NUMBER_DOUBLE: return "'double'";
        case NodeType::STRING: return "'string'";
        case NodeType::NUMBER_FLOAT : return "'float'";
        case NodeType::BOND : return "'bond'";
        default: return "unknown";
    }
}

void SemanticAnalyzer::analyse(const ASTNode *root) {
    visit(root);
}

void SemanticAnalyzer::visit(const ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NodeType::PROGRAM:
            for (const auto& child : node->children) {
                visit(child.get());
            }
            break;
        case NodeType::DECLARATION:
            visitDeclaration(node);
            break;

        case NodeType::ASSIGNMENT:
            visitAssignment(node);
            break;
        case NodeType::FUNCTION_CALL:
            for (const auto& child : node->children) {
                inferType(child.get());
            }
            break;
    }
}

void SemanticAnalyzer::visitDeclaration(const ASTNode* node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* typeNode = node->children[1].get();
    const ASTNode* valNode  = node->children[2].get();

    std::string varName = idNode->value;
    std::string varTypeFormatted = "'" + typeNode->value + "'";
    std::string varType =  typeNode->value;
    NodeType valueType = inferType(valNode);

    if (symbols.count(varName)) {
        expect("Semantic Error: variable '" + varName + "' is already declared in this scope", idNode->line, idNode->column);
    }

    if (!isCompatible(varType, valueType)) {
        expect("Semantic Error: type mismatch; cannot assign " + nodeTypeToString(valueType) + " to variable '" + varName + "' of type " + varTypeFormatted, valNode->line, valNode->column);
    }


    declareSymbol(node, idNode->isConst, idNode->stickyUsed, idNode->isSticky );


}


NodeType SemanticAnalyzer::inferType(const ASTNode* node) {

    if (node->type == NodeType::BINARY_OPERATION) {
        NodeType leftType = inferType(node->children[0].get());
        NodeType rightType = inferType(node->children[1].get());

        if (leftType == NodeType::STRING || rightType == NodeType::STRING) {
            std::string errorMsg = "Semantic Error: Operator '" + node->value +
                                   "' is not supported for type 'string'";
            expect(errorMsg, node->line, node->column);
        }
        if (node->value == "/") {
            if (node->children[1]->type == NodeType::NUMBER || node->children[1]->type == NodeType::NUMBER_DOUBLE || node->children[1]->type == NodeType::NUMBER_FLOAT) {
                double val = std::stod(node->children[1]->value);
                if (val == 0.0) {
                    expect("Semantic Error: Division by zero", node->line, node->column);
                }
            }
        }



        if (leftType == NodeType::NUMBER_DOUBLE || rightType == NodeType::NUMBER_DOUBLE) return NodeType::NUMBER_DOUBLE;
        if (leftType == NodeType::NUMBER_FLOAT || rightType == NodeType::NUMBER_FLOAT) return NodeType::NUMBER_FLOAT;

        return NodeType::NUMBER;
    }

    if (node->type == NodeType::IDENTIFIER) {
        if (symbols.count(node->value)) {
            std::string typeStr = symbols[node->value].type;
            if (typeStr == "int") return NodeType::NUMBER;
            if (typeStr == "double") return NodeType::NUMBER_DOUBLE;
            if (typeStr == "float") return NodeType::NUMBER_FLOAT;
            if (typeStr == "string") return NodeType::STRING;
            if (typeStr == "bond") return NodeType::BOND;
        }
        return NodeType::BOND; // Unknown identifier or unknown type
    }

    return node->type;
}

bool SemanticAnalyzer::isCompatible(const std::string& declaredType, NodeType valueType) {

    if (declaredType == "int" && valueType == NodeType::NUMBER) {
        return true;
    }

    if (declaredType == "double" && (valueType == NodeType::NUMBER || valueType == NodeType::NUMBER_DOUBLE)) {
        return true;
    }

    if (declaredType == "string" && valueType == NodeType::STRING) {
        return true;
    }

    if (declaredType == "float" && valueType == NodeType::NUMBER_FLOAT) {
        return true;
    }

    if (declaredType == "boolean" || declaredType == "bool" && valueType == NodeType::BOOLEAN) {
        return true;
    }

    return false;
}

void SemanticAnalyzer::expect(std::string msg, int line, int column) {
    std::stringstream stream;
    stream << msg;
    if (line > 0) {
        stream << " at " << line << ":" << column;
    }
    stream << std::endl;
    throw ParseError(stream.str());
}

void SemanticAnalyzer::declareSymbol(const ASTNode *node, bool isConst, bool stickyUsed, bool isSticky) {
    std::string idNode = node->children[0]->value;
    std::string typeNode = node->children[1]->value;
    SymbolInfo info;
    info.type = typeNode;
    info.isConst = isConst;
    info.isSticky = isSticky;
    info.stickyUsed = stickyUsed;
    symbols[idNode] = info;

}

void SemanticAnalyzer::visitAssignment(const ASTNode* node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* valNode = node->children[1].get();
    std::string varName = idNode->value;
    NodeType valueType = inferType(valNode);


    if (symbols.count(varName) == 0) {
        expect("Semantic Error: variable '" + varName + "' is not declared in this scope", idNode->line, idNode->column);
    }


    SymbolInfo& info = symbols[varName];
    std::string declaredType = info.type;

    if (info.isConst == true) {
        expect("Semantic Error: cannot assign to variable '" + varName + "' because it is a constant", idNode->line, idNode->column);
    }


    if (info.isSticky == true) {
        if (info.stickyUsed == true) {
            expect("Semantic Error: variable '" + varName + "' is 'sticky' and has already been reassigned once", idNode->line, idNode->column);
        } else {
            info.stickyUsed = true;
        }
    }



    if (!isCompatible(declaredType, valueType)) {
        expect("Semantic Error: type mismatch; cannot assign " + nodeTypeToString(valueType) + " to variable '" + varName + "' of type '" + declaredType + "'", valNode->line, valNode->column);
    }
}






