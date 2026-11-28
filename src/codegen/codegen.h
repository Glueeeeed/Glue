#ifndef GLUESCRIPTCOMPILER_CODEGEN_H
#define GLUESCRIPTCOMPILER_CODEGEN_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "../AST/ast.h"
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct namedValuesStruct {
    llvm::AllocaInst* pointer;
    NodeType nodeType;
    bool isNullable = false;
    int initializedCount = 0;
};


struct LoopContext {
    llvm::BasicBlock* continueTarget;
    llvm::BasicBlock* breakTarget;
};

class CodeGenerator {

    std::vector<std::unordered_map<std::string, namedValuesStruct>> namedValuesStack;
    std::vector<LoopContext> loopStack;

    llvm::LLVMContext &context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    llvm::BasicBlock* currentExitBlock;
    llvm::AllocaInst* returnValue = nullptr;
    llvm::Function* printf = nullptr;
    llvm::Function* currentFunction = nullptr;
    void enterScope();
    void exitScope();
    namedValuesStruct* lookupNamedValue(const std::string& name);
    void visit(const ASTNode* node);
    void visitDeclaration(const ASTNode* node);
    void visitAssignment(const ASTNode* node);
    void visitArrayDeclaration(const ASTNode* node);
    llvm::Value* visitArrayAccess(const ASTNode* node);
    void visitIfStatement(const ASTNode* node);
    void visitWhileStatement(const ASTNode* node);
    llvm::Value* visitFunction(const ASTNode* node);
    llvm::Value* visitExpression(const ASTNode* node);

    llvm::FunctionType *buildLLVMFunctionType(const ASTNode *node, std::string &outFuncName);
    void declareFunctions(const ASTNode *root);

    // void newHeapString(std::string str, llvm::Value *type);
    // llvm::Value* createHeapString(std::string str);

    void expect(std::string msg);
    static llvm::AllocaInst* createEntryAlloca(llvm::Function* F, llvm::IRBuilder<> &B, llvm::Type* ty, const std::string &name) {
        llvm::IRBuilder<> tmp(&F->getEntryBlock(), F->getEntryBlock().begin());
        return tmp.CreateAlloca(ty, nullptr, name);
    }


    // utils  
    llvm::Value* toBool(llvm::Value* val);
    llvm::Type* getLLVMType(const std::string& typeName, bool isNullable = false);
    NodeType getNodeType(const std::string& typeName);
    void visitParameters(const ASTNode* paramsNode, llvm::Function *func);
    void generateSrandSeed();
    void emitBoundsCheck(llvm::Value *indexVal, llvm::Type *allocType);
    void emitDivisionByZeroCheck(llvm::Value *divisor);




public:

    explicit CodeGenerator(llvm::LLVMContext &ctx)
    : context(ctx), builder(context) {}
    void generateCode(const ASTNode* node);
    void generateCode(const std::vector<const ASTNode*>& roots);
    void generate(const ASTNode* node);

    void saveIr(const std::string& filename = "gir.ll");


    void compileAndRun(const std::string& outputBin = "glue_program", bool execute = false, std::string optimization = "-O2");
    void runJit();

    void save();
    void run();
};

#endif //GLUESCRIPTCOMPILER_CODEGEN_H