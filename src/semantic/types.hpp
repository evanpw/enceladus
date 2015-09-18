#ifndef TYPES_HPP
#define TYPES_HPP

#include <cassert>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class BaseType;
class ConstructedType;
class FunctionType;
class Type;
class TypeConstructor;
class TypeScheme;
class TypeTable;
class TypeVariable;
class ValueConstructor;

enum TypeTag {ttBase, ttFunction, ttVariable, ttConstructed};

// Base class of all types
class Type
{
public:
    Type(TypeTable* table, TypeTag tag)
    : _table(table), _tag(tag)
    {}

    virtual ~Type() {}
    virtual std::string name() const = 0;
    virtual bool isBoxed() const = 0;

    TypeTable* table()
    {
        return _table;
    }

    virtual TypeTag tag() const
    {
        return _tag;
    }

    virtual bool isVariable() const
    {
        return false;
    }

    virtual const std::vector<ValueConstructor*>& valueConstructors() const
    {
        return _valueConstructors;
    }

    virtual std::pair<size_t, ValueConstructor*> getValueConstructor(const std::string& name) const;

    virtual void addValueConstructor(ValueConstructor* valueConstructor)
    {
        _valueConstructors.push_back(valueConstructor);
    }

    virtual std::set<TypeVariable*> freeVars();

    template <class T>
    T* get()
    {
        return dynamic_cast<T*>(this);
    }

private:
    TypeTable* _table;
    TypeTag _tag;
    std::vector<ValueConstructor*> _valueConstructors;
};

// If a type variable with target set, then dereference. Otherwise, return type
Type* unwrap(Type* type);

// Represents a bottom-level basic types (Int, Bool, ...)
class BaseType : public Type
{
public:
    virtual std::string name() const
    {
        return _name;
    }

    virtual bool isBoxed() const
    {
        return !_primitive;
    }

private:
    friend TypeTable;

    BaseType(TypeTable* table, const std::string& name, bool primitive)
    : Type(table, ttBase), _name(name), _primitive(primitive)
    {
    }

    std::string _name;
    bool _primitive;
};

// The type of a function from one type to another
class FunctionType : public Type
{
public:
    virtual std::string name() const;
    virtual bool isBoxed() const { return true; }

    const std::vector<Type*>& inputs() const
    {
        return _inputs;
    }

    Type* output() const
    {
        return _output;
    }

private:
    friend TypeTable;

    FunctionType(TypeTable* table, const std::vector<Type*>& inputs, Type* output)
    : Type(table, ttFunction), _inputs(inputs), _output(output)
    {
    }

    std::vector<Type*> _inputs;
    Type* _output;
};

// The type of any type constructed from other types
class ConstructedType : public Type
{
public:
    virtual std::string name() const;

    virtual bool isBoxed() const
    {
        return true;
    }

    TypeConstructor* typeConstructor()
    {
        return _typeConstructor;
    }

    const std::vector<Type*>& typeParameters() const
    {
        return _typeParameters;
    }

private:
    friend TypeTable;

    ConstructedType(TypeTable* table, TypeConstructor* typeConstructor, std::initializer_list<Type*> typeParameters);
    ConstructedType(TypeTable* table, TypeConstructor* typeConstructor, const std::vector<Type*>& typeParameters);

    TypeConstructor* _typeConstructor;
    std::vector<Type*> _typeParameters;
};

// A variable which can be substituted with a type. Used for polymorphism.
class TypeVariable : public Type
{
public:
    virtual std::string name() const
    {
        flatten();
        if (_target)
        {
            return _target->name();
        }
        else
        {
            std::stringstream ss;
            ss << "T" << _index;
            return ss.str();
        }
    }

    virtual bool isBoxed() const
    {
        flatten();
        if (_target)
        {
            return _target->isBoxed();
        }
        else
        {
            return true;
        }
    }

    virtual TypeTag tag() const
    {
        flatten();
        if (_target)
        {
            return _target->tag();
        }
        else
        {
            return ttVariable;
        }
    }

    virtual bool isVariable() const
    {
        return true;
    }

    virtual const std::vector<ValueConstructor*>& valueConstructors() const
    {
        flatten();
        assert(_target);
        return _target->valueConstructors();
    }

    virtual std::pair<size_t, ValueConstructor*> getValueConstructor(const std::string& name) const
    {
        flatten();
        assert(_target);
        return _target->getValueConstructor(name);
    }

    virtual void addValueConstructor(ValueConstructor* valueConstructor)
    {
        flatten();
        assert(_target);
        _target->addValueConstructor(valueConstructor);
    }

    Type* deref() const
    {
        flatten();
        return _target;
    }

    Type* target() const
    {
        return _target;
    }

    void assign(Type* target)
    {
        assert(!_rigid);
        _target = target;
    }

    int index() const
    {
        return _index;
    }

    bool rigid() const
    {
        return _rigid;
    }

private:
    friend TypeTable;

    TypeVariable(TypeTable* table, bool rigid)
    : Type(table, ttVariable), _index(_count++), _rigid(rigid)
    {
    }

    void flatten() const
    {
        // Flatten long chains when possible
        while (_target && _target->tag() == ttVariable)
        {
            _target = _target->get<TypeVariable>()->target();
        }
    }

    mutable Type* _target = nullptr;

    int _index;
    bool _rigid;

    static int _count;
};

struct Symbol;

class ValueConstructor
{
public:
    virtual std::string name() const;

