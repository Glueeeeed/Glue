#pragma once
#include <optional>
#include <string>
#include <vector>
#include "../AST/ast.h"
#include "../lexer/lexer.h"

struct SourceUnit {
    std::string canonicalPath;
    std::vector<Token> tokens;
    std::optional<AST> ast;
};