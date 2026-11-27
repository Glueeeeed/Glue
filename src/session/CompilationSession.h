#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "SourceUnit.h"


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
        : entryPath_(normalizePath(entryFile)) {}

    void loadEntry() {
        SourceUnit unit;
        unit.canonicalPath = entryPath_;
        unit.tokens = lexer(entryPath_.c_str());
        if (unit.tokens.empty()) {
            throw std::runtime_error("Cannot load file: " + entryPath_);
        }
        units_.push_back(std::move(unit));
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
        std::string canon = normalizePath(std::move(rawPath));
        if (const SourceUnit* existing = findUnit(canon)) {
            return *existing;
        }

        SourceUnit unit;
        unit.canonicalPath = canon;
        unit.tokens = lexer(canon.c_str());
        if (unit.tokens.empty()) {
            throw std::runtime_error("Cannot load file: " + canon);
        }
        units_.push_back(std::move(unit));
        return units_.back();
    }

    const SourceUnit& mainUnit() const { return units_.front(); }
    const std::string& entryPath() const { return entryPath_; }

private:
    std::string entryPath_;
    std::vector<SourceUnit> units_;
};