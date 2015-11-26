#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include "semantic/types.hpp"
#include "utility.hpp"

#include <cassert>
#include <vector>

class AstNode;
class FunctionDefNode;
class TraitDefNode;
class SymbolTable;


enum Kind {kDummy, kVariable, kFunction, kCapture, kType, kMethod, kMemberVar, kTrait, kTraitMethod};

class Symbol
{
public:
    // This is just so that we can use dynamic_cast
    virtual ~Symbol() {}

    std::string name;

    // The node at which this symbol is first declared.
    AstNode* node;

    bool global;

    // Type (possibly polymorphic) of this variable or function
    Type* type = nullptr;

    // Variable, function, ...?
    Kind kind;

protected:
    Symbol(const std::string& name, Kind kind, AstNode* node, bool global)
    : name(name)
    , node(node)
    , global(global)
    , kind(kind)
    {}
};

// Symbol type for syntactic elements that we dont' want overridden (e.g., Else)
class DummySymbol : public Symbol
{
private:
    friend class SymbolTable;
    DummySymbol(const std::string& name, AstNode* node);
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
    VariableSymbol(const std::string& name, AstNode* node, bool global);
};

class FunctionSymbol : public Symbol
{
public:
    bool isExternal = false;
    bool isBuiltin = false;
    bool isConstructor = false;
    bool isLambda = false;

    FunctionDefNode* definition;

protected:
    friend class SymbolTable;
    FunctionSymbol(const std::string& name, AstNode* node, FunctionDefNode* definition);
};

// Variable captured in a closure
class CaptureSymbol : public Symbol
{
public:
    VariableSymbol* envSymbol;
    size_t index;

private:
    friend class SymbolTable;
    CaptureSymbol(const std::string& name, AstNode* node, VariableSymbol* envSymbol, size_t index);
};

class MemberVarSymbol;

class ConstructorSymbol : public FunctionSymbol
{
public:
    ValueConstructor* constructor;
    std::vector<MemberVarSymbol*> memberSymbols;

private:
    friend class SymbolTable;
    ConstructorSymbol(const std::string& name, AstNode* node, ValueConstructor* constructor, const std::vector<MemberVarSymbol*>& memberSymbols);
};

class MethodSymbol;

class TypeSymbol : public Symbol
{
private:
    friend class SymbolTable;
    TypeSymbol(const std::string& name, AstNode* node, Type* type);
};

class TraitMethodSymbol;

class TraitInstance
{
public:
    TraitInstance(Type* type, AstNode* implNode, Trait* trait, std::unordered_map<std::string, MethodSymbol*>&& methods, std::unordered_map<std::string, Type*>&& associatedTypes)
    : type(type), implNode(implNode), trait(trait), methods(methods), associatedTypes(associatedTypes)
    {}

    Type* type;
    AstNode* implNode;
    Trait* trait;

    std::unordered_map<std::string, MethodSymbol*> methods;
    std::unordered_map<std::string, Type*> associatedTypes;
};

class TraitSymbol : public Symbol
{
public:
    Trait* trait;

    // Plays the role of the instance type in the method types
    Type* traitVar;

    // Type parameters: should all appear in traitVar
    std::vector<Type*> typeParameters;

    std::unordered_map<std::string, TraitMethodSymbol*> methods;
    std::unordered_map<std::string, Type*> associatedTypes;

    TraitInstance* addInstance(Type* type, AstNode* implNode, std::unordered_map<std::string, MethodSymbol*>&& methods, std::unordered_map<std::string, Type*>&& associatedTypes)
    {
        TraitInstance* instance = new TraitInstance(type, implNode, trait, std::move(methods), std::move(associatedTypes));
        _instances[type].reset(instance);

        return instance;
    }

    TraitInstance* getInstance(Type* type)
    {
        auto i = _instances.find(type);
        if (i == _instances.end())
        {
            return nullptr;
        }
        else
        {
            return i->second.get();
        }
    }

private:
    friend class SymbolTable;
    TraitSymbol(const std::string& name, AstNode* node, Trait* trait, Type* traitVar, std::vector<Type*>&& typeParameters);

    std::unordered_map<Type*, std::unique_ptr<TraitInstance>> _instances;
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

class TraitMethodSymbol : public MemberSymbol
{
public:
    virtual bool isTraitMethod() { return true; }

    TraitSymbol* traitSymbol;
    Trait* trait;

private:
    friend class SymbolTable;
    TraitMethodSymbol(const std::string& name, AstNode* node, TraitSymbol* traitSymbol);
};

class MemberVarSymbol : public MemberSymbol
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
