#include "codegen.h"

#include <sstream>
#include <fstream>

#include <cstdlib>
#include "../parser/parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"

void CodeGenerator::generateCode(const ASTNode *node) {
    generateCode(std::vector<const ASTNode*>{node});
}

void CodeGenerator::generateCode(const std::vector<const ASTNode*>& roots) {
    module = std::make_unique<llvm::Module>("glue_auto", context);
    module->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));

    namedValuesStack.clear();
    loopStack.clear();
    for (const ASTNode* root : roots) {
        declareFunctions(root);
    }
    for (const ASTNode* root : roots) {
        generate(root);
    }

    save();
}

llvm::FunctionType *CodeGenerator::buildLLVMFunctionType(const ASTNode *node, std::string &outFuncName) {
    outFuncName = (node->value == "Main") ? "main" : node->value;

    std::string returnTypeName = (!node->children.empty() && node->children[0]->type == NodeType::TYPE)
                                     ? node->children[0]->value
                                     : "void";

    bool returnIsNullable = false;
    if (!returnTypeName.empty() && returnTypeName.back() == '?') {
        returnIsNullable = true;
        returnTypeName.pop_back();
    }

    llvm::Type *retType = getLLVMType(returnTypeName, returnIsNullable);

    std::vector<llvm::Type *> paramTypes;
    const ASTNode *paramsNode = (node->children.size() > 2) ? node->children[1].get() : nullptr;
    if (paramsNode) {
        for (const auto &param : paramsNode->children) {
            std::string pType = param->children[1]->value;
            bool pNullable = false;
            if (!pType.empty() && pType.back() == '?') {
                pNullable = true;
                pType.pop_back();
            }
            paramTypes.push_back(getLLVMType(pType, pNullable));
        }
    }

    return llvm::FunctionType::get(retType, paramTypes, false);
}

void CodeGenerator::declareFunctions(const ASTNode *root) {
    if (!root) return;

    for (const auto &child : root->children) {
        if (!child || child->type != NodeType::FUNCTION_DECLARATION) continue;

        std::string funcName;
        llvm::FunctionType *funcType = buildLLVMFunctionType(child.get(), funcName);

        if (!module->getFunction(funcName)) {
            llvm::Function::Create(
                funcType, llvm::Function::ExternalLinkage, funcName, module.get());
        }
    }
}

void CodeGenerator::enterScope() {
    namedValuesStack.emplace_back();
}

void CodeGenerator::exitScope() {
    if (!namedValuesStack.empty()) {
        namedValuesStack.pop_back();
    }
}

namedValuesStruct *CodeGenerator::lookupNamedValue(const std::string &name) {
    for (auto it = namedValuesStack.rbegin(); it != namedValuesStack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &(found->second);
        }
    }
    return nullptr;
}


void CodeGenerator::generate(const ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NodeType::PROGRAM:
        case NodeType::BLOCK:
            enterScope();
            for (const auto &child: node->children) {
                generate(child.get());
            }
            exitScope();
            break;
        case NodeType::FUNCTION_DECLARATION: {
            std::string funcName = node->value;
            funcName = (funcName == "Main") ? "main" : funcName;


            std::string returnTypeName = (!node->children.empty() && node->children[0]->type == NodeType::TYPE)
                                             ? node->children[0]->value
                                             : "void";

            bool returnIsNullable = false;
            if (!returnTypeName.empty() && returnTypeName.back() == '?') {
                returnIsNullable = true;
                returnTypeName.pop_back();
            }

            llvm::Type *retType = getLLVMType(returnTypeName, returnIsNullable);

            std::vector<llvm::Type *> paramTypes;
            const ASTNode *paramsNode = (node->children.size() > 2) ? node->children[1].get() : nullptr;

            if (paramsNode) {
                for (const auto &param: paramsNode->children) {
                    std::string pType = param->children[1]->value;
                    bool pNullable = false;
                    if (!pType.empty() && pType.back() == '?') {
                        pNullable = true;
                        pType.pop_back();
                    }
                    paramTypes.push_back(getLLVMType(pType, pNullable));
                }
            }


            llvm::FunctionType *funcType = buildLLVMFunctionType(node, funcName);

            llvm::Function *func = module->getFunction(funcName);
            if (!func) {
                func = llvm::Function::Create(
                    funcType, llvm::Function::ExternalLinkage, funcName, module.get());
            }

            currentFunction = func;
            llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", func);
            builder.SetInsertPoint(entry);


            // generate random seed for srand

            if (funcName == "main") {
                generateSrandSeed();
            }


            enterScope();


            if (paramsNode) {
                visitParameters(paramsNode, func);
            }

            if (funcName == "main") {
                this->currentExitBlock = llvm::BasicBlock::Create(context, "exit", func);
                this->returnValue = createEntryAlloca(func, builder, builder.getInt32Ty(), "returnValue");
                builder.CreateStore(builder.getInt32(0), this->returnValue);
            }
            llvm::BasicBlock *ExitBB = this->currentExitBlock;

            const ASTNode *bodyNode = node->children.back().get();
            if (bodyNode && bodyNode->type == NodeType::BLOCK) {
                for (const auto &child: bodyNode->children) {
                    generate(child.get());
                }
            }

            exitScope();

            if (funcName == "main") {
                if (auto *existingTerm = builder.GetInsertBlock()->getTerminator()) {
                    existingTerm->eraseFromParent();
                }

                builder.CreateBr(ExitBB);

                builder.SetInsertPoint(ExitBB);

                auto printfType = llvm::FunctionType::get(
                    builder.getInt32Ty(),
                    {builder.getPtrTy()},
                    true
                );
                auto printfFunc = module->getOrInsertFunction("printf", printfType);

                llvm::Value *retValLoaded = builder.CreateLoad(builder.getInt32Ty(), this->returnValue, "retValLoaded");

                auto exitMsgFormat = builder.CreateGlobalString(
                    "\nProgram completed with exit code %d.\n",
                    "fmt_exit_code"
                );

                builder.CreateCall(printfFunc, {exitMsgFormat, retValLoaded});

                auto fflushFunc = module->getOrInsertFunction("fflush", printfType);
                builder.CreateCall(fflushFunc, {llvm::ConstantPointerNull::get(builder.getPtrTy())});

                // auto getcharFunc = module->getOrInsertFunction("getchar", llvm::FunctionType::get(builder.getInt32Ty(), false));
                // builder.CreateCall(getcharFunc);


                llvm::Value *retValFinal = builder.CreateLoad(builder.getInt32Ty(), this->returnValue, "retValFinal");
                builder.CreateRet(retValFinal);
            } else {
                if (!builder.GetInsertBlock()->getTerminator()) {
                    if (currentFunction->getReturnType()->isVoidTy()) {
                        builder.CreateRetVoid();
                    } else if (currentFunction->getReturnType()->isIntegerTy(1)) {
                        builder.CreateRet(builder.getInt1(false));
                    } else if (currentFunction->getReturnType()->isPointerTy()) {
                        builder.CreateRet(llvm::ConstantPointerNull::get(builder.getPtrTy()));
                    } else if (currentFunction->getReturnType()->isDoubleTy()) {
                        builder.CreateRet(llvm::ConstantFP::get(builder.getDoubleTy(), 0.0));
                    } else if (currentFunction->getReturnType()->isFloatTy()) {
                        builder.CreateRet(llvm::ConstantFP::get(builder.getFloatTy(), 0.0f));
                    } else {
                        builder.CreateRet(builder.getInt32(0));
                    }
                }
            }
            break;
        }
        case NodeType::RETURN_STATEMENT:
            if (currentFunction->getName() == "main") {
                if (!node->children.empty()) {
                    const ASTNode *retExpr = node->children[0].get();
                    llvm::Value *retVal = visitExpression(retExpr);
                    if (retVal->getType()->isIntegerTy(1)) {
                        retVal = builder.CreateZExt(retVal, builder.getInt32Ty(), "retbool");
                    } else if (retVal->getType()->isIntegerTy(16)) {
                        retVal = builder.CreateSExt(retVal, builder.getInt32Ty(), "retsmall");
                    } else if (retVal->getType()->isIntegerTy(8)) {
                        retVal = builder.CreateSExt(retVal, builder.getInt32Ty(), "rettiny");
                    } else if (retVal->getType()->isIntegerTy(64)) {
                        retVal = builder.CreateTrunc(retVal, builder.getInt32Ty(), "retlong");
                    } else if (retVal->getType()->isFloatingPointTy()) {
                        retVal = builder.CreateFPToSI(retVal, builder.getInt32Ty(), "retfloat");
                    }
                    builder.CreateStore(retVal, this->returnValue);
                }
                builder.CreateBr(this->currentExitBlock);
            } else if (!node->children.empty()) {
                const ASTNode *retExpr = node->children[0].get();
                llvm::Type *expectedRetTy = currentFunction->getReturnType();

                if (retExpr->type == NodeType::NULO) {
                    builder.CreateRet(llvm::Constant::getNullValue(expectedRetTy));
                } else {
                    llvm::Value *retVal = visitExpression(retExpr);

                    if (expectedRetTy->isIntegerTy() && retVal->getType()->isIntegerTy()) {
                        if (expectedRetTy->getIntegerBitWidth() != retVal->getType()->getIntegerBitWidth()) {
                            if (expectedRetTy->getIntegerBitWidth() > retVal->getType()->getIntegerBitWidth()) {
                                retVal = builder.CreateSExt(retVal, expectedRetTy, "ret_cast");
                            } else {
                                retVal = builder.CreateTrunc(retVal, expectedRetTy, "ret_cast");
                            }
                        }
                    }

                    if (expectedRetTy->isStructTy() && !retVal->getType()->isStructTy()) {
                        llvm::Value *structVal = llvm::UndefValue::get(expectedRetTy);
                        structVal = builder.CreateInsertValue(structVal, builder.getInt1(true), 0);
                        structVal = builder.CreateInsertValue(structVal, retVal, 1);
                        builder.CreateRet(structVal);
                    } else if (!expectedRetTy->isStructTy() && retVal->getType()->isStructTy()) {
                        llvm::Value *unboxed = builder.CreateExtractValue(retVal, 1, "unboxed_ret");
                        builder.CreateRet(unboxed);
                    } else {
                        builder.CreateRet(retVal);
                    }
                }
            } else {
                builder.CreateRetVoid();
            }
            break;
        case NodeType::DECLARATION:
            visitDeclaration(node);
            break;
        case NodeType::ASSIGNMENT:
            visitAssignment(node);
            break;
        case NodeType::ARRAY_DECLARATION:
            visitArrayDeclaration(node);
            break;
        case NodeType::FUNCTION_CALL:
            visitFunction(node);
            break;
        case NodeType::IF_STATEMENT:
            visitIfStatement(node);
            break;
        case NodeType::WHILE_STATEMENT:
            visitWhileStatement(node);
            break;
        case NodeType::BREAK_STATEMENT: {
            if (!loopStack.empty()) {
                builder.CreateBr(loopStack.back().breakTarget);
                llvm::Function *func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock *deadBB = llvm::BasicBlock::Create(context, "after.break", func);
                builder.SetInsertPoint(deadBB);
            }
            break;
        }
        case NodeType::CONTINUE_STATEMENT: {
            if (!loopStack.empty()) {
                builder.CreateBr(loopStack.back().continueTarget);
                llvm::Function *func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock *deadBB = llvm::BasicBlock::Create(context, "after.continue", func);
                builder.SetInsertPoint(deadBB);
            }
            break;
        }
        default:
            break;
    }
}

