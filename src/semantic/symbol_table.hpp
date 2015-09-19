#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include "semantic/symbol.hpp"

#include <unordered_map>
#include <memory>

class SymbolTable
{
public:
    void pushScope();
    void popScope();

    enum WhichTable {TYPE = 0, OTHER = 1};

    // Returns nullptr if the symbol is not found in the symbol table
    Symbol* find(const std::string& name, WhichTable whichTable = OTHER);
    Symbol* findTopScope(const std::string& name, WhichTable whichTable = OTHER);
    void insert(Symbol* symbol, WhichTable = OTHER);

    bool isTopScope() const { return _scopes.size() == 1; }

private:
    // Owning references
    std::vector<std::unique_ptr<Symbol>> _symbols;

    using KeyType = std::pair<std::string, WhichTable>;

    struct PairHash
    {
        size_t operator()(const KeyType& key) const
        {
            return std::hash<std::string>()(key.first) + static_cast<size_t>(key.second);
        }
    };

    // Scoped lookup table
    using Scope = std::unordered_map<KeyType, Symbol*, PairHash>;
    std::vector<Scope> _scopes;
};

#endif
