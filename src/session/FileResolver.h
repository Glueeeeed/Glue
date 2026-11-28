#pragma once

#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

class FileResolver {
public:
    explicit FileResolver(const std::filesystem::path& entryFile,
                          const std::filesystem::path& projectRoot = {})
        : projectRoot_(projectRoot.empty() ? findProjectRoot(entryFile) : canonicalize(projectRoot)) {}

    std::string resolve(const std::string& moduleName, const std::string& importerPath) const {
        if (!isModuleName(moduleName)) {
            throw std::runtime_error("Invalid import name '" + moduleName + "' in " + importerPath);
        }

        const std::filesystem::path candidate = projectRoot_ / "libs" / (moduleName + ".glue");
        std::error_code ec;
        if (!std::filesystem::is_regular_file(candidate, ec) || ec) {
            throw std::runtime_error("Cannot resolve import '" + moduleName + "' from " + importerPath +
                                     "; expected file: " + candidate.string());
        }

        return canonicalize(candidate).string();
    }

    const std::filesystem::path& projectRoot() const {
        return projectRoot_;
    }

private:
    std::filesystem::path projectRoot_;

    static std::filesystem::path canonicalize(const std::filesystem::path& path) {
        std::error_code ec;
        std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
        return ec ? path.lexically_normal() : canonical;
    }

    static std::filesystem::path findProjectRoot(const std::filesystem::path& entryFile) {
        const std::filesystem::path entry = canonicalize(entryFile);
        std::filesystem::path directory = entry.parent_path();
        for (std::filesystem::path current = directory; !current.empty(); current = current.parent_path()) {
            std::error_code ec;
            if (std::filesystem::is_directory(current / "libs", ec) && !ec) {
                return canonicalize(current);
            }
            if (current == current.root_path()) {
                break;
            }
        }
        return canonicalize(directory);
    }

    static bool isModuleName(const std::string& name) {
        if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_')) {
            return false;
        }
        for (char character : name) {
            if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '_')) {
                return false;
            }
        }
        return true;
    }
};