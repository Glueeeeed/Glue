#include "semantic.h"
#include "../parser/parser.h"
#include "scope.h"
#include <sstream>

std::string nodeTypeToString(NodeType t) {
    switch (t) {
        case NodeType::NUMBER: return "'int'";
        case NodeType::NUMBER_DOUBLE: return "'double'";
        case NodeType::STRING: return "'string'";
        case NodeType::NUMBER_FLOAT : return "'float'";
        case NodeType::BOND : return "'bond'";
        case NodeType::BOOLEAN: return "'boolean'";
        default: return "unknown";
    }
}

void SemanticAnalyzer::analyse(const ASTNode *root) {
    visit(root);
}

void SemanticAnalyzer::visit(const ASTNode* node) {
    if (!node) return;
    switch (node->type) {
        case NodeType::PROGRAM: {
            bool hasMain = false;
            for (const auto& child : node->children) {
                if (child->type == NodeType::FUNCTION_DECLARATION && (child->value == "main" || child->value == "Main")) {
                    hasMain = true;
                }
                visit(child.get());
            }
            if (!hasMain) {
                expect("Compile Error: Program must contain a 'main' function");
            }
            break;
        }
        case NodeType::FUNCTION_DECLARATION:
            visitFunctionDeclaration(node);
            break;
        case NodeType::BLOCK: {
            symbolTable.enterScope();
            for (const auto& child : node->children) {
                visit(child.get());
            }
            symbolTable.exitScope();
            break;
        }
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
        case NodeType::RETURN_STATEMENT: {
            if (currentFunctionReturnType == "void") {
                if (!node->children.empty()) {
                    expect("Compile Error: Function '" + currentFunctionName + "' with return type 'void' cannot return a value", node->line, node->column);
                }
            } else {
                if (node->children.empty()) {
                    expect("Compile Error: Function '" + currentFunctionName + "' with return type '" + currentFunctionReturnType + "' must return a value", node->line, node->column);
                } else {
                    NodeType retType = inferType(node->children[0].get());
                    if (!isCompatible(currentFunctionReturnType, retType)) {
                        expect("Compile Error: Function '" + currentFunctionName + "' with return type '" + currentFunctionReturnType + "' cannot return value of type " + nodeTypeToString(retType), node->line, node->column);
                    }
                }
            }
            break;
        }
        case NodeType::IF_STATEMENT: {
            NodeType condType = inferType(node->children[0].get());
            if (condType != NodeType::BOOLEAN && condType != NodeType::NUMBER && condType != NodeType::NUMBER_DOUBLE && condType != NodeType::NUMBER_FLOAT) {
                expect("Compile Error: If statement condition must be of type boolean or number", node->line, node->column);
            }

            visit(node->children[1].get());

            if (node->children.size() > 2) {
                visit(node->children[2].get());
            }
            break;
        }
        case NodeType::WHILE_STATEMENT: {
            NodeType condType = inferType(node->children[0].get());
            if (condType != NodeType::BOOLEAN && condType != NodeType::NUMBER && condType != NodeType::NUMBER_DOUBLE && condType != NodeType::NUMBER_FLOAT) {
                expect("Compile Error: While loop condition must be of type boolean or number", node->line, node->column);
            }

            visit(node->children[1].get());
            break;
        }
        default:
            break;
    }
}

void SemanticAnalyzer::visitDeclaration(const ASTNode* node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* typeNode = node->children[1].get();
    const ASTNode* valNode  = node->children[2].get();

    std::string varName = idNode->value;
    std::string varTypeFormatted = "'" + typeNode->value + "'";
    std::string varType = typeNode->value;
    NodeType valueType = inferType(valNode);

    if (!isCompatible(varType, valueType)) {
        expect("Compile Error: type mismatch; cannot assign " + nodeTypeToString(valueType) + " to variable '" + varName + "' of type " + varTypeFormatted, valNode->line, valNode->column);
    }

    SymbolInfo info;
    info.type = varType;
    info.isConst = idNode->isConst;
    info.isSticky = idNode->isSticky;
    info.stickyUsed = idNode->stickyUsed;

    if (!symbolTable.declare(varName, info)) {
        expect("Compile Error: variable '" + varName + "' is already declared in this scope", idNode->line, idNode->column);
    }
}