    Symbol* symbol() const
    {
        return _symbol;
    }

    struct MemberDesc
    {
        MemberDesc(const std::string& name, Type* type, size_t location)
        : name(name), type(type), location(location)
        {}

        std::string name;
        Type* type;
        size_t location;
    };

    std::vector<MemberDesc>& members()
    {
        return _members;
    }

private:
    friend TypeTable;

    ValueConstructor(Symbol* symbol, const std::vector<Type*>& memberTypes, const std::vector<std::string>& memberNames = {});

    Symbol* _symbol;
    std::vector<MemberDesc> _members;
};

class TypeScheme
{
public:
    TypeTable* table()
    {
        return _type->table();
    }

    std::string name() const;

    // Convenience redirections to the underlying type
    virtual TypeTag tag() const
    {
        return _type->tag();
    }

    virtual bool isBoxed() const
    {
        return _type->isBoxed();
    }

    const std::vector<ValueConstructor*>& valueConstructors() const
    {
        return _type->valueConstructors();
    }

    Type* type()
    {
        return _type;
    }

    std::set<TypeVariable*> freeVars();

    const std::set<TypeVariable*>& quantified() const
    {
        return _quantified;
    }

private:
    friend TypeTable;

    TypeScheme(Type* type, const std::set<TypeVariable*>& quantified)
    : _type(type)
    {
        for (auto& elem : quantified)
        {
            _quantified.emplace(elem);
        }
    }

    TypeScheme(Type* type, std::initializer_list<TypeVariable*> quantified)
    : _type(type)
    {
        for (auto& elem : quantified)
        {
            _quantified.emplace(elem);
        }
    }

    Type* _type;
    std::set<TypeVariable*> _quantified;
};

class TypeConstructor
{
public:
    TypeTable* table()
    {
        return _table;
    }

    const std::string& name() const
    {
        return _name;
    }

    size_t parameters() const
    {
        return _parameters;
    }

    virtual const std::vector<ValueConstructor*>& valueConstructors() const
    {
        return _valueConstructors;
    }

    void addValueConstructor(ValueConstructor* valueConstructor)
    {
        _valueConstructors.emplace_back(valueConstructor);
    }

private:
    friend TypeTable;

    TypeConstructor(TypeTable* table, const std::string& name, size_t parameters = 0)
    : _table(table), _name(name), _parameters(parameters)
    {}

    TypeTable* _table;
    std::string _name;
    size_t _parameters;
    std::vector<ValueConstructor*> _valueConstructors;
};

// Exists only to own all type-related objects
class TypeTable
{
public:
    TypeTable();

    BaseType* createBaseType(const std::string& name, bool primitive = false)
    {
        BaseType* type = new BaseType(this, name, primitive);
        _types.emplace_back(type);

        return type;
    }

    FunctionType* createFunctionType(const std::vector<Type*>& inputs, Type* output)
    {
        FunctionType* type = new FunctionType(this, inputs, output);
        _types.emplace_back(type);

        return type;
    }

    ConstructedType* createConstructedType(TypeConstructor* typeConstructor, std::initializer_list<Type*> typeParameters)
    {
        ConstructedType* type = new ConstructedType(this, typeConstructor, typeParameters);
        _types.emplace_back(type);

        return type;
    }

    ConstructedType* createConstructedType(TypeConstructor* typeConstructor, const std::vector<Type*>& typeParameters)
    {
        ConstructedType* type = new ConstructedType(this, typeConstructor, typeParameters);
        _types.emplace_back(type);

        return type;
    }

    TypeVariable* createTypeVariable(bool rigid=false)
    {
        TypeVariable* type = new TypeVariable(this, rigid);
        _types.emplace_back(type);

        return type;
    }

    TypeConstructor* createTypeConstructor(const std::string& name, size_t parameters = 0)
    {
        TypeConstructor* typeConstructor = new TypeConstructor(this, name, parameters);
        _typeConstructors.emplace_back(typeConstructor);

        return typeConstructor;
    }

    TypeScheme* createTypeScheme(Type* type, std::initializer_list<TypeVariable*> quantified = {})
    {
        assert(type->table() == this);

        TypeScheme* typeScheme = new TypeScheme(type, quantified);
        _typeSchemes.emplace_back(typeScheme);

        return typeScheme;
    }

    TypeScheme* createTypeScheme(Type* type, const std::set<TypeVariable*>& quantified)
    {
        assert(type->table() == this);

        TypeScheme* typeScheme = new TypeScheme(type, quantified);
        _typeSchemes.emplace_back(typeScheme);

        return typeScheme;
    }

    ValueConstructor* createValueConstructor(Symbol* symbol, const std::vector<Type*>& memberTypes, const std::vector<std::string>& memberNames = {})
    {
        ValueConstructor* valueConstructor = new ValueConstructor(symbol, memberTypes, memberNames);
        _valueConstructors.emplace_back(valueConstructor);

        return valueConstructor;
    }

    // For easy access to commonly-used types
    Type* Int;
    Type* Bool;
    Type* Unit;
    Type* String;

    TypeConstructor* Function;
    TypeConstructor* Array;

private:
    std::vector<std::unique_ptr<Type>> _types;
    std::vector<std::unique_ptr<TypeConstructor>> _typeConstructors;
    std::vector<std::unique_ptr<TypeScheme>> _typeSchemes;
    std::vector<std::unique_ptr<ValueConstructor>> _valueConstructors;
};

#endif
