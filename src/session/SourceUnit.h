#include <string>
#include <vector>
#include "lexer/lexer.h"

struct SourceUnit {
    std::string canonicalPath;
    std::vector<Token> tokens;

};