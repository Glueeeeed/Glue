#include "semantic.h"
#include "../parser/parser.h"
#include "scope.h"
#include <sstream>

// Priority order (lowest to highest):
// byte(0) < small(1) < int(2) < long(3) < float(5) < double(6)
int SemanticAnalyzer::getTypePriority(NodeType t) {
    switch (t) {
        case NodeType::NUMBER_BYTE:   return 0;
        case NodeType::NUMBER_SMALL:  return 1;
        case NodeType::NUMBER:        return 2;
        case NodeType::NUMBER_LONG:   return 3;
        case NodeType::NUMBER_FLOAT:  return 5;
        case NodeType::NUMBER_DOUBLE: return 6;
        default:                      return -1;
    }
}

NodeType SemanticAnalyzer::promoteTypes(NodeType left, NodeType right) {
    int leftPri = getTypePriority(left);
    int rightPri = getTypePriority(right);

    // If either type is not a numeric type with priority, return the higher one or left
    if (leftPri < 0 && rightPri < 0) {
        return left;
    }
    if (leftPri < 0) {
        return right;
    }
    if (rightPri < 0) {
        return left;
    }

    // Return the type with higher priority
    if (leftPri >= rightPri) {
        return left;
    } else {
        return right;
    }
}

std::string SemanticAnalyzer::nodeTypeToString(NodeType t) {
    switch (t) {
        case NodeType::NUMBER: return "'int'";
        case NodeType::NUMBER_BYTE: return "'byte'";
        case NodeType::NUMBER_SMALL: return "'small'";
        case NodeType::NUMBER_LONG: return "'long'";
        case NodeType::NUMBER_DOUBLE: return "'double'";
        case NodeType::STRING: return "'string'";
        case NodeType::NUMBER_FLOAT : return "'float'";
        case NodeType::NULO : return "'nulo'";
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
            visitProgram(node);
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
            visitFunctionCall(node);
            for (const auto& child : node->children) {
                inferType(child.get());
            }
            break;
        case NodeType::RETURN_STATEMENT: {
            visitReturn(node);
            break;
        }
        case NodeType::IF_STATEMENT: {
            visitIf(node);
            break;
        }
        case NodeType::WHILE_STATEMENT: {
            visitWhile(node);
            break;
        }
        case NodeType::CONTINUE_STATEMENT: {
            if (loopDepth == 0) {
                expect("Compile Error: '" + node->value + "' statement can only be used inside a loop", node->line, node->column);
            }
            break;
        }
        case NodeType::BREAK_STATEMENT: {
            if (loopDepth == 0) {
                expect("Compile Error: '" + node->value + "' statement can only be used inside a loop", node->line, node->column);
            }
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
    bool valueIsNullable = isExprNullable(valNode);


    if (varType == "byte") typeNode->type = NodeType::NUMBER_BYTE;
    else if (varType == "small") typeNode->type = NodeType::NUMBER_SMALL;
    else if (varType == "int") typeNode->type = NodeType::NUMBER;
    else if (varType == "long") typeNode->type = NodeType::NUMBER_LONG;
    else if (varType == "float") typeNode->type = NodeType::NUMBER_FLOAT;
    else if (varType == "double") typeNode->type = NodeType::NUMBER_DOUBLE;



    if (!isCompatible(varType, valueType)) {
        bool isLiteral = (valNode->type == NodeType::NUMBER || valNode->type == NodeType::NUMBER_BYTE ||
                          valNode->type == NodeType::NUMBER_SMALL || valNode->type == NodeType::NUMBER_LONG ||
                          valNode->type == NodeType::NUMBER_FLOAT ||
                          valNode->type == NodeType::NUMBER_DOUBLE);

        if (isLiteral && literalFitsInType(valNode->value, varType)) {
        } else if (isNarrowingConversion(varType, valueType)) {
            warn("Possible loss of precision: implicit narrowing from " + nodeTypeToString(valueType) + " to " + varTypeFormatted + " for variable '" +
    varName + "'", valNode->line, valNode->column);
        } else {
            expect("Compile Error: type mismatch; cannot assign " + nodeTypeToString(valueType) + " to variable '" + varName + "' of type " +
    varTypeFormatted, valNode->line, valNode->column);
        }
    }

    SymbolInfo info;
    info.type = varType;
    info.isConst = idNode->isConst;
    info.isSticky = idNode->isSticky;
    info.stickyUsed = idNode->stickyUsed;
    info.isNullable = idNode->isNullable;

    if (!info.isNullable && valueIsNullable) {
        expect("Compile Error: cannot assign nullable value to non-nullable variable '" + varName + "' of type " + varTypeFormatted, valNode->line, valNode->column);
    }

    if (!symbolTable.declare(varName, info)) {
        expect("Compile Error: variable '" + varName + "' is already declared in this scope", idNode->line, idNode->column);
    }
}


NodeType SemanticAnalyzer::inferType(const ASTNode* node) {
    if (!node) return NodeType::BOND;

    switch (node->type) {
        case NodeType::FUNCTION_CALL: {
            visitFunctionCall(node);

            static const std::unordered_map<std::string_view, NodeType> builtins = {
                {"shin", NodeType::STRING},{"toInt", NodeType::NUMBER}, {"random", NodeType::NUMBER},
                {"toDouble", NodeType::NUMBER_DOUBLE},
                {"toFloat", NodeType::NUMBER_FLOAT}
            };

            if (auto it = builtins.find(node->value); it != builtins.end()) {
                return it->second;
            }

            if (SymbolInfo* info = symbolTable.lookup(node->value)) {
                return mapStringToNodeType(info->type);
            }

            expect("Compile Error: undefined function '" + node->value + "'", node->line, node->column);
            return NodeType::BOND;
        }

        case NodeType::BINARY_OPERATION: {

            const ASTNode* leftNode = node->children[0].get();
            const ASTNode* rightNode = node->children[1].get();


            NodeType leftType = inferType(leftNode);
            NodeType rightType = inferType(rightNode);

            if (node->value != "==" && node->value != "!=" && node->value != "sino") {
                if (isExprNullable(leftNode) || isExprNullable(rightNode)) {
                    expect("Compile Error: arithmetic/logical operator '" + node->value + "' cannot be applied to non-nulo types", node->line, node->column);
                }
            }

            if (leftType == NodeType::STRING || rightType == NodeType::STRING) {
                expect("Compile Error: Operator '" + node->value + "' is not supported for type 'string'", node->line, node->column);
            }

            if (node->value == "/") {
                checkDivisionByZero(node->children[1].get(), node->line, node->column);
            }

            if (node->value == "sino") {
                NodeType leftType = inferType(node->children[0].get());
                NodeType rightType = inferType(node->children[1].get());

                if (leftType != rightType) {
                    expect("Compile Error: type mismatch in 'sino' operator; left and right operands must have compatible base types", node->line, node->column);
                }
                return leftType;
            }

            if (node->value == "==" || node->value == ">" || node->value == "<" || node->value == ">=" || node->value == "<=" || node->value == "!=") return NodeType::BOOLEAN;

            // Use type promotion based on priority
            return promoteTypes(leftType, rightType);
        }

        case NodeType::IDENTIFIER: {
            if (SymbolInfo* info = symbolTable.lookup(node->value)) {
                return mapStringToNodeType(info->type);
            }
            expect("Compile Error: variable '" + node->value + "' is not declared in this scope", node->line, node->column);
            return NodeType::BOND;
        }

        default:
            return node->type;
    }
}


bool SemanticAnalyzer::isCompatible(const std::string& declaredType, NodeType valueType) {

    if (declaredType == "byte" && (valueType == NodeType::NUMBER_BYTE || valueType == NodeType::NULO)) {
        return true;
    }

    if (declaredType == "small" && (valueType == NodeType::NUMBER_SMALL || valueType == NodeType::NULO)) {
        return true;
    }

    if (declaredType == "int" && (valueType == NodeType::NUMBER || valueType == NodeType::NULO)) {
        return true;
    }

    if (declaredType == "long" && (valueType == NodeType::NUMBER_LONG || valueType == NodeType::NULO)) {
        return true;
    }

    if (declaredType == "float" && (valueType == NodeType::NUMBER_FLOAT || valueType == NodeType::NULO)) {
        return true;
    }

    if (declaredType == "double" && (valueType == NodeType::NUMBER_DOUBLE || valueType == NodeType::NULO)) {
        return true;
    }

    if (declaredType == "string" && (valueType == NodeType::STRING || valueType == NodeType::NULO)) {
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

void SemanticAnalyzer::warn(std::string msg, int line, int column) {
    std::stringstream stream;
    stream << "Warning: " << msg;
    if (line > 0) {
        stream << " at " << line << ":" << column;
    }
    stream << std::endl;
    std::cerr << stream.str();
}

bool SemanticAnalyzer::isNarrowingConversion(const std::string& declaredType, NodeType valueType) {
    NodeType declaredNodeType;
    if (declaredType == "byte") declaredNodeType = NodeType::NUMBER_BYTE;
    else if (declaredType == "small") declaredNodeType = NodeType::NUMBER_SMALL;
    else if (declaredType == "int") declaredNodeType = NodeType::NUMBER;
    else if (declaredType == "long") declaredNodeType = NodeType::NUMBER_LONG;
    else if (declaredType == "float") declaredNodeType = NodeType::NUMBER_FLOAT;
    else if (declaredType == "double") declaredNodeType = NodeType::NUMBER_DOUBLE;
    else return false;
    int declaredPri = getTypePriority(declaredNodeType);
    int valuePri = getTypePriority(valueType);

    return (declaredPri >= 0 && valuePri >= 0 && valuePri > declaredPri);
}

bool SemanticAnalyzer::literalFitsInType(const std::string& literalValue, const std::string& targetType) {
    if (targetType == "byte") {
        try {
            long long val = std::stoll(literalValue);
            return val >= -128 && val <= 127;
        } catch (...) {
            return false;
        }
    }
    if (targetType == "small") {
        try {
            long long val = std::stoll(literalValue);
            return val >= -32768 && val <= 32767;
        } catch (...) {
            return false;
        }
    }
    if (targetType == "int") {
        try {
            long long val = std::stoll(literalValue);
            return val >= -2147483648LL && val <= 2147483647LL;
        } catch (...) {
            return false;
        }
    }
    // long, float, double — always accept
    return true;
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
    bool valueIsNullable = isExprNullable(valNode);


    SymbolInfo* info = symbolTable.lookup(varName);
    if (!info) {
        expect("Compile Error: variable '" + varName + "' is not declared in this scope", idNode->line, idNode->column);
    }

    if (info->isConst) {
        expect("Compile Error: cannot assign to variable '" + varName + "' because it is a constant", idNode->line, idNode->column);
    }

    if (info->isSticky) {

        if (isWhile) {
            expect("Compile Error: cannot use 'sticky' variable in while statement", idNode->line, idNode->column);
        }

        if (info->stickyUsed) {
            expect("Compile Error: variable '" + varName + "' is 'sticky' and has already been reassigned once", idNode->line, idNode->column);
        } else {
            info->stickyUsed = true;
        }
    }

    if (!isCompatible(info->type, valueType)) {
        bool isLiteral = (valNode->type == NodeType::NUMBER || valNode->type == NodeType::NUMBER_BYTE ||
                          valNode->type == NodeType::NUMBER_SMALL || valNode->type == NodeType::NUMBER_LONG ||
                          valNode->type == NodeType::NUMBER_FLOAT ||
                          valNode->type == NodeType::NUMBER_DOUBLE);

        if (isLiteral && literalFitsInType(valNode->value, info->type)) {
        } else if (isNarrowingConversion(info->type, valueType)) {
            warn("Possible loss of precision: implicit narrowing from " + nodeTypeToString(valueType) + " to '" + info->type + "' for variable '" +
            varName + "'", valNode->line, valNode->column);
        }  else {
            expect("Compile Error: type mismatch; cannot assign " + nodeTypeToString(valueType) + " to variable '" + varName + "' of type '" + info->type
    + "'", valNode->line, valNode->column);
        }
    }


    if (!info->isNullable && valueIsNullable) {
        // Check if the variable was originally nullable (before type narrowing)
        SymbolInfo* originalInfo = symbolTable.lookupOriginal(varName);
        if (originalInfo && originalInfo->isNullable) {
            // The variable was narrowed from nullable; assigning nulo restores nullable state
            info->isNullable = true;
        } else {
            expect("Compile Error: cannot assign nulo to non-nulo variable '" + varName + "'", valNode->line, valNode->column);
        }
    }
}

void SemanticAnalyzer::visitFunctionCall(const ASTNode* node) {
    std::string funcName = node->value;


    if (funcName == "random") {
        if (node->children.size() != 2) expect("Compile Error: function '" + funcName + "' expects 2 arguments, but " + std::to_string(node->children.size()) + " were provided", node->line, node->column);
    }



    if (funcName == "shout" || funcName == "shin"  || funcName == "random" || funcName == "toInt" || funcName == "toFloat" || funcName == "toDouble" ) return;

    SymbolInfo* info = symbolTable.lookup(funcName);
    if (!info) expect("Compile Error: function '" + funcName + "' is not declared", node->line, node->column);


    if (node->children.size() != info->args.size()) {
        expect("Compile Error: function '" + funcName + "' expects " + std::to_string(info->args.size()) +
               " arguments, but " + std::to_string(node->children.size()) + " were provided", node->line, node->column);
        return;
    }

    for (size_t i = 0; i < node->children.size(); ++i) {
        const auto* argNode = node->children[i].get();
        NodeType inferredType = inferType(argNode);
        bool argIsNullable = isExprNullable(argNode);
        const std::string& expectedType = info->args[i];
        bool expectedNullable = (i < info->argsNullable.size()) ? info->argsNullable[i] : false;


        bool expectedIsInt = (expectedType == "byte" || expectedType == "small" || expectedType == "int" || expectedType == "long");
        bool inferredIsInt = (inferredType == NodeType::NUMBER_BYTE || inferredType == NodeType::NUMBER_SMALL ||
                              inferredType == NodeType::NUMBER || inferredType == NodeType::NUMBER_LONG);

        if (!expectedIsInt || !inferredIsInt) {
            if (!isCompatible(expectedType, inferredType)) {
                expect("Compile Error: argument " + std::to_string(i + 1) + " of function '" + funcName +
                       "' is of type " + nodeTypeToString(inferredType) + ", but expected type is '" + expectedType + "'",
                       argNode->line, argNode->column);
            }
        } else if (expectedIsInt && inferredIsInt) {
            bool isLiteral = (argNode->type == NodeType::NUMBER || argNode->type == NodeType::NUMBER_BYTE ||
                              argNode->type == NodeType::NUMBER_SMALL || argNode->type == NodeType::NUMBER_LONG);
            if (isLiteral) {
                if (!literalFitsInType(argNode->value, expectedType)) {
                    expect("Compile Error: literal value '" + argNode->value +
                           "' does not fit in type '" + expectedType + "'",
                           argNode->line, argNode->column);
                }
            } else {
                NodeType expectedNodeType;
                if (expectedType == "byte") expectedNodeType = NodeType::NUMBER_BYTE;
                else if (expectedType == "small") expectedNodeType = NodeType::NUMBER_SMALL;
                else if (expectedType == "int") expectedNodeType = NodeType::NUMBER;
                else expectedNodeType = NodeType::NUMBER_LONG;

                if (getTypePriority(inferredType) > getTypePriority(expectedNodeType)) {
                    expect("Compile Error: argument " + std::to_string(i + 1) + " of function '" + funcName +
                           "' is of type " + nodeTypeToString(inferredType) + ", but expected type is '" + expectedType +
                           "' (narrowing conversion not allowed for non-literals)",
                           argNode->line, argNode->column);
                }
            }
        }

        if (!expectedNullable && argIsNullable) {
            expect("Compile Error: cannot pass nullable value to non-nullable parameter " + std::to_string(i + 1) +
                   " of function '" + funcName + "'", argNode->line, argNode->column);
        }
    }
}

void SemanticAnalyzer::visitFunctionDeclaration(const ASTNode* node) {
    const std::string& funcName = node->value;
    std::string returnType = node->children[0]->value;

    bool returnIsNullable = false;
    if (!returnType.empty() && returnType.back() == '?') {
        returnIsNullable = true;
        returnType.pop_back();
    }

    currentFunctionName = funcName;
    currentFunctionReturnType = returnType;
    currentFunctionIsNullable = returnIsNullable;

    SymbolInfo info;
    info.type = returnType;
    info.isNullable = returnIsNullable;

    const ASTNode* paramsNode = (node->children.size() > 2) ? node->children[1].get() : nullptr;

    if (paramsNode) {
        for (const auto& paramNode : paramsNode->children) {
            std::string pType = paramNode->children[1]->value;
            bool pNullable = false;
            if (!pType.empty() && pType.back() == '?') {
                pNullable = true;
                pType.pop_back();
            }
            info.args.push_back(pType);
            info.argsNullable.push_back(pNullable);
        }
    }

    symbolTable.declare(funcName, info);

    const ASTNode* bodyNode = node->children.back().get();
    bool isMain = (funcName == "main" || funcName == "Main");
    bool hasReturnAtEnd = hasEndingReturn(bodyNode);

    if (isMain) {
        if (returnType != "int" || returnIsNullable) {
            expect("Compile Error: 'main' function must return type 'int' (non-nullable)", node->line, node->column);
        }
        if (!hasReturnAtEnd) {
            expect("Compile Error: Function 'main' must end with a return statement", node->line, node->column);
        }
    } else if (returnType != "void" && !hasReturnAtEnd) {
        expect("Compile Error: Function '" + funcName + "' with return type '" + returnType + "' must return a value", node->line, node->column);
    }

    symbolTable.enterScope();

    if (paramsNode) {
        for (size_t i = 0; i < paramsNode->children.size(); ++i) {
            const auto& param = paramsNode->children[i];
            const std::string& paramName = param->children[0]->value;
            std::string paramType = param->children[1]->value;
            bool isParamNullable = info.argsNullable[i];

            if (!paramType.empty() && paramType.back() == '?') {
                paramType.pop_back();
            }

            SymbolInfo paramInfo;
            paramInfo.type = paramType;
            paramInfo.isNullable = isParamNullable;

            if (!symbolTable.declare(paramName, paramInfo)) {
                expect("Compile Error: redeclaration of parameter '" + paramName + "'", param->line, param->column);
            }
        }
    }

    visit(bodyNode);

    symbolTable.exitScope();
}






