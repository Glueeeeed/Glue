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