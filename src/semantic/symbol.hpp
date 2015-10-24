#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include "semantic/types.hpp"

#include <cassert>
#include <vector>

class AstNode;
class FunctionDefNode;
class TraitDefNode;
class SymbolTable;


enum Kind {kVariable, kFunction, kType, kMethod, kMemberVar, kTrait, kTraitMethod};

class Symbol
{
public:
    // This is just so that we can use dynamic_cast
    virtual ~Symbol() {}

    std::string name;

    // The node at which this symbol is first declared.
    AstNode* node;

    // May be null
    FunctionDefNode* enclosingFunction;

    bool global;

    // Type (possibly polymorphic) of this variable or function
    Type* type = nullptr;

    // Variable, function, ...?
    Kind kind;

protected:
    Symbol(const std::string& name, Kind kind, AstNode* node, FunctionDefNode* enclosingFunction, bool global)
    : name(name)
    , node(node)
    , enclosingFunction(enclosingFunction)
    , global(global)
    , kind(kind)
    {}
};


class VariableSymbol : public Symbol
{
public:
    // Is this symbol a function parameter?
    bool isParam = false;

    bool isStatic = false;

    // Used by the code generator to assign a place on the stack (relative to rbp) for all of
    // the local variables.
    int offset = -1;

    // For static strings
    std::string contents;

private:
    friend class SymbolTable;
    VariableSymbol(const std::string& name, AstNode* node, FunctionDefNode* enclosingFunction, bool global);
};

class FunctionSymbol : public Symbol
{
public:
    bool isExternal = false;
    bool isBuiltin = false;
    bool isConstructor = false;

    FunctionDefNode* definition;

protected:
    friend class SymbolTable;
    FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);
};

struct MemberVarSymbol;

class ConstructorSymbol : public FunctionSymbol
{
public:
    ValueConstructor* constructor;
    std::vector<MemberVarSymbol*> memberSymbols;

private:
    friend class SymbolTable;
    ConstructorSymbol(const std::string& name, AstNode* node, ValueConstructor* constructor, const std::vector<MemberVarSymbol*>& memberSymbols);
};

class TypeSymbol : public Symbol
{
private:
    friend class SymbolTable;
    TypeSymbol(const std::string& name, AstNode* node, Type* type);
};

class TraitSymbol : public Symbol
{
public:
    Trait* trait;

    // Plays the role of the instance type in the method types
    Type* traitVar;

    // Type parameters: should all appear in traitVar
    std::vector<Type*> typeParameters;

    std::unordered_map<std::string, Type*> methods;

private:
    friend class SymbolTable;
    TraitSymbol(const std::string& name, AstNode* node, Trait* trait, Type* traitVar, std::vector<Type*>&& typeParameters);
};

class MemberSymbol : public Symbol
{
public:
    Type* parentType;

    virtual bool isMethod() { return false; }
    virtual bool isMemberVar() { return false; }
    virtual bool isTraitMethod() { return false; }

protected:
    friend class SymbolTable;
    MemberSymbol(const std::string& name, Kind kind, AstNode* node, Type* parentType);
};

class MethodSymbol : public MemberSymbol
{
public:
    FunctionDefNode* definition;

    virtual bool isMethod() { return true; }

protected:
    friend class SymbolTable;
    MethodSymbol(const std::string& name, FunctionDefNode* node, Type* parentType);
};

struct TraitMethodSymbol : public MemberSymbol
{
public:
    virtual bool isTraitMethod() { return true; }

    Trait* trait;

private:
    friend class SymbolTable;
    TraitMethodSymbol(const std::string& name, AstNode* node, TraitSymbol* traitSymbol);
};

struct MemberVarSymbol : public MemberSymbol
{
public:
    ConstructorSymbol* constructorSymbol = nullptr;
    size_t index;

    virtual bool isMemberVar() { return true; }

private:
    friend class SymbolTable;
    MemberVarSymbol(const std::string& name, AstNode* node, Type* parentType, size_t index);
};

#endif