NodeType SemanticAnalyzer::inferType(const ASTNode* node) {

    if (node->type == NodeType::FUNCTION_CALL) {
        if (symbols.count(node->value)) {
            std::string typeStr = symbols[node->value].type;
            if (typeStr == "int")  return NodeType::NUMBER;
            if (typeStr == "double") return NodeType::NUMBER_DOUBLE;
            if (typeStr == "float") return NodeType::NUMBER_FLOAT;
            if (typeStr == "string") return NodeType::STRING;
            if (typeStr == "bool" || typeStr == "boolean") return NodeType::BOOLEAN;


        }
        return NodeType::BOND;
    }

    if (node->type == NodeType::BINARY_OPERATION) {
        NodeType leftType = inferType(node->children[0].get());
        NodeType rightType = inferType(node->children[1].get());

        if (leftType == NodeType::STRING || rightType == NodeType::STRING) {
            std::string errorMsg = "Compile Error: Operator '" + node->value +
                                   "' is not supported for type 'string'";
            expect(errorMsg, node->line, node->column);
        }
        if (node->value == "/") {
            if (node->children[1]->type == NodeType::NUMBER || node->children[1]->type == NodeType::NUMBER_DOUBLE || node->children[1]->type == NodeType::NUMBER_FLOAT) {
                double val = std::stod(node->children[1]->value);
                if (val == 0.0) {
                    expect("Compile Error: Division by zero", node->line, node->column);
                }
            }
        }



        if (leftType == NodeType::NUMBER_DOUBLE || rightType == NodeType::NUMBER_DOUBLE) return NodeType::NUMBER_DOUBLE;
        if (leftType == NodeType::NUMBER_FLOAT || rightType == NodeType::NUMBER_FLOAT) return NodeType::NUMBER_FLOAT;

        return NodeType::NUMBER;
    }

    if (node->type == NodeType::IDENTIFIER) {
        SymbolInfo* info = symbolTable.lookup(node->value);
        if (info) {
            if (info->type == "int") return NodeType::NUMBER;
            if (info->type == "double") return NodeType::NUMBER_DOUBLE;
            if (info->type == "float") return NodeType::NUMBER_FLOAT;
            if (info->type == "string") return NodeType::STRING;
            if (info->type == "bond") return NodeType::BOND;
            if (info->type == "bool" || info->type == "boolean") return NodeType::BOOLEAN;
        }
        expect("Compile Error: variable '" + node->value + "' is not declared in this scope", node->line, node->column);
        return NodeType::BOND;
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

    if ((declaredType == "boolean" || declaredType == "bool") && valueType == NodeType::BOOLEAN) {
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
    const ASTNode* idNode  = node->children[0].get();
    const ASTNode* valNode = node->children[1].get();
    std::string varName = idNode->value;
    NodeType valueType = inferType(valNode);

    SymbolInfo* info = symbolTable.lookup(varName);
    if (!info) {
        expect("Compile Error: variable '" + varName + "' is not declared in this scope", idNode->line, idNode->column);
    }

    if (info->isConst) {
        expect("Compile Error: cannot assign to variable '" + varName + "' because it is a constant", idNode->line, idNode->column);
    }

    if (info->isSticky) {
        if (info->stickyUsed) {
            expect("Compile Error: variable '" + varName + "' is 'sticky' and has already been reassigned once", idNode->line, idNode->column);
        } else {
            info->stickyUsed = true;
        }
    }

    if (!isCompatible(info->type, valueType)) {
        expect("Compile Error: type mismatch; cannot assign " + nodeTypeToString(valueType) + " to variable '" + varName + "' of type '" + info->type + "'", valNode->line, valNode->column);
    }
}

void SemanticAnalyzer::visitFunctionDeclaration(const ASTNode* node) {
    std::string funcName = node->value;
    std::string returnType = node->children[0]->value;
    currentFunctionReturnType = returnType;
    currentFunctionName = funcName;

    SymbolInfo info;
    info.type = returnType;
    symbols[funcName] = info;


    if (returnType != "void") {
        bool endsWithReturn = false;
        if (node->children.size() > 1 && node->children[1]->type == NodeType::BLOCK) {
            const auto& block = node->children[1];
            if (!block->children.empty() && block->children.back()->type == NodeType::RETURN_STATEMENT) {
                endsWithReturn = true;
            }
        }
        if (!endsWithReturn) {
            expect("Compile Error: Function '" + funcName + "' with return type '" + returnType + "' must return a value", node->line, node->column);
        }
    }


    if (node->value == "main" || node->value == "Main") {


        if (returnType != "int") {
            expect("Compile Error: 'main' function must return type 'int'", node->line, node->column);
        }


        bool endsWithReturn = false;
        if (node->children.size() > 1 && node->children[1]->type == NodeType::BLOCK) {
            const auto& block = node->children[1];
            if (!block->children.empty() && block->children.back()->type == NodeType::RETURN_STATEMENT) {
                endsWithReturn = true;
            }
        }
        
        if (!endsWithReturn) {
            expect("Compile Error: Function 'main' must end with a return statement (e.g., return 0;)", node->line, node->column);
        }
    }

    for (const auto& child : node->children) {
        visit(child.get());
    }
}






