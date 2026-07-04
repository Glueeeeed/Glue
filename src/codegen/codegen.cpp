#include "codegen.h"

#include <sstream>
#include "../parser/parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

void CodeGenerator::generateCode(const ASTNode *node) {
    module = std::make_unique<llvm::Module>("glue_auto", context);
    module->setTargetTriple(llvm::Triple("x86_64-redhat-linux-gnu"));
    // module->setTargetTriple(llvm::Triple("x86_64-w64-windows-gnu")); // Windows

    auto *mainType = llvm::FunctionType::get(builder.getInt32Ty(), false);
    currentFunction = llvm::Function::Create(
        mainType,
        llvm::Function::ExternalLinkage,
        "main",
        module.get()
    );


    auto *entry = llvm::BasicBlock::Create(context, "entry", currentFunction);
    builder.SetInsertPoint(entry);

    llvm::BasicBlock* ExitBB = llvm::BasicBlock::Create(context, "exit", currentFunction);

    generate(node);

    if (!entry->getTerminator()) {
        builder.CreateBr(ExitBB);
    }

    builder.SetInsertPoint(ExitBB);
    auto exitInfo = module->getOrInsertFunction("printf",llvm::FunctionType::get(builder.getInt32Ty(),{ llvm::PointerType::get(context, 0) },true));
    builder.CreateCall(exitInfo, builder.CreateGlobalString("Program completed successfully. Press Enter to exit."));
    auto *exit = llvm::FunctionType::get(builder.getInt32Ty(), false);
    currentFunction = llvm::Function::Create(
        exit,
        llvm::Function::ExternalLinkage,
        "getchar",
        module.get()
    );
    builder.CreateCall(currentFunction);
    builder.CreateRet(builder.getInt32(0));

    save();
}



void CodeGenerator::generate(const ASTNode *node) {

    if (!node) return;

    switch (node->type) {
        case NodeType::PROGRAM:
            for (const auto& child : node->children) {
                generate(child.get());
            }
            break;
        case NodeType::DECLARATION:
            visitDeclaration(node);
            break;
        case NodeType::ASSIGNMENT:
            visitAssignment(node);
            break;
        case NodeType::FUNCTION_CALL:
            visitFunction(node);
            break;
    }


}


void CodeGenerator::visitDeclaration(const ASTNode *node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* typeNode = node->children[1].get();
    const ASTNode* valNode  = node->children[2].get();

    std::string varName = idNode->value;

    if (typeNode->value == "int") {
        int value = std::stoi(valNode->value);
        auto* allocaInst = createEntryAlloca(currentFunction, builder, builder.getInt32Ty(), varName);
        builder.CreateStore(builder.getInt32(value), allocaInst);
        namedValuesStruct& namedValueData = namedValues[varName];
        namedValueData.nodeType = NodeType::NUMBER;
        namedValueData.pointer = allocaInst;
    } else if (typeNode->value == "double") {
        double value = std::stod(valNode->value);
        auto* allocaInst = createEntryAlloca(currentFunction, builder, builder.getDoubleTy(), varName);
        builder.CreateStore(llvm::ConstantFP::get(context, llvm::APFloat(value)), allocaInst);
        namedValuesStruct& namedValueData = namedValues[varName];
        namedValueData.nodeType = NodeType::NUMBER_DOUBLE;
        namedValueData.pointer = allocaInst;
    } else if (typeNode->value == "float") {
        float value = std::stof(valNode->value);
        auto* allocaInst = createEntryAlloca(currentFunction, builder, builder.getFloatTy(), varName);
        builder.CreateStore(llvm::ConstantFP::get(context, llvm::APFloat(value)), allocaInst);
        namedValuesStruct& namedValueData = namedValues[varName];
        namedValueData.nodeType = NodeType::NUMBER_FLOAT;
        namedValueData.pointer = allocaInst;
    } else if (typeNode->value == "string") {
        std::string value = valNode->value;
        llvm::Value* heapPtr = createHeapString(value);
        auto* allocaInst = builder.CreateAlloca(builder.getPtrTy(), nullptr, varName);
        builder.CreateStore(heapPtr, allocaInst);
        namedValuesStruct& namedValueData = namedValues[varName];
        namedValueData.nodeType = NodeType::STRING;
        namedValueData.pointer = allocaInst;
    }
}

