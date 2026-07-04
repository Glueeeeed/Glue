#ifndef PARSER_H
#define PARSER_H
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include "../lexer/lexer.h"
#include "../AST/ast.h"
#include "../codegen/codegen.h"
#include "../semantic/semantic.h"

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

class Parser {
    private:
        std::vector<Token> tokens;
        int tokenPos = 0;
        AST ast;
        SemanticAnalyzer semantic;
        CodeGenerator codegen;
    public:
       Parser(const std::vector<Token> & tokens_,llvm::LLVMContext &context ) : codegen(context) {
       this->tokens = tokens_;
    }

        Token currentToken() const;
        Token peekToken() const;
        Token nextToken();
        void  parseAssign();
        void  parseDeclaration();
        void  parseArgument();
        void  parseExpression();
        void  parseLiteral();
        void  parseFunctionCall();
        void parse();
        static void expect(std::string msg,  int line = 0, int column = 0);
        void printASTCall();


};

#endif