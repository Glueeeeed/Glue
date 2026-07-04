#ifndef GLUESCRIPTCOMPILER_CODEGEN_H
#define GLUESCRIPTCOMPILER_CODEGEN_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "../AST/ast.h"
#include <iostream>
#include <map>
#include <sstream>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

struct namedValuesStruct {
    llvm::AllocaInst* pointer;
    NodeType nodeType;
};

// struct anyVariable {
//     std::string name;
//     NodeType valueType;
//     bool isDeclared = true;
// };

class CodeGenerator {
private:

    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;

    std::unordered_map<std::string, namedValuesStruct> namedValues;
    // std::unordered_map<std::string, anyVariable> anyVariables;
    llvm::Function* printf = nullptr;
    llvm::Function* currentFunction = nullptr;

    void visit(const ASTNode* node);
    void visitDeclaration(const ASTNode* node);
    void visitAssignment(const ASTNode* node);
    void visitFunction(const ASTNode* node);

    llvm::Value* createHeapString(std::string str);
    void newHeapString(std::string str, llvm::Value *type);
    void expect(std::string msg);
    static llvm::AllocaInst* createEntryAlloca(llvm::Function* F, llvm::IRBuilder<> &B, llvm::Type* ty, const std::string &name) {
        llvm::IRBuilder<> tmp(&F->getEntryBlock(), F->getEntryBlock().begin());
        return tmp.CreateAlloca(ty, nullptr, name);
    }

public:
    explicit CodeGenerator(llvm::LLVMContext &context)
    : builder(context) {}
    void generateCode(const ASTNode* node);
    void generate(const ASTNode* node);
    void save();
    void run();
};

#endif //GLUESCRIPTCOMPILER_CODEGEN_H