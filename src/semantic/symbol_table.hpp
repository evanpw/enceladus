#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include "semantic/symbol.hpp"

#include <unordered_map>
#include <memory>

class SymbolTable
{
public:
    VariableSymbol* createVariableSymbol(const std::string& name, AstNode* node, bool global);
    FunctionSymbol* createFunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);
    CaptureSymbol* createCaptureSymbol(const std::string& name, AstNode* node, VariableSymbol* envSymbol, size_t index);
    ConstructorSymbol* createConstructorSymbol(const std::string& name, AstNode* node, ValueConstructor* constructor, const std::vector<MemberVarSymbol*>& memberSymbols);

    MethodSymbol* createMethodSymbol(const std::string& name, FunctionDefNode* node, Type* parentType);
    TraitMethodSymbol* createTraitMethodSymbol(const std::string& name, AstNode* node, TraitSymbol* traitSymbol);
    MemberVarSymbol* createMemberVarSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition, Type* parentType, size_t index);

    TraitSymbol* createTraitSymbol(const std::string& name, AstNode* node, Trait* trait, Type* traitVar, std::vector<Type*>&& typeParameters = {});
    TypeSymbol* createTypeSymbol(const std::string& name, AstNode* node, Type* type);

    DummySymbol* createDummySymbol(const std::string& name, AstNode* node);

    void pushScope();
    void popScope();

    enum WhichTable {TYPE = 0, OTHER = 1};

    // Returns nullptr if the symbol is not found in the symbol table
    Symbol* find(const std::string& name, WhichTable whichTable = OTHER);
    Symbol* findTopScope(const std::string& name, WhichTable whichTable = OTHER);

    // Members are stored in a separate unscoped table, and can have more than
    // one entry with the same name (with different types)
    void findMembers(const std::string& name, std::vector<MemberSymbol*>& result);
    void resolveMemberSymbol(const std::string& name, Type* parentType, std::vector<MemberSymbol*>& symbols);

    MethodSymbol* resolveTraitInstanceMethod(const std::string& name, Type* objectType, TraitSymbol* traitSymbol);
    Type* resolveAssociatedType(const std::string& name, Type* objectType, TraitSymbol* traitSymbol);

    bool isTopScope() const { return _scopes.size() == 1; }

private:
    void insert(Symbol* symbol, WhichTable = OTHER);

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

    // Unscoped table of methods / member variables
    std::unordered_map<std::string, std::vector<MemberSymbol*>> _members;
};

#endif