void CodeGenerator::visitIfStatement(const ASTNode *node) {
    llvm::Value *condVal = visitExpression(node->children[0].get());
    if (!condVal) return;

    if (condVal->getType()->isStructTy()) {
        condVal = builder.CreateExtractValue(condVal, 0, "has_val");
    } else if (condVal->getType()->isFloatingPointTy()) {
        condVal = builder.CreateFCmpONE(condVal, llvm::ConstantFP::get(condVal->getType(), 0.0), "ifcond");
    } else if (condVal->getType()->isIntegerTy() && !condVal->getType()->isIntegerTy(1)) {
        condVal = builder.CreateIsNotNull(condVal, "ifcond");
    } else if (condVal->getType()->isPointerTy()) {
        condVal = builder.CreateIsNotNull(condVal, "ifcond");
    }

    llvm::Function *func = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context, "then", func);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(context, "else");
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "ifcont");

    bool hasElse = node->children.size() > 2;

    builder.CreateCondBr(condVal, thenBB, hasElse ? elseBB : mergeBB);

    builder.SetInsertPoint(thenBB);
    generate(node->children[1].get());
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(mergeBB);
    }

    if (hasElse) {
        func->insert(func->end(), elseBB);
        builder.SetInsertPoint(elseBB);
        generate(node->children[2].get());
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(mergeBB);
        }
    }

    func->insert(func->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);
}


void CodeGenerator::visitWhileStatement(const ASTNode *node) {
    llvm::Function *func = builder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBB = llvm::BasicBlock::Create(context, "while.cond", func);
    llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(context, "while.body");
    llvm::BasicBlock *endBB = llvm::BasicBlock::Create(context, "while.end");

    builder.CreateBr(condBB);

    builder.SetInsertPoint(condBB);
    llvm::Value *condVal = visitExpression(node->children[0].get());
    if (!condVal) return;

    if (condVal->getType()->isStructTy()) {
        condVal = builder.CreateExtractValue(condVal, 0, "has_val");
    } else if (condVal->getType()->isFloatingPointTy()) {
        condVal = builder.CreateFCmpONE(condVal, llvm::ConstantFP::get(condVal->getType(), 0.0), "whilecond");
    } else if (condVal->getType()->isIntegerTy() && !condVal->getType()->isIntegerTy(1)) {
        condVal = builder.CreateIsNotNull(condVal, "whilecond");
    } else if (condVal->getType()->isPointerTy()) {
        condVal = builder.CreateIsNotNull(condVal, "whilecond");
    }

    builder.CreateCondBr(condVal, bodyBB, endBB);

    func->insert(func->end(), bodyBB);
    builder.SetInsertPoint(bodyBB);

    loopStack.push_back({condBB, endBB});
    generate(node->children[1].get());
    loopStack.pop_back();

    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(condBB);
    }

    func->insert(func->end(), endBB);
    builder.SetInsertPoint(endBB);
}