void CodeGenerator::visitAssignment(const ASTNode *node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* valNode  = node->children[1].get();

    std::string varName = idNode->value;
    const namedValuesStruct& values = namedValues[varName];
    NodeType nodeType = values.nodeType;
    llvm::AllocaInst* ptr = values.pointer;
        if (valNode->type == NodeType::NUMBER) {
            int value = std::stoi(valNode->value);
            builder.CreateStore(builder.getInt32(value), ptr);
        } else if (valNode->type  == NodeType::NUMBER_DOUBLE) {
            double value = std::stod(valNode->value);
            builder.CreateStore(llvm::ConstantFP::get(builder.getDoubleTy(), value),ptr);
        } else if (valNode->type == NodeType::NUMBER_FLOAT) {
            float value = std::stof(valNode->value);
            builder.CreateStore(llvm::ConstantFP::get(builder.getFloatTy(), value),ptr);
        } else if (valNode->type == NodeType::STRING) {
            std::string value = valNode->value;
            newHeapString(value, ptr);
        }

}


void CodeGenerator::visitFunction(const ASTNode* node) {

    auto printfFunc = module->getOrInsertFunction("printf",llvm::FunctionType::get(builder.getInt32Ty(),{ llvm::PointerType::get(context, 0) },true));
    std::string format;
    std::vector<llvm::Value*> args;

    size_t argStart = 0;
    for (size_t i = argStart; i < node->children.size(); ++i) {
        const ASTNode* arg = node->children[i].get();
        if (!arg) continue;

        if (arg->type == NodeType::STRING) {
            format += "%s";
            args.push_back(builder.CreateGlobalString(arg->value));
        } else if (arg->type == NodeType::NUMBER) {
            format += "%d";
            args.push_back(builder.getInt32(std::stoi(arg->value)));
        } else if (arg->type == NodeType::NUMBER_DOUBLE || arg->type == NodeType::NUMBER_FLOAT) {
            format += "%g";
        }  else if (arg->type == NodeType::IDENTIFIER) {
            auto it = namedValues.find(arg->value);
            if (it == namedValues.end()) {
                expect("Compilation error: Unknown variable: " + arg->value);
            }
            if (it->second.nodeType == NodeType::NUMBER) {
                llvm::AllocaInst* alloca = it->second.pointer;
                llvm::Value* loaded = builder.CreateLoad(builder.getInt32Ty(), alloca);
                format += "%d";
                args.push_back(loaded);
            } else if (it->second.nodeType == NodeType::NUMBER_DOUBLE) {
                llvm::AllocaInst* alloca = it->second.pointer;
                llvm::Value* loaded = builder.CreateLoad(builder.getDoubleTy(), alloca);
                format += "%g";
                args.push_back(loaded);
            } else if (it->second.nodeType == NodeType::NUMBER_FLOAT) {
                llvm::AllocaInst* alloca = it->second.pointer;
                llvm::Value* loaded = builder.CreateLoad(builder.getFloatTy(), alloca);
                llvm::Value* as_double = builder.CreateFPExt(loaded, builder.getDoubleTy());
                format += "%g";
                args.push_back(as_double);
            } else if (it->second.nodeType == NodeType::STRING) {
                llvm::AllocaInst* alloca = it->second.pointer;
                llvm::Value* loaded = builder.CreateLoad(builder.getPtrTy(), alloca);
                format += "%s";
                args.push_back(loaded);
            }
        } else {
            expect("Compilation error: invalid argument format");
        }
    }

    format += "\n";

    llvm::Value* fmt = builder.CreateGlobalString(format);
    args.insert(args.begin(), fmt);

    builder.CreateCall(printfFunc, args);


}

llvm::Value *CodeGenerator::createHeapString(std::string str) {
    auto len = str.length();
    auto* value = builder.getInt64Ty();
    llvm::Constant* size = llvm::ConstantInt::get(value, len + 1);

    llvm::DataLayout dataLayout = builder.GetInsertBlock()->getModule()->getDataLayout();
    llvm::Type *intPtrTy = builder.getIntPtrTy(dataLayout);

    auto* ptr = builder.CreateMalloc(intPtrTy, value, size, nullptr);
    llvm::Constant* src = builder.CreateGlobalString(str + '\0');
    builder.CreateMemCpy(ptr, std::nullopt, src, std::nullopt, size);
    return ptr;
}

void CodeGenerator::newHeapString(std::string str, llvm::Value *type) {
    llvm::Value* oldPtr = builder.CreateLoad(builder.getPtrTy(), type);
    builder.CreateFree(oldPtr);
    llvm::Value* value = createHeapString(str);
    builder.CreateStore(value, type);
}


void CodeGenerator::save() {
    std::error_code EC;
    llvm::raw_fd_ostream out("temp.ll", EC);
    module->print(out, nullptr);
    run();
}

void CodeGenerator::run() {
    system("clang temp.ll -o temp_exe && ./temp_exe");
    // system("clang --target=x86_64-w64-windows-gnu temp.ll -o temp.exe"); // FOR WINDOWS


}



void CodeGenerator::expect(std::string msg) {
    std::stringstream stream;
    stream << msg << std::endl;
    throw ParseError(stream.str());
}



