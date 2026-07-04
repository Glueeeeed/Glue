#ifndef SEMANTIC_H
#define SEMANTIC_H
#include <string>
#include <unordered_map>

#include "../AST/ast.h"

struct SymbolInfo {
    std::string type;
    bool isConst = false;
    bool isSticky = false;
    mutable bool stickyUsed = false;
};

class SemanticAnalyzer {
    std::unordered_map<std::string, SymbolInfo> symbols;
    public:
    void analyse(const ASTNode* node);
    void visit(const ASTNode* node);
    void visitDeclaration(const ASTNode* node);
    void visitAssignment(const ASTNode* node);
    bool isCompatible(const std::string& declaredType, NodeType valueType);
    void declareSymbol(const ASTNode* node, bool isConst, bool stickyUsed, bool isSticky);
    static void expect(std::string msg);



};

#endif