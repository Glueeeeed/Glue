#include "codegen.h"

#include <sstream>
#include "../parser/parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"

void CodeGenerator::generateCode(const ASTNode *node) {
    module = std::make_unique<llvm::Module>("glue_auto", context);
    module->setTargetTriple(llvm::Triple("x86_64-redhat-linux-gnu"));

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
    
    auto *getcharType = llvm::FunctionType::get(builder.getInt32Ty(), false);
    auto getcharFunc = module->getOrInsertFunction("getchar", getcharType);
    builder.CreateCall(getcharFunc);
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
        default:
            break;
    }
}

void CodeGenerator::visitDeclaration(const ASTNode *node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* typeNode = node->children[1].get();
    const ASTNode* valNode  = node->children[2].get();

    std::string varName = idNode->value;
    llvm::Value* val = visitExpression(valNode);

    llvm::Type* ty = nullptr;
    NodeType nt = NodeType::BOND;
    if (typeNode->value == "int") {
        ty = builder.getInt32Ty();
        nt = NodeType::NUMBER;
        if (val->getType()->isFloatingPointTy()) {
            val = builder.CreateFPToSI(val, ty, "castit");
        }
    } else if (typeNode->value == "double") {
        ty = builder.getDoubleTy();
        nt = NodeType::NUMBER_DOUBLE;
        if (!val->getType()->isDoubleTy()) {
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPExt(val, ty, "castd");
            } else {
                val = builder.CreateSIToFP(val, ty, "castd");
            }
        }
    } else if (typeNode->value == "float") {
        ty = builder.getFloatTy();
        nt = NodeType::NUMBER_FLOAT;
        if (!val->getType()->isFloatTy()) {
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPTrunc(val, ty, "castf");
            } else {
                val = builder.CreateSIToFP(val, ty, "castf");
            }
        }
    } else if (typeNode->value == "string") {
        ty = builder.getPtrTy();
        nt = NodeType::STRING;
    } else if (typeNode->value == "bool" || typeNode->value == "boolean") {
        ty = builder.getInt1Ty();
        nt = NodeType::BOOLEAN;
        if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(1)) {
            val = builder.CreateIsNotNull(val, "boolcast");
        }
    } else {
        expect("Codegen Error: unknown type '" + typeNode->value + "'");
        return;
    }

    auto* allocaInst = createEntryAlloca(currentFunction, builder, ty, varName);
    builder.CreateStore(val, allocaInst);
    
    namedValuesStruct& namedValueData = namedValues[varName];
    namedValueData.nodeType = nt;
    namedValueData.pointer = allocaInst;
}

void CodeGenerator::visitAssignment(const ASTNode *node) {
    const ASTNode* idNode   = node->children[0].get();
    const ASTNode* valNode  = node->children[1].get();

    std::string varName = idNode->value;
    if (namedValues.count(varName) == 0) {
        expect("Codegen Error: variable '" + varName + "' not found");
    }

    llvm::Value* val = visitExpression(valNode);
    auto& var = namedValues[varName];

    if (var.nodeType == NodeType::NUMBER) {
        if (val->getType()->isFloatingPointTy()) {
            val = builder.CreateFPToSI(val, builder.getInt32Ty(), "castit");
        }
    } else if (var.nodeType == NodeType::NUMBER_DOUBLE) {
        if (!val->getType()->isDoubleTy()) {
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPExt(val, builder.getDoubleTy(), "castd");
            } else {
                val = builder.CreateSIToFP(val, builder.getDoubleTy(), "castd");
            }
        }
    } else if (var.nodeType == NodeType::NUMBER_FLOAT) {
        if (!val->getType()->isFloatTy()) {
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPTrunc(val, builder.getFloatTy(), "castf");
            } else {
                val = builder.CreateSIToFP(val, builder.getFloatTy(), "castf");
            }
        }
    } else if (var.nodeType == NodeType::BOOLEAN) {
        if (!val->getType()->isIntegerTy(1)) {
            val = builder.CreateIsNotNull(val, "boolcast");
        }
    }

    builder.CreateStore(val, var.pointer);
}

void CodeGenerator::visitFunction(const ASTNode* node) {
    auto printfFunc = module->getOrInsertFunction("printf", llvm::FunctionType::get(builder.getInt32Ty(), { builder.getPtrTy() }, true));
    std::string format;
    std::vector<llvm::Value*> args;

    for (size_t i = 0; i < node->children.size(); ++i) {
        const ASTNode* arg = node->children[i].get();
        if (!arg) continue;

        llvm::Value* val = visitExpression(arg);
        if (!val) continue;

        llvm::Type* type = val->getType();
        if (type->isIntegerTy(1)) {
            format += "%d";
            args.push_back(builder.CreateZExt(val, builder.getInt32Ty()));
        } else if (type->isIntegerTy()) {
            format += "%d";
            args.push_back(val);
        } else if (type->isDoubleTy()) {
            format += "%g";
            args.push_back(val);
        } else if (type->isFloatTy()) {
            format += "%g";
            args.push_back(builder.CreateFPExt(val, builder.getDoubleTy()));
        } else if (type->isPointerTy()) {
            format += "%s";
            args.push_back(val);
        }
    }

    format += "\n";
    llvm::Value* fmt = builder.CreateGlobalString(format);
    args.insert(args.begin(), fmt);
    builder.CreateCall(printfFunc, args);
}

