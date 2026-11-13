#include "scope.h"

Scope::Scope() {
    enterScope();
}

void Scope::enterScope() {
    scopes.emplace_back();
}

void Scope::exitScope() {
    if (scopes.size() > 1) {
        scopes.pop_back();
    }
}

bool Scope::declare(const std::string& name, const SymbolInfo& info) {
    if (scopes.empty()) {
        return false;
    }
    if (scopes.back().count(name)) {
        return false;
    }
    scopes.back()[name] = info;
    return true;
}


SymbolInfo* Scope::lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &(found->second);
        }
    }
    return nullptr;
}

SymbolInfo* Scope::lookupOriginal(const std::string& name) {
    // Search from the outermost scope to find the original declaration
    // Skip the innermost scope (current scope) which may contain narrowed versions
    if (scopes.size() <= 1) {
        return lookup(name);
    }
    for (size_t i = 0; i < scopes.size() - 1; ++i) {
        auto found = scopes[i].find(name);
        if (found != scopes[i].end()) {
            return &(found->second);
        }
    }
    // If not found in outer scopes, check the innermost scope as fallback
    return lookup(name);
}