#include <fstream>
#include <iostream>
#include "lexer/lexer.h"
#include "AST/ast.h"
#include "parser/parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
using namespace std;

// ONLY FOR DEBUG

// static const char* tokenTypeName(TokenType t) {
//     switch (t) {
//         case TokenType::KEYWORD: return "KEYWORD";
//         case TokenType::TYPE: return "TYPE";
//         case TokenType::IDENTIFIER: return "IDENTIFIER";
//         case TokenType::DOT: return "DOT";
//         case TokenType::EQUALS: return "EQUALS";
//         case TokenType::LPAREN: return "LPAREN";
//         case TokenType::RPAREN: return "RPAREN";
//         case TokenType::SEMICOLON: return "SEMICOLON";
//         case TokenType::STRING: return "STRING";
//         case TokenType::NUMBER: return "NUMBER";
//         case TokenType::PLUS: return "PLUS";
//         case TokenType::MINUS: return "MINUS";
//         case TokenType::MULTIPLY: return "MULTIPLY";
//         case TokenType::DIVISION: return "DIVISION";
//         case TokenType::NUMBER_DOUBLE: return "NUMBER_DOUBLE";
//         case TokenType::NUMBER_FLOAT: return "NUMBER_FLOAT";
//         case TokenType::COMMA: return "COMMA";
//         case TokenType::LBRACE: return "LBRACE";
//         case TokenType::RBRACE: return "RBRACE";
//         case TokenType::BOOLEAN_TRUE: return "BOOLEAN_TRUE";
//         case TokenType::BOOLEAN_FALSE: return "BOOLEAN_FALSE";
//         case TokenType::GREATER: return "GREATER";
//         case TokenType::LESS: return "LESS";
//         case TokenType::EQUALS_EQUALS: return "EQUALS_EQUALS";
//         case TokenType::NOT_EQUALS: return "NOT_EQUALS";
//         case TokenType::GREATER_EQUALS: return "GREATER_EQUALS";
//         case TokenType::LESS_EQUALS: return "LESS_EQUALS";
//         case TokenType::END_OF_FILE: return "END_OF_FILE";
//         case TokenType::UNKNOWN: return "UNKNOWN";
//     }
//     return "UNKNOWN";
// }

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [FILE]\n";
        return 1;
    }


    auto tokens = lexer(argv[1]);

    // ONLY FOR DEBUG
    // for (const auto &t : tokens) {
    //     std::cout << tokenTypeName(t.type) << " '" << t.value
    //               << "' (" << t.line << ":" << t.column << ")\n";
    // }
    llvm::LLVMContext context;

    Parser parser(tokens, context);
    try {
        parser.parse();
        parser.printASTCall();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }

    return 0;
}