llvm::Value* CodeGenerator::visitExpression(const ASTNode *node) {
    if (!node) return nullptr;

    switch (node->type) {
        case NodeType::NUMBER:
            return builder.getInt32(std::stoi(node->value));
        case NodeType::NUMBER_DOUBLE:
            return llvm::ConstantFP::get(context, llvm::APFloat(std::stod(node->value)));
        case NodeType::NUMBER_FLOAT:
            return llvm::ConstantFP::get(context, llvm::APFloat(std::stof(node->value)));
        case NodeType::STRING:
            return createHeapString(node->value);
        case NodeType::IDENTIFIER: {
            if (namedValues.count(node->value)) {
                auto var = namedValues[node->value];
                llvm::Type* ty;
                if (var.nodeType == NodeType::NUMBER) {
                    ty = builder.getInt32Ty();
                } else if (var.nodeType == NodeType::NUMBER_DOUBLE) {
                    ty = builder.getDoubleTy();
                } else if (var.nodeType == NodeType::NUMBER_FLOAT) {
                    ty = builder.getFloatTy();
                } else if (var.nodeType == NodeType::BOOLEAN) {
                    ty = builder.getInt1Ty();
                } else {
                    ty = builder.getPtrTy();
                }
                return builder.CreateLoad(ty, var.pointer, node->value);
            }
            expect("Codegen Error: variable '" + node->value + "' not found");
        }
        case NodeType::BINARY_OPERATION: {
            llvm::Value* L = visitExpression(node->children[0].get());
            llvm::Value* R = visitExpression(node->children[1].get());
            if (!L || !R) return nullptr;

            bool isLFP = L->getType()->isFloatingPointTy();
            bool isRFP = R->getType()->isFloatingPointTy();

            if (isLFP && !isRFP) {
                R = builder.CreateSIToFP(R, L->getType(), "promoter");
            } else if (!isLFP && isRFP) {
                L = builder.CreateSIToFP(L, R->getType(), "promotel");
            } else if (isLFP && isRFP) {
                if (L->getType()->isDoubleTy() && R->getType()->isFloatTy()) {
                    R = builder.CreateFPExt(R, builder.getDoubleTy(), "promoter");
                } else if (L->getType()->isFloatTy() && R->getType()->isDoubleTy()) {
                    L = builder.CreateFPExt(L, builder.getDoubleTy(), "promotel");
                }
            }

            bool isFP = L->getType()->isFloatingPointTy() || R->getType()->isFloatingPointTy();

            if (node->value == "+") {
                return isFP ? builder.CreateFAdd(L, R, "addtmp") : builder.CreateAdd(L, R, "addtmp");
            } else if (node->value == "-") {
                return isFP ? builder.CreateFSub(L, R, "subtmp") : builder.CreateSub(L, R, "subtmp");
            } else if (node->value == "*") {
                return isFP ? builder.CreateFMul(L, R, "multmp") : builder.CreateMul(L, R, "multmp");
            } else if (node->value == "/") {
                return isFP ? builder.CreateFDiv(L, R, "divtmp") : builder.CreateSDiv(L, R, "divtmp");
            } else if (node->value == "==") {
                return isFP ? builder.CreateFCmpOEQ(L, R, "cmptmp") : builder.CreateICmpEQ(L, R, "cmptmp");
            } else if (node->value == "!=") {
                return isFP ? builder.CreateFCmpONE(L, R, "cmptmp") : builder.CreateICmpNE(L, R, "cmptmp");
            } else if (node->value == "<") {
                return isFP ? builder.CreateFCmpOLT(L, R, "cmptmp") : builder.CreateICmpSLT(L, R, "cmptmp");
            } else if (node->value == ">") {
                return isFP ? builder.CreateFCmpOGT(L, R, "cmptmp") : builder.CreateICmpSGT(L, R, "cmptmp");
            } else if (node->value == "<=") {
                return isFP ? builder.CreateFCmpOLE(L, R, "cmptmp") : builder.CreateICmpSLE(L, R, "cmptmp");
            } else if (node->value == ">=") {
                return isFP ? builder.CreateFCmpOGE(L, R, "cmptmp") : builder.CreateICmpSGE(L, R, "cmptmp");
            }
        }
        case NodeType::BOOLEAN:
            return builder.getInt1(node->value == "true");
        default:
            return nullptr;
    }
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
}

void CodeGenerator::expect(std::string msg) {
    std::stringstream stream;
    stream << msg << std::endl;
    throw ParseError(stream.str());
}