void CodeGenerator::visitArrayDeclaration(const ASTNode *node) {

    const ASTNode *typeNode = node->children[0].get();
    const ASTNode *literals = node->children[1].get();

    std::string arrName = node->value;
    std::string typeName = typeNode->value;
    std::string size = typeNode->children[0]->value;
    bool isNullable = node->isNullable;


    llvm::Type *arrType = getLLVMType(typeName, isNullable);
    int arrSize = std::stoi(size);

    llvm::ArrayType *arrArrayType = llvm::ArrayType::get(arrType, arrSize);
    auto* allocaInst = builder.CreateAlloca(arrArrayType, nullptr, arrName);

    int i = 0;
    for (const auto& literal : literals->children) {
        auto* value = builder.CreateGEP(arrArrayType, allocaInst,{builder.getInt32(0), builder.getInt32(i)},"arr.gep");
        llvm::Value* literalValue = visitExpression(literal.get());
        builder.CreateStore(literalValue, value);
        i++;
    }

    int initCount = i;

    llvm::Value* defaultValue = llvm::Constant::getNullValue(arrType);
    for (; i < arrSize; i++) {
        auto* value = builder.CreateGEP(arrArrayType, allocaInst, {builder.getInt32(0), builder.getInt32(i)}, "arr.gep");
        builder.CreateStore(defaultValue, value);
    }

    if (!namedValuesStack.empty()) {
        namedValuesStack.back()[arrName] = {allocaInst, getNodeType(typeName), isNullable, initCount};
    }


}


void CodeGenerator::visitDeclaration(const ASTNode *node) {
    const ASTNode *idNode = node->children[0].get();
    const ASTNode *typeNode = node->children[1].get();
    const ASTNode *valNode = node->children[2].get();

    std::string varName = idNode->value;
    std::string typeName = typeNode->value;
    bool isNullable = idNode->isNullable;


    llvm::Type *varType = getLLVMType(typeName, isNullable);
    auto *allocaInst = createEntryAlloca(currentFunction, builder, varType, varName);

    if (valNode->type == NodeType::NULO) {
        llvm::Value *nullVal = llvm::Constant::getNullValue(varType);
        builder.CreateStore(nullVal, allocaInst);
    } else if (isNullable && typeName != "string") {
        llvm::Value *rawVal = visitExpression(valNode);

        if (rawVal->getType()->isStructTy()) {
            builder.CreateStore(rawVal, allocaInst);
        } else {
            if (typeName == "int" && rawVal->getType()->isFloatingPointTy()) {
                rawVal = builder.CreateFPToSI(rawVal, builder.getInt32Ty(), "castit");
            } else if (typeName == "small") {
                llvm::Type* smallTy = builder.getInt16Ty();
                if (rawVal->getType()->isFloatingPointTy()) {
                    rawVal = builder.CreateFPToSI(rawVal, smallTy, "castsmall");
                } else if (rawVal->getType()->isIntegerTy(32)) {
                    rawVal = builder.CreateTrunc(rawVal, smallTy, "castsmall");
                }
            } else if (typeName == "byte") {
                llvm::Type* tinyTy = builder.getInt8Ty();
                if (rawVal->getType()->isFloatingPointTy()) {
                    rawVal = builder.CreateFPToSI(rawVal, tinyTy, "casttiny");
                } else if (rawVal->getType()->isIntegerTy(32)) {
                    rawVal = builder.CreateTrunc(rawVal, tinyTy, "casttiny");
                }
            } else if (typeName == "long") {
                llvm::Type* longTy = builder.getInt64Ty();
                if (rawVal->getType()->isFloatingPointTy()) {
                    rawVal = builder.CreateFPToSI(rawVal, longTy, "castlong");
                } else if (rawVal->getType()->isIntegerTy(32)) {
                    rawVal = builder.CreateSExt(rawVal, longTy, "castlong");
                }
            } else if (typeName == "double" && !rawVal->getType()->isDoubleTy()) {
                if (rawVal->getType()->isFloatingPointTy()) {
                    rawVal = builder.CreateFPExt(rawVal, builder.getDoubleTy(), "castd");
                } else {
                    rawVal = builder.CreateSIToFP(rawVal, builder.getDoubleTy(), "castd");
                }
            } else if (typeName == "float" && !rawVal->getType()->isFloatTy()) {
                if (rawVal->getType()->isFloatingPointTy()) {
                    rawVal = builder.CreateFPTrunc(rawVal, builder.getFloatTy(), "castf");
                } else {
                    rawVal = builder.CreateSIToFP(rawVal, builder.getFloatTy(), "castf");
                }
            } else if ((typeName == "bool" || typeName == "boolean") && !rawVal->getType()->isIntegerTy(1)) {
                rawVal = builder.CreateIsNotNull(rawVal, "boolcast");
            }

            llvm::Value *structVal = llvm::UndefValue::get(varType);
            structVal = builder.CreateInsertValue(structVal, builder.getInt1(true), 0);
            structVal = builder.CreateInsertValue(structVal, rawVal, 1);
            builder.CreateStore(structVal, allocaInst);
        }
    } else {
        llvm::Value *rawVal = visitExpression(valNode);
        if (rawVal->getType()->isStructTy()) {
            rawVal = builder.CreateExtractValue(rawVal, 1, "unboxed_val");
        }
        if (typeName == "small") {
            llvm::Type* smallTy = builder.getInt16Ty();
            if (rawVal->getType()->isFloatingPointTy()) {
                rawVal = builder.CreateFPToSI(rawVal, smallTy, "castsmall");
            } else if (rawVal->getType()->isIntegerTy(32)) {
                rawVal = builder.CreateTrunc(rawVal, smallTy, "castsmall");
            }
        } else if (typeName == "byte") {
            llvm::Type* tinyTy = builder.getInt8Ty();
            if (rawVal->getType()->isFloatingPointTy()) {
                rawVal = builder.CreateFPToSI(rawVal, tinyTy, "casttiny");
            } else if (rawVal->getType()->isIntegerTy(32)) {
                rawVal = builder.CreateTrunc(rawVal, tinyTy, "casttiny");
            }
        } else if (typeName == "long") {
            llvm::Type* longTy = builder.getInt64Ty();
            if (rawVal->getType()->isFloatingPointTy()) {
                rawVal = builder.CreateFPToSI(rawVal, longTy, "castlong");
            } else if (rawVal->getType()->isIntegerTy(32)) {
                rawVal = builder.CreateSExt(rawVal, longTy, "castlong");
            }
        }
        builder.CreateStore(rawVal, allocaInst);
    }

    if (!namedValuesStack.empty()) {
        namedValuesStack.back()[varName] = {allocaInst, getNodeType(typeName), isNullable};
    }
}

