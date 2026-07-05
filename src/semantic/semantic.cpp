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
    }
}

void SemanticAnalyzer::visitDeclaration(const ASTNode* node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* typeNode = node->children[1].get();
    const ASTNode* valNode  = node->children[2].get();

    std::string varName = idNode->value;
    std::string varTypeFormatted = "'" + typeNode->value + "'";
    std::string varType =  typeNode->value;
    NodeType valueType = valNode->type;

    if (symbols.count(varName)) {
        expect("Semantic error: variable '" + varName + "'" + " is already declared");
    }

    if (!isCompatible(varType, valueType)) {
        expect("Semantic error: Cannot assign a value of type "+ nodeTypeToString(valueType) + " to a variable of type " + varTypeFormatted);
    }


    declareSymbol(node, idNode->isConst, idNode->stickyUsed, idNode->isSticky );


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

    if (declaredType == "bond" && valueType == NodeType::BOND) {
        return true;
    }

    return false;
}

void SemanticAnalyzer::expect(std::string msg) {
    std::stringstream stream;
    stream << msg << std::endl;
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
    const ASTNode* typeNode = node->children[1].get();
    std::string varName = idNode->value;
    NodeType valueType = typeNode->type;


    if (symbols.count(varName) == 0) {
        expect("Semantic Error: variable '" + varName + "'" + " is not declared");
    }


    const SymbolInfo& info = symbols[varName];
    std::string declaredType = info.type;

    if (info.isConst == true) {
        expect("Semantic error: '" + varName + "'" + " is constant");
    }


    if (info.isSticky == true) {
        if (info.stickyUsed == true) {
            expect("Semantic error: '" + varName + "'" + " can be assigned only once");
        } else {
            info.stickyUsed = true;
        }
    }



    if (!isCompatible(declaredType, valueType)) {
        expect("Semantic error: Cannot assign a value of type "+ nodeTypeToString(valueType) + " to a variable of type '" + declaredType + "'");
    }






}






