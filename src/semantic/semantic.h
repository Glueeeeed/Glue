#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <string>
#include <unordered_map>
#include "scope.h"
#include "../AST/ast.h"

class SemanticAnalyzer {
    Scope symbolTable;
    std::unordered_map<std::string, SymbolInfo> symbols;
    std::string currentFunctionReturnType;
    std::string currentFunctionName;

public:
    void analyse(const ASTNode* node);
    void visit(const ASTNode* node);
    void visitDeclaration(const ASTNode* node);
    void visitAssignment(const ASTNode* node);
    void visitFunctionDeclaration(const ASTNode* node);
    NodeType inferType(const ASTNode* node);
    bool isCompatible(const std::string& declaredType, NodeType valueType);
    void declareSymbol(const ASTNode* node, bool isConst, bool stickyUsed, bool isSticky);
    static void expect(std::string msg, int line = 0, int column = 0);
};

#endif // SEMANTIC_H