void CodeGenerator::visitAssignment(const ASTNode *node) {
    const ASTNode *idNode = node->children[0].get();
    const ASTNode *valNode = node->children[1].get();

    if (idNode->type == NodeType::ARRAY_ACCESS) {
        const ASTNode *arrayNode = idNode->children[0].get();
        const ASTNode *indexNode = idNode->children[1].get();

        std::string arrayName = arrayNode->value;
        namedValuesStruct *var = lookupNamedValue(arrayName);
        if (!var) {
            expect("Compile Error: array '" + arrayName + "' not found");
            return;
        }

        llvm::Value *indexVal = visitExpression(indexNode);
        if (!indexVal) return;

        if (indexVal->getType()->isIntegerTy() && !indexVal->getType()->isIntegerTy(32)) {
            if (indexVal->getType()->getIntegerBitWidth() < 32) {
                indexVal = builder.CreateSExt(indexVal, builder.getInt32Ty(), "arr.idx.ext");
            } else {
                indexVal = builder.CreateTrunc(indexVal, builder.getInt32Ty(), "arr.idx.trunc");
            }
        }

        llvm::Type *allocType = var->pointer->getAllocatedType();

        emitBoundsCheck(indexVal, allocType);

        llvm::Value *gepPtr = builder.CreateGEP(allocType, var->pointer, {builder.getInt32(0), indexVal}, "arr.gep");

        llvm::Value *val = visitExpression(valNode);
        if (!val) return;

        if (val->getType()->isStructTy()) {
            val = builder.CreateExtractValue(val, 1, "unboxed_val");
        }

        builder.CreateStore(val, gepPtr);
        return;
    }

    std::string varName = idNode->value;
    namedValuesStruct *var = lookupNamedValue(varName);
    if (!var) {
        expect("Compile Error: variable '" + varName + "' not found");
        return;
    }

    if (var->isNullable && var->nodeType != NodeType::STRING) {
        llvm::Type *varType = var->pointer->getAllocatedType();

        if (valNode->type == NodeType::NULO) {
            llvm::Value *nullVal = llvm::Constant::getNullValue(varType);
            builder.CreateStore(nullVal, var->pointer);
            return;
        }

        llvm::Value *val = visitExpression(valNode);

        if (val->getType()->isStructTy()) {
            builder.CreateStore(val, var->pointer);
            return;
        }

        if (var->nodeType == NodeType::NUMBER_BYTE) {
            llvm::Type* tinyTy = builder.getInt8Ty();
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPToSI(val, tinyTy, "casttiny");
            } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(8)) {
                val = builder.CreateTrunc(val, tinyTy, "casttiny");
            }
        } else if (var->nodeType == NodeType::NUMBER_SMALL) {
            llvm::Type* smallTy = builder.getInt16Ty();
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPToSI(val, smallTy, "castsmall");
            } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(16)) {
                if (val->getType()->isIntegerTy(1)) {
                    val = builder.CreateZExt(val, smallTy, "castsmall");
                } else {
                    val = builder.CreateTrunc(val, smallTy, "castsmall");
                }
            }
        } else if (var->nodeType == NodeType::NUMBER) {
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPToSI(val, builder.getInt32Ty(), "castit");
            } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(32)) {
                if (val->getType()->isIntegerTy(1)) {
                    val = builder.CreateZExt(val, builder.getInt32Ty(), "castit");
                } else if (val->getType()->getIntegerBitWidth() < 32) {
                    val = builder.CreateSExt(val, builder.getInt32Ty(), "castit");
                } else {
                    val = builder.CreateTrunc(val, builder.getInt32Ty(), "castit");
                }
            }
        } else if (var->nodeType == NodeType::NUMBER_LONG) {
            llvm::Type* longTy = builder.getInt64Ty();
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPToSI(val, longTy, "castlong");
            } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(64)) {
                val = builder.CreateSExt(val, longTy, "castlong");
            }
        } else if (var->nodeType == NodeType::NUMBER_DOUBLE) {
            if (!val->getType()->isDoubleTy()) {
                if (val->getType()->isFloatingPointTy()) {
                    val = builder.CreateFPExt(val, builder.getDoubleTy(), "castd");
                } else {
                    val = builder.CreateSIToFP(val, builder.getDoubleTy(), "castd");
                }
            }
        } else if (var->nodeType == NodeType::NUMBER_FLOAT) {
            if (!val->getType()->isFloatTy()) {
                if (val->getType()->isFloatingPointTy()) {
                    val = builder.CreateFPTrunc(val, builder.getFloatTy(), "castf");
                } else {
                    val = builder.CreateSIToFP(val, builder.getFloatTy(), "castf");
                }
            }
        } else if (var->nodeType == NodeType::BOOLEAN) {
            if (!val->getType()->isIntegerTy(1)) {
                val = builder.CreateIsNotNull(val, "boolcast");
            }
        }

        llvm::Value *structVal = llvm::UndefValue::get(varType);
        structVal = builder.CreateInsertValue(structVal, builder.getInt1(true), 0);
        structVal = builder.CreateInsertValue(structVal, val, 1);
        builder.CreateStore(structVal, var->pointer);
        return;
    }

    if (valNode->type == NodeType::NULO) {
        llvm::Value *nullVal = llvm::Constant::getNullValue(var->pointer->getAllocatedType());
        builder.CreateStore(nullVal, var->pointer);
        return;
    }

    llvm::Value *val = visitExpression(valNode);

    if (val->getType()->isStructTy()) {
        val = builder.CreateExtractValue(val, 1, "unboxed_val");
    }

    if (var->nodeType == NodeType::NUMBER_BYTE) {
        llvm::Type* tinyTy = builder.getInt8Ty();
        if (val->getType()->isFloatingPointTy()) {
            val = builder.CreateFPToSI(val, tinyTy, "casttiny");
        } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(8)) {
            val = builder.CreateTrunc(val, tinyTy, "casttiny");
        }
    } else if (var->nodeType == NodeType::NUMBER_SMALL) {
        llvm::Type* smallTy = builder.getInt16Ty();
        if (val->getType()->isFloatingPointTy()) {
            val = builder.CreateFPToSI(val, smallTy, "castsmall");
        } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(16)) {
            if (val->getType()->isIntegerTy(1)) {
                val = builder.CreateZExt(val, smallTy, "castsmall");
            } else {
                val = builder.CreateTrunc(val, smallTy, "castsmall");
            }
        }
    } else if (var->nodeType == NodeType::NUMBER) {
        if (val->getType()->isFloatingPointTy()) {
            val = builder.CreateFPToSI(val, builder.getInt32Ty(), "castit");
        } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(32)) {
            if (val->getType()->isIntegerTy(1)) {
                val = builder.CreateZExt(val, builder.getInt32Ty(), "castit");
            } else if (val->getType()->getIntegerBitWidth() < 32) {
                val = builder.CreateSExt(val, builder.getInt32Ty(), "castit");
            } else {
                val = builder.CreateTrunc(val, builder.getInt32Ty(), "castit");
            }
        }
    } else if (var->nodeType == NodeType::NUMBER_LONG) {
        llvm::Type* longTy = builder.getInt64Ty();
        if (val->getType()->isFloatingPointTy()) {
            val = builder.CreateFPToSI(val, longTy, "castlong");
        } else if (val->getType()->isIntegerTy() && !val->getType()->isIntegerTy(64)) {
            val = builder.CreateSExt(val, longTy, "castlong");
        }
    } else if (var->nodeType == NodeType::NUMBER_DOUBLE) {
        if (!val->getType()->isDoubleTy()) {
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPExt(val, builder.getDoubleTy(), "castd");
            } else {
                val = builder.CreateSIToFP(val, builder.getDoubleTy(), "castd");
            }
        }
    } else if (var->nodeType == NodeType::NUMBER_FLOAT) {
        if (!val->getType()->isFloatTy()) {
            if (val->getType()->isFloatingPointTy()) {
                val = builder.CreateFPTrunc(val, builder.getFloatTy(), "castf");
            } else {
                val = builder.CreateSIToFP(val, builder.getFloatTy(), "castf");
            }
        }
    } else if (var->nodeType == NodeType::BOOLEAN) {
        if (!val->getType()->isIntegerTy(1)) {
            val = builder.CreateIsNotNull(val, "boolcast");
        }
    }

    builder.CreateStore(val, var->pointer);
}

llvm::Value *CodeGenerator::visitFunction(const ASTNode *node) {
    std::string functionName = node->value;


    if (functionName == "shin") {
        auto mallocFunc = module->getOrInsertFunction(
            "malloc", llvm::FunctionType::get(builder.getPtrTy(), {builder.getInt64Ty()}, false));
        llvm::Value *buffer = builder.CreateCall(mallocFunc, {builder.getInt64(1024)});

        auto scanfFunc = module->getOrInsertFunction(
            "scanf", llvm::FunctionType::get(builder.getInt32Ty(), {builder.getPtrTy()}, true));
        llvm::Value *fmt = builder.CreateGlobalString("%1023s");
        builder.CreateCall(scanfFunc, {fmt, buffer});

        return buffer;
    }

    if (functionName == "toInt") {
        llvm::Value *strVal = visitExpression(node->children[0].get());
        auto atoiFunc = module->getOrInsertFunction(
            "atoi", llvm::FunctionType::get(builder.getInt32Ty(), {builder.getPtrTy()}, false));
        return builder.CreateCall(atoiFunc, {strVal});
    }

    if (functionName == "toDouble") {
        llvm::Value *strVal = visitExpression(node->children[0].get());
        auto atofFunc = module->getOrInsertFunction(
            "atof", llvm::FunctionType::get(builder.getDoubleTy(), {builder.getPtrTy()}, false));
        return builder.CreateCall(atofFunc, {strVal});
    }

    if (functionName == "toFloat") {
        llvm::Value *strVal = visitExpression(node->children[0].get());
        auto atofFunc = module->getOrInsertFunction(
            "atof", llvm::FunctionType::get(builder.getDoubleTy(), {builder.getPtrTy()}, false));
        llvm::Value *dVal = builder.CreateCall(atofFunc, {strVal});
        return builder.CreateFPTrunc(dVal, builder.getFloatTy());
    }

    if (functionName == "random") {
        llvm::Value *minVal = visitExpression(node->children[0].get());
        llvm::Value *maxVal = visitExpression(node->children[1].get());

        auto randFunc = module->getOrInsertFunction("rand", llvm::FunctionType::get(builder.getInt32Ty(), false));
        llvm::Value *r = builder.CreateCall(randFunc);

        // range = (max - min) + 1
        llvm::Value *diff = builder.CreateSub(maxVal, minVal);
        llvm::Value *range = builder.CreateAdd(diff, builder.getInt32(1));

        // result = min + (rand() % range)
        llvm::Value *mod = builder.CreateSRem(r, range);
        return builder.CreateAdd(minVal, mod);
    }

    if (functionName == "shout") {
        auto printfFunc = module->getOrInsertFunction(
            "printf", llvm::FunctionType::get(builder.getInt32Ty(), {builder.getPtrTy()}, true));
        std::string format;
        std::vector<llvm::Value *> args;

        llvm::Value *fmtNuloShared = builder.CreateGlobalString("nulo");
        llvm::Value *fmtStrShared = builder.CreateGlobalString("%s");

        for (size_t i = 0; i < node->children.size(); ++i) {
            const ASTNode *arg = node->children[i].get();
            if (!arg) continue;

            if (arg->type == NodeType::IDENTIFIER) {
                namedValuesStruct *var = lookupNamedValue(arg->value);
                if (var) {
                    llvm::Type *allocType = var->pointer->getAllocatedType();
                    if (allocType->isArrayTy()) {
                        llvm::Value *fmtBraceOpen = builder.CreateGlobalString("{");
                        builder.CreateCall(printfFunc, {fmtBraceOpen});

                        llvm::ArrayType *arrayType = llvm::cast<llvm::ArrayType>(allocType);
                        llvm::Type *elementType = arrayType->getElementType();
                        uint64_t count = arrayType->getNumElements();

                        for (uint64_t j = 0; j < count; ++j) {


                            llvm::Value *gepPtr = builder.CreateGEP(allocType, var->pointer,
                                {builder.getInt32(0), builder.getInt32(j)}, "shout.arr.gep");
                            llvm::Value *elemVal = builder.CreateLoad(elementType, gepPtr, "shout.arr.elem");


                            if (elementType->isIntegerTy(1)) {
                                llvm::Value *extVal = builder.CreateZExt(elemVal, builder.getInt32Ty(), "shout_bool");
                                llvm::Value *fmtBool = builder.CreateGlobalString("%d");
                                builder.CreateCall(printfFunc, {fmtBool, extVal});
                            } else if (elementType->isIntegerTy(64)) {
                                llvm::Value *fmtLong = builder.CreateGlobalString("%lld");
                                builder.CreateCall(printfFunc, {fmtLong, elemVal});
                            } else if (elementType->isIntegerTy()) {
                                llvm::Value *printVal = elemVal;
                                if (elementType->isIntegerTy(8) || elementType->isIntegerTy(16)) {
                                    printVal = builder.CreateSExt(elemVal, builder.getInt32Ty(), "shout_ext");
                                }
                                llvm::Value *fmtInt = builder.CreateGlobalString("%d");
                                builder.CreateCall(printfFunc, {fmtInt, printVal});
                            } else if (elementType->isDoubleTy()) {
                                llvm::Value *fmtDbl = builder.CreateGlobalString("%.17g");
                                builder.CreateCall(printfFunc, {fmtDbl, elemVal});
                            } else if (elementType->isFloatTy()) {
                                llvm::Value *extF = builder.CreateFPExt(elemVal, builder.getDoubleTy(), "shout_float");
                                llvm::Value *fmtFlt = builder.CreateGlobalString("%.7g");
                                builder.CreateCall(printfFunc, {fmtFlt, extF});
                            } else if (elementType->isPointerTy()) {
                                llvm::Value *fmtStr = builder.CreateGlobalString("%s");
                                builder.CreateCall(printfFunc, {fmtStr, elemVal});
                            }

                            if (j < count - 1) {
                                llvm::Value *fmtComma = builder.CreateGlobalString(", ");
                                builder.CreateCall(printfFunc, {fmtComma});
                            }
                        }
                        llvm::Value *fmtBraceClose = builder.CreateGlobalString("}\n");
                        builder.CreateCall(printfFunc, {fmtBraceClose});
                        continue;
                    }
                }
            }

            llvm::Value *val = visitExpression(arg);
            if (!val) continue;

            llvm::Type *type = val->getType();

            if (type->isStructTy()) {
                llvm::Value *hasVal = builder.CreateExtractValue(val, 0, "has_val");
                llvm::Value *innerVal = builder.CreateExtractValue(val, 1, "inner_val");

                llvm::Function *func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock *valBB = llvm::BasicBlock::Create(context, "shout.val", func);
                llvm::BasicBlock *nuloBB = llvm::BasicBlock::Create(context, "shout.nulo", func);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "shout.merge", func);

                builder.CreateCondBr(hasVal, valBB, nuloBB);

                builder.SetInsertPoint(valBB);
                std::string innerFmt;
                llvm::Value *printVal = innerVal;

                if (innerVal->getType()->isFloatTy()) {
                    innerFmt = "%.7g";
                    printVal = builder.CreateFPExt(innerVal, builder.getDoubleTy(), "float_promoted");
                } else if (innerVal->getType()->isDoubleTy()) {
                    innerFmt = "%.17g";
                } else if (innerVal->getType()->isIntegerTy(1)) {
                    innerFmt = "%d";
                    printVal = builder.CreateZExt(innerVal, builder.getInt32Ty(), "bool_promoted");
                } else if (innerVal->getType()->isIntegerTy(64)) {
                    innerFmt = "%lld";
                } else if (innerVal->getType()->isIntegerTy()) {
                    innerFmt = "%d";
                    if (innerVal->getType()->isIntegerTy(8) || innerVal->getType()->isIntegerTy(16)) {
                        printVal = builder.CreateSExt(innerVal, builder.getInt32Ty(), "shout_ext");
                    }
                } else if (innerVal->getType()->isPointerTy()) {
                    innerFmt = "%s\n";
                }

                llvm::Value *fmtVal = builder.CreateGlobalString(innerFmt);
                builder.CreateCall(printfFunc, {fmtVal, printVal});
                builder.CreateBr(mergeBB);

                builder.SetInsertPoint(nuloBB);
                builder.CreateCall(printfFunc, {fmtNuloShared});
                builder.CreateBr(mergeBB);

                builder.SetInsertPoint(mergeBB);
                continue;
            }

            if (type->isPointerTy()) {
                llvm::Value *isNull = builder.CreateIsNull(val, "is_null");

                llvm::Function *func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock *strBB = llvm::BasicBlock::Create(context, "shout.str", func);
                llvm::BasicBlock *nuloBB = llvm::BasicBlock::Create(context, "shout.nulo", func);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "shout.merge", func);

                builder.CreateCondBr(isNull, nuloBB, strBB);

                builder.SetInsertPoint(strBB);
                builder.CreateCall(printfFunc, {fmtStrShared, val});
                builder.CreateBr(mergeBB);

                builder.SetInsertPoint(nuloBB);
                builder.CreateCall(printfFunc, {fmtNuloShared});
                builder.CreateBr(mergeBB);

                builder.SetInsertPoint(mergeBB);
                continue;
            }

            if (type->isIntegerTy(1)) {
                format += "%d";
                args.push_back(builder.CreateZExt(val, builder.getInt32Ty()));
            } else if (type->isIntegerTy(64)) {
                format += "%lld";
                args.push_back(val);
            } else if (type->isIntegerTy()) {
                format += "%d";
                if (type->isIntegerTy(8) || type->isIntegerTy(16)) {
                    args.push_back(builder.CreateSExt(val, builder.getInt32Ty(), "shout_ext"));
                } else {
                    args.push_back(val);
                }
            } else if (type->isDoubleTy()) {
                format += "%.17g";
                args.push_back(val);
            } else if (type->isFloatTy()) {
                format += "%.7g";
                args.push_back(builder.CreateFPExt(val, builder.getDoubleTy()));
            } else if (type->isPointerTy()) {
                format += "%s";
                args.push_back(val);
            }
        }

        format += "\n";
        llvm::Value *fmt = builder.CreateGlobalString(format);
        args.insert(args.begin(), fmt);
        return builder.CreateCall(printfFunc, args);
    }

    if (functionName == "abortar") {
        const ASTNode *arg = node->children[0].get();


        if (arg->type != NodeType::STRING) {
            expect("Compile Error: argument '" + arg->value + "' must be a string");
        }

        llvm::Value* str = builder.CreateGlobalString(arg->value);
        llvm::FunctionType* abortarType = llvm::FunctionType::get(builder.getVoidTy(), { builder.getPtrTy() }, false);

        auto abortarFunc = module->getOrInsertFunction("glue_abortar", abortarType);

        return builder.CreateCall(abortarFunc, {str});
    }

    llvm::Function *callee = module->getFunction(functionName);
    if (!callee) {
        expect("Compile Error: function '" + functionName + "' not found");
        return nullptr;
    }

    std::vector<llvm::Value *> args;
    llvm::FunctionType *calleeType = callee->getFunctionType();

    for (size_t i = 0; i < node->children.size(); ++i) {
        const ASTNode *argNode = node->children[i].get();
        if (!argNode) continue;

        llvm::Type *expectedParamType = (i < calleeType->getNumParams())
                                            ? calleeType->getParamType(i)
                                            : nullptr;

        llvm::Value *argVal = nullptr;

        if (expectedParamType && expectedParamType->isStructTy()) {
            if (argNode->type == NodeType::NULO) {
                argVal = llvm::Constant::getNullValue(expectedParamType);
            } else {
                argVal = visitExpression(argNode);
                if (argVal && !argVal->getType()->isStructTy()) {
                    llvm::Value *structVal = llvm::UndefValue::get(expectedParamType);
                    structVal = builder.CreateInsertValue(structVal, builder.getInt1(true), 0);
                    structVal = builder.CreateInsertValue(structVal, argVal, 1);
                    argVal = structVal;
                }
            }
        } else {
            argVal = visitExpression(argNode);
            if (argVal && expectedParamType && !expectedParamType->isStructTy() && argVal->getType()->isStructTy()) {
                argVal = builder.CreateExtractValue(argVal, 1, "unboxed_arg");
            }
        }

        if (argVal) {
            args.push_back(argVal);
        }
    }

    return builder.CreateCall(callee, args);
}

llvm::Value *CodeGenerator::visitExpression(const ASTNode *node) {
    if (!node) return nullptr;

    switch (node->type) {
        case NodeType::NUMBER_BYTE:
            return builder.getInt8(std::stoi(node->value));
        case NodeType::NUMBER_SMALL:
            return builder.getInt16(std::stoi(node->value));
        case NodeType::NUMBER: {
            try {
                int32_t val32 = std::stoi(node->value);
                return builder.getInt32(val32);
            } catch (const std::out_of_range&) {
                try {
                    int64_t val64 = std::stoll(node->value);
                    return builder.getInt64(val64);
                } catch (const std::out_of_range&) {
                    expect("Compile Error: number too large, use 'long' type");
                    return builder.getInt32(0);
                }
            }
        }
        case NodeType::NUMBER_LONG:
            return builder.getInt64(std::stoll(node->value));
        case NodeType::NUMBER_DOUBLE:
            return llvm::ConstantFP::get(context, llvm::APFloat(std::stod(node->value)));
        case NodeType::NUMBER_FLOAT:
            return llvm::ConstantFP::get(context, llvm::APFloat(std::stof(node->value)));
        case NodeType::STRING:
            return builder.CreateGlobalString(node->value);
        case NodeType::IDENTIFIER: {
            namedValuesStruct *var = lookupNamedValue(node->value);
            if (var) {
                return builder.CreateLoad(var->pointer->getAllocatedType(), var->pointer, node->value);
            }
            expect("Compile Error: variable '" + node->value + "' not found");
        }
        case NodeType::BINARY_OPERATION: {
            const ASTNode *leftNode = node->children[0].get();
            const ASTNode *rightNode = node->children[1].get();

              if (node->value == "sino" || node->value == "??") {
                const ASTNode *leftNode = node->children[0].get();
                const ASTNode *rightNode = node->children[1].get();

                llvm::Value *leftVal = visitExpression(leftNode);
                if (!leftVal) return nullptr;

                if (!leftVal->getType()->isStructTy() && !leftVal->getType()->isPointerTy()) {
                    return leftVal;
                }

                llvm::Value *hasVal = nullptr;
                if (leftVal->getType()->isStructTy()) {
                    hasVal = builder.CreateExtractValue(leftVal, 0, "has_val");
                } else if (leftVal->getType()->isPointerTy()) {
                    llvm::Value *nullPtr = llvm::ConstantPointerNull::get(builder.getPtrTy());
                    hasVal = builder.CreateICmpNE(leftVal, nullPtr, "not_null");
                }

                llvm::Function *func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock *thenBB  = llvm::BasicBlock::Create(context, "sino.then", func);
                llvm::BasicBlock *elseBB  = llvm::BasicBlock::Create(context, "sino.else", func);
                llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "sino.merge", func);

                builder.CreateCondBr(hasVal, thenBB, elseBB);

                builder.SetInsertPoint(thenBB);
                llvm::Value *thenUnboxed = leftVal;
                if (leftVal->getType()->isStructTy()) {
                    thenUnboxed = builder.CreateExtractValue(leftVal, 1, "unboxed_left");
                }
                builder.CreateBr(mergeBB);
                thenBB = builder.GetInsertBlock();

                builder.SetInsertPoint(elseBB);
                llvm::Value *rightVal = visitExpression(rightNode);
                builder.CreateBr(mergeBB);
                elseBB = builder.GetInsertBlock();

                builder.SetInsertPoint(mergeBB);


                if (rightVal->getType()->isStructTy()) {
                    llvm::Value *thenVal = leftVal;
                    if (!thenVal->getType()->isStructTy()) {
                        auto currentIP = builder.saveIP();

                        builder.SetInsertPoint(thenBB->getTerminator());
                        llvm::Value *boxed = llvm::UndefValue::get(rightVal->getType());
                        boxed = builder.CreateInsertValue(boxed, builder.getInt1(true), 0);
                        boxed = builder.CreateInsertValue(boxed, thenVal, 1);
                        thenVal = boxed;
                        builder.restoreIP(currentIP);
                    }

                    llvm::PHINode *phi = builder.CreatePHI(rightVal->getType(), 2, "sino.res");
                    phi->addIncoming(thenVal, thenBB);
                    phi->addIncoming(rightVal, elseBB);
                    return phi;
                }


                llvm::PHINode *phi = builder.CreatePHI(rightVal->getType(), 2, "sino.res");
                phi->addIncoming(thenUnboxed, thenBB);
                phi->addIncoming(rightVal, elseBB);
                return phi;
            }

            if (node->value == "==" || node->value == "!=") {
                bool isRightNulo = (rightNode->type == NodeType::NULO);
                bool isLeftNulo = (leftNode->type == NodeType::NULO);

                if (isRightNulo || isLeftNulo) {
                    const ASTNode *targetNode = isRightNulo ? leftNode : rightNode;
                    namedValuesStruct *var = lookupNamedValue(targetNode->value);

                    if (var && var->isNullable) {
                        if (var->nodeType != NodeType::STRING) {
                            llvm::Value *structVal = builder.CreateLoad(var->pointer->getAllocatedType(), var->pointer);
                            llvm::Value *hasVal = builder.CreateExtractValue(structVal, 0, "has_val");
                            return (node->value == "!=") ? hasVal : builder.CreateNot(hasVal, "is_nulo");
                        } else {
                            llvm::Value *ptrVal = builder.CreateLoad(builder.getPtrTy(), var->pointer);
                            llvm::Value *nullPtr = llvm::ConstantPointerNull::get(builder.getPtrTy());
                            return (node->value == "!=")
                                       ? builder.CreateICmpNE(ptrVal, nullPtr)
                                       : builder.CreateICmpEQ(ptrVal, nullPtr);
                        }
                    }
                }
            }

            llvm::Value *L = visitExpression(leftNode);
            llvm::Value *R = visitExpression(rightNode);
            if (!L || !R) return nullptr;

            if (node->value == "and") {
                return builder.CreateAnd(toBool(L), toBool(R), "andtmp");
            } else if (node->value == "or") {
                return builder.CreateOr(toBool(L), toBool(R), "ortmp");
            }

            if (node->value == "==" || node->value == "!=") {
                bool isLBool = L->getType()->isIntegerTy(1);
                bool isRBool = R->getType()->isIntegerTy(1);

                if (isLBool || isRBool) {
                    llvm::Value *boolL = toBool(L);
                    llvm::Value *boolR = toBool(R);
                    return (node->value == "==")
                               ? builder.CreateICmpEQ(boolL, boolR, "cmptmp")
                               : builder.CreateICmpNE(boolL, boolR, "cmptmp");
                }
            }

            if (L->getType()->isStructTy()) {
                L = builder.CreateExtractValue(L, 1, "unboxed_l");
            }
            if (R->getType()->isStructTy()) {
                R = builder.CreateExtractValue(R, 1, "unboxed_r");
            }

            bool isLFP = L->getType()->isFloatingPointTy();
            bool isRFP = R->getType()->isFloatingPointTy();

            // Promote integer types to the wider type
            if (!isLFP && !isRFP && L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                unsigned lBits = L->getType()->getIntegerBitWidth();
                unsigned rBits = R->getType()->getIntegerBitWidth();

                if (lBits != rBits) {
                    if (lBits < rBits) {
                        // Promote L to R's width
                        if (lBits == 1) {
                            L = builder.CreateZExt(L, R->getType(), "promotel");
                        } else {
                            L = builder.CreateSExt(L, R->getType(), "promotel");
                        }
                    } else {
                        // Promote R to L's width
                        if (rBits == 1) {
                            R = builder.CreateZExt(R, L->getType(), "promoter");
                        } else {
                            R = builder.CreateSExt(R, L->getType(), "promoter");
                        }
                    }
                }
            }

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
                if (!isFP) {
                    emitDivisionByZeroCheck(R);
                }
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
            } else if (node->value == "%") {
                emitDivisionByZeroCheck(R);
                return builder.CreateSRem(L, R, "modtmp");
            }


            return nullptr;
        }
        case NodeType::NULO:
            return llvm::ConstantPointerNull::get(builder.getPtrTy());
        case NodeType::BOOLEAN:
            return builder.getInt1(node->value == "true");
        case NodeType::FUNCTION_CALL: {
            return visitFunction(node);
        }
        case NodeType::ARRAY_ACCESS: {
            return visitArrayAccess(node);
        }
        default:
            return nullptr;
    }
}

llvm::Value *CodeGenerator::visitArrayAccess(const ASTNode *node) {

    const ASTNode *arrayNode = node->children[0].get();
    const ASTNode *indexNode = node->children[1].get();

    std::string arrayName = arrayNode->value;
    namedValuesStruct *var = lookupNamedValue(arrayName);
    if (!var) {
        expect("Compile Error: array '" + arrayName + "' not found");
        return nullptr;
    }

    llvm::Value *indexVal = visitExpression(indexNode);
    if (!indexVal) return nullptr;

    if (indexVal->getType()->isIntegerTy() && !indexVal->getType()->isIntegerTy(32)) {
        if (indexVal->getType()->getIntegerBitWidth() < 32) {
            indexVal = builder.CreateSExt(indexVal, builder.getInt32Ty(), "arr.idx.ext");
        } else {
            indexVal = builder.CreateTrunc(indexVal, builder.getInt32Ty(), "arr.idx.trunc");
        }
    }

    llvm::Type *allocType = var->pointer->getAllocatedType();

    emitBoundsCheck(indexVal, allocType);

    llvm::Value *gepPtr = builder.CreateGEP(allocType, var->pointer, {builder.getInt32(0), indexVal}, "arr.gep");

    llvm::ArrayType *arrayType = llvm::cast<llvm::ArrayType>(allocType);
    llvm::Type *elementType = arrayType->getElementType();
    llvm::Value *loadedVal = builder.CreateLoad(elementType, gepPtr, "arr.load");

    return loadedVal;
}


