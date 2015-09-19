#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include "semantic/symbol.hpp"

#include <unordered_map>
#include <memory>

class SymbolTable
{
public:
    VariableSymbol* createVariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global);
    FunctionSymbol* createFunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);
    MemberSymbol* createMemberSymbol(const std::string& name, AstNode* node);
    MethodSymbol* createMethodSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition, Type* parentType);

    TypeSymbol* createTypeSymbol(const std::string& name, AstNode* node, Type* type);
    TypeConstructorSymbol* createTypeConstructorSymbol(const std::string& name, AstNode* node, TypeConstructor* typeConstructor);

    void pushScope();
    void popScope();

    enum WhichTable {TYPE = 0, OTHER = 1};

    // Returns nullptr if the symbol is not found in the symbol table
    Symbol* find(const std::string& name, WhichTable whichTable = OTHER);
    Symbol* findTopScope(const std::string& name, WhichTable whichTable = OTHER);

    // Methods are stored in a separate unscoped table, and can have more than
    // one entry with the same name
    void findMethods(const std::string& name, std::vector<MethodSymbol*>& result);

    bool isTopScope() const { return _scopes.size() == 1; }

private:
    void insert(Symbol* symbol, WhichTable = OTHER);
    void insertMethod(MethodSymbol* symbol);

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

    // Unscoped method table
    std::unordered_map<std::string, std::vector<MethodSymbol*>> _methods;
};

#endif
