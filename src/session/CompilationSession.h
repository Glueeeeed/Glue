#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "SourceUnit.h"
#include "../codegen/codegen.h"
#include "../parser/parser.h"


inline std::string normalizePath(const std::string& rawPath) {
    if (rawPath.empty()) return "";
    std::error_code ec;
    std::filesystem::path p = std::filesystem::weakly_canonical(std::filesystem::path(rawPath), ec);
    if (ec) {
        return rawPath;
    }
    return p.string();
}

class CompilationSession {
public:
    explicit CompilationSession(std::string entryFile)
        : entryPath_(normalizePath(entryFile)), codeGenerator_(context_) {}

    void loadEntry() {
        addUnit(entryPath_);
    }

    const SourceUnit* findUnit(const std::string& path) const {
        std::string canon = normalizePath(path);
        for (const auto& u : units_) {
            if (u.canonicalPath == canon) {
                return &u;
            }
        }
        return nullptr;
    }

    bool hasUnit(const std::string& path) const {
        return findUnit(path) != nullptr;
    }


    const SourceUnit& addUnit(std::string rawPath) {
        std::string canon = normalizePath(rawPath);
        if (const SourceUnit* existing = findUnit(canon)) {
            return *existing;
        }
        if (analysisStarted_ || codegenStarted_) {
            throw std::logic_error("Cannot add a source unit after analysis has started");
        }

        std::error_code ec;
        if (!std::filesystem::is_regular_file(canon, ec) || ec) {
            throw std::runtime_error("Cannot load file: " + canon);
        }

        SourceUnit unit;
        unit.canonicalPath = canon;
        unit.tokens = lexer(canon.c_str());
        units_.push_back(std::move(unit));
        return units_.back();
    }

    void parseAll() {
        if (!findUnit(entryPath_)) {
            throw std::logic_error("Load the entry file before parsing");
        }
        for (size_t i = 0; i < units_.size(); ++i) {
            SourceUnit& unit = units_[i];
            if (unit.ast.has_value()) {
                continue;
            }
            Parser parser(unit.tokens, context_);
            parser.parse();
            unit.ast = parser.getAST();
        }
    }

    void analyzeAll() {
        if (analysisStarted_) {
            throw std::logic_error("Semantic analysis has already started for this session");
        }
        if (!findUnit(entryPath_)) {
            throw std::logic_error("Load the entry file before semantic analysis");
        }
        for (const auto& unit : units_) {
            if (!unit.ast.has_value()) {
                throw std::logic_error("Parse all source units before semantic analysis");
            }
        }

        analysisStarted_ = true;
        for (const auto& unit : units_) {
            semantic_.registerFunctionSignatures(unit.ast->getRoot());
        }

        for (const auto& unit : units_) {
            semantic_.analyseRegistered(unit.ast->getRoot(), unit.canonicalPath == entryPath_);
        }
        analysisComplete_ = true;
    }

    void generateCode() {
        if (!analysisComplete_) {
            throw std::logic_error("Run semantic analysis before code generation");
        }
        if (codegenStarted_) {
            throw std::logic_error("Code generation has already started for this session");
        }

        codegenStarted_ = true;
        std::vector<const ASTNode*> roots;
        roots.reserve(units_.size());
        for (const auto& unit : units_) {
            roots.push_back(unit.ast->getRoot());
        }
        codeGenerator_.generateCode(roots);
        codegenComplete_ = true;
    }

    void compileAndRun(const std::string& outputBin = "glue_program", bool execute = false,
                       const std::string& optimization = "-O2") {
        if (!codegenComplete_) {
            throw std::logic_error("Generate code before compiling the program");
        }
        codeGenerator_.compileAndRun(outputBin, execute, optimization);
    }

    void runJit() {
        if (!codegenComplete_) {
            throw std::logic_error("Generate code before running the program with JIT");
        }
        codeGenerator_.runJit();
    }

    const SourceUnit& mainUnit() const {
        const SourceUnit* unit = findUnit(entryPath_);
        if (!unit) {
            throw std::logic_error("The compilation session has no entry source unit");
        }
        return *unit;
    }
    const std::string& entryPath() const { return entryPath_; }
    const std::vector<SourceUnit>& units() const { return units_; }
    void printAST() const;
    void printAST(const std::string& path) const;

    const ASTNode *get() const {
        const SourceUnit* unit = findUnit(entryPath_);
        return !unit || !unit->ast.has_value() ? nullptr : unit->ast->getRoot();
    }


private:
    std::string entryPath_;
    std::vector<SourceUnit> units_;
    SemanticAnalyzer semantic_;
    llvm::LLVMContext context_;
    CodeGenerator codeGenerator_;
    bool analysisStarted_ = false;
    bool analysisComplete_ = false;
    bool codegenStarted_ = false;
    bool codegenComplete_ = false;
};