// llvm::Value *CodeGenerator::createHeapString(std::string str) {
//     auto len = str.length();
//     auto* value = builder.getInt64Ty();
//     llvm::Constant* size = llvm::ConstantInt::get(value, len + 1);
//
//     llvm::DataLayout dataLayout = builder.GetInsertBlock()->getModule()->getDataLayout();
//     llvm::Type *intPtrTy = builder.getIntPtrTy(dataLayout);
//
//     auto* ptr = builder.CreateMalloc(intPtrTy, value, size, nullptr);
//     llvm::Constant* src = builder.CreateGlobalString(str + '\0');
//     builder.CreateMemCpy(ptr, std::nullopt, src, std::nullopt, size);
//     return ptr;
// }


// void CodeGenerator::newHeapString(std::string str, llvm::Value *type) {
//     llvm::Value* oldPtr = builder.CreateLoad(builder.getPtrTy(), type);
//     builder.CreateFree(oldPtr);
//     llvm::Value* value = createHeapString(str);
//     builder.CreateStore(value, type);
// }



void CodeGenerator::saveIr(const std::string& filename) {
    std::error_code EC;
    if (EC) {
        std::cerr << "Error: failed to open '" << filename << "'\n";
        return;
    }
    llvm::raw_fd_ostream out(filename, EC);
    module->print(out, nullptr);
}

void CodeGenerator::compileAndRun(const std::string& outputBin, bool execute, std::string optimization) {

    saveIr("gir.ll");
#if defined(_WIN32) || defined(_WIN64)
    std::string cmd = "clang " + optimization + " gir.ll ../libs/runtime.c -o " + outputBin + ".exe -llegacy_stdio_definitions";
    int result = system(cmd.c_str());
    std::string runCmd = outputBin + ".exe";
    std::string delCmd = "del gir.ll";
#else
    std::string cmd = "clang " + optimization + " gir.ll ../libs/runtime.c -o " + outputBin;
    int result = system(cmd.c_str());
    std::string runCmd = "./" + outputBin;
    std::string delCmd = "rm gir.ll";
#endif

    if (result != 0) {
        std::cerr << "[Glue] Compilation failed with code " << result << "\n";
        return;
    }

    if (!execute) {
        std::cout << "[Glue] Successfully compiled to: " << outputBin << "\n";
    }
    if (execute) {
        system(runCmd.c_str());
    }

    system(delCmd.c_str());
}

void CodeGenerator::runJit() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto jitResult = llvm::orc::LLJITBuilder().create();
    if (!jitResult) {
        std::cerr << "[Glue] Error: failed to create JIT!\n";
        llvm::consumeError(jitResult.takeError());
        return;
    }
    auto &jit = *jitResult;

    std::string runtimeObj = "runtime.o";
    std::string runtimeSrc = "../libs/runtime.c";
    std::string compileCmd = "clang -c -fPIC " + runtimeSrc + " -o " + runtimeObj;
    if (system(compileCmd.c_str()) != 0) {
        std::cerr << "[Glue] JIT Error: failed to compile runtime.c!\n";
        return;
    }

    {
        std::error_code EC;
        auto runtimeBuffer = llvm::MemoryBuffer::getFile(runtimeObj);
        if (EC) {
            std::cerr << "[Glue] JIT Error: failed to load " << runtimeObj << ": " << EC.message() << "\n";
            std::string delCmd = "rm -f " + runtimeObj;
            system(delCmd.c_str());
            return;
        }
        if (auto err = jit->addObjectFile(std::move(*runtimeBuffer))) {
            std::cerr << "[Glue] JIT Error: failed to load runtime object file!\n";
            llvm::consumeError(std::move(err));
            std::string delCmd = "rm -f " + runtimeObj;
            system(delCmd.c_str());
            return;
        }
    }

    auto jitContext = std::make_unique<llvm::LLVMContext>();

    std::string irStr;
    {
        llvm::raw_string_ostream ros(irStr);
        module->print(ros, nullptr);
    }

    auto buffer = llvm::MemoryBuffer::getMemBufferCopy(irStr, "jit_module");
    llvm::SMDiagnostic diag;
    auto jitModule = llvm::parseIR(buffer->getMemBufferRef(), diag, *jitContext);
    if (!jitModule) {
        std::cerr << "[Glue] JIT Error: failed to parse IR for JIT context: " << diag.getMessage().str() << "\n";
        std::string delCmd = "rm -f " + runtimeObj;
        system(delCmd.c_str());
        return;
    }
    jitModule->setDataLayout(module->getDataLayoutStr());
    jitModule->setTargetTriple(module->getTargetTriple());

    llvm::orc::ThreadSafeModule tsModule(std::move(jitModule), std::move(jitContext));

    if (auto err = jit->addIRModule(std::move(tsModule))) {
        std::cerr << "[Glue] JIT Error: failed to load module!\n";
        llvm::consumeError(std::move(err));
        std::string delCmd = "rm -f " + runtimeObj;
        system(delCmd.c_str());
        return;
    }

    auto mainAddrResult = jit->lookup("main");
    if (auto err = mainAddrResult.takeError()) {
        std::cerr << "[Glue] JIT Error: failed to find main function!\n";
        llvm::consumeError(std::move(err));
        std::string delCmd = "rm -f " + runtimeObj;
        system(delCmd.c_str());
        return;
    }

    auto mainFunc = mainAddrResult->toPtr<int ()>();
    mainFunc();

    std::string delCmd = "rm -f " + runtimeObj;
    system(delCmd.c_str());
}



void CodeGenerator::save() {
    saveIr("gir.ll");
}

void CodeGenerator::run() {
#if defined(_WIN32) || defined(_WIN64)
    system("clang -O2 gir.ll -o glue_program.exe -llegacy_stdio_definitions");
    system("glue_program.exe");
    system("del gir.ll");
#else
    system("clang -O2 gir.ll ../libs/runtime.c -o glue_program && ./glue_program");
    system("rm gir.ll");
#endif
}

void CodeGenerator::expect(std::string msg) {
    std::stringstream stream;
    stream << msg << std::endl;
    throw ParseError(stream.str());
}
