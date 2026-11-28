#include <iostream>
#include "CompilationSession.h"

void CompilationSession::printAST() const {
    for (const auto& unit : units_) {
        if (!unit.ast.has_value()) continue;
        const ASTNode* root = unit.ast->getRoot();
        if (!root) {
            std::cerr << "Warning: empty AST root for " << unit.canonicalPath << "\n";
            continue;
        }
        std::cout << "=== AST: " << unit.canonicalPath << " ===\n";
        unit.ast->printAST(root);
    }
}

void CompilationSession::printAST(const std::string& path) const {
    const SourceUnit* unit = findUnit(path);
    if (!unit) {
        throw std::out_of_range("Source unit is not part of this session: " + normalizePath(path));
    }
    if (!unit->ast.has_value() || !unit->ast->getRoot()) {
        throw std::logic_error("Source unit has not been parsed: " + unit->canonicalPath);
    }

    std::cout << "=== AST: " << unit->canonicalPath << " ===\n";
    unit->ast->printAST(unit->ast->getRoot());
}
