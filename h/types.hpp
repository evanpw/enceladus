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
class TypeConstructor;
class Type;
class TypeScheme;
class TypeVariable;
class ValueConstructor;

enum TypeTag {ttBase, ttFunction, ttVariable, ttConstructed};

// Base class of all types
class Type
{
public:
    Type(TypeTag tag)
    : _tag(tag)
    {}

    virtual ~Type() {}
    virtual std::string name() const = 0;
    virtual bool isBoxed() const = 0;

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
        _valueConstructors.emplace_back(valueConstructor);
    }

    virtual std::set<TypeVariable*> freeVars();

    template <class T>
    T* get()
    {
        return dynamic_cast<T*>(this);
    }

    // For easy access to commonly-used types
    static std::shared_ptr<Type> Int;
    static std::shared_ptr<Type> Bool;
    static std::shared_ptr<Type> Unit;
    static std::shared_ptr<Type> String;

private:
    TypeTag _tag;
    std::vector<ValueConstructor*> _valueConstructors;
};

// If a type variable with target set, then dereference. Otherwise, return type
std::shared_ptr<Type> unwrap(const std::shared_ptr<Type>& type);

// Represents a bottom-level basic types (Int, Bool, ...)
class BaseType : public Type
{
public:
    static std::shared_ptr<Type> create(const std::string& name, bool primitive = false)
    {
        return std::shared_ptr<Type>(new BaseType(name, primitive));
    }

    virtual std::string name() const
    {
        return _name;
    }

    virtual bool isBoxed() const
    {
        return !_primitive;
    }

private:
    BaseType(const std::string& name, bool primitive)
    : Type(ttBase), _name(name), _primitive(primitive)
    {
    }

    std::string _name;
    bool _primitive;
};

// The type of a function from one type to another
class FunctionType : public Type
{
public:
    static std::shared_ptr<Type> create(const std::vector<std::shared_ptr<Type>>& inputs, const std::shared_ptr<Type>& output)
    {
        return std::shared_ptr<Type>(new FunctionType(inputs, output));
    }

    virtual std::string name() const;
    virtual bool isBoxed() const { return true; }

    const std::vector<std::shared_ptr<Type>>& inputs() const
    {
        return _inputs;
    }

    const std::shared_ptr<Type>& output() const
    {
        return _output;
    }

private:
    FunctionType(const std::vector<std::shared_ptr<Type>>& inputs, const std::shared_ptr<Type>& output)
    : Type(ttFunction), _inputs(inputs), _output(output)
    {
    }

    std::vector<std::shared_ptr<Type>> _inputs;
    std::shared_ptr<Type> _output;
};

// The type of any type constructed from other types
class ConstructedType : public Type
{
public:
    static std::shared_ptr<Type> create(const std::shared_ptr<TypeConstructor>& typeConstructor, std::initializer_list<std::shared_ptr<Type>> typeParameters)
    {
        return std::shared_ptr<Type>(new ConstructedType(typeConstructor, typeParameters));
    }

    static std::shared_ptr<Type> create(const std::shared_ptr<TypeConstructor>& typeConstructor, const std::vector<std::shared_ptr<Type>> typeParameters)
    {
        return std::shared_ptr<Type>(new ConstructedType(typeConstructor, typeParameters));
    }

    virtual std::string name() const;

    virtual bool isBoxed() const
    {
        return true;
    }

    const std::shared_ptr<TypeConstructor>& typeConstructor() const
    {
        return _typeConstructor;
    }

    const std::vector<std::shared_ptr<Type>>& typeParameters() const
    {
        return _typeParameters;
    }

private:
    ConstructedType(const std::shared_ptr<TypeConstructor>& typeConstructor, std::initializer_list<std::shared_ptr<Type>> typeParameters);
    ConstructedType(const std::shared_ptr<TypeConstructor>& typeConstructor, const std::vector<std::shared_ptr<Type>> typeParameters);

    std::shared_ptr<TypeConstructor> _typeConstructor;
    std::vector<std::shared_ptr<Type>> _typeParameters;
};

// A variable which can be substituted with a type. Used for polymorphism.
class TypeVariable : public Type
{
public:
    static std::shared_ptr<Type> create(bool rigid=false)
    {
        return std::shared_ptr<Type>(new TypeVariable(rigid));
    }

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
            ss << "a" << _index;
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

    std::shared_ptr<Type> deref() const
    {
        flatten();
        return _target;
    }

    std::shared_ptr<Type> target() const
    {
        return _target;
    }

    void assign(const std::shared_ptr<Type> target)
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
    TypeVariable(bool rigid)
    : Type(ttVariable)
    , _index(_count++)
    , _rigid(rigid)
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

    mutable std::shared_ptr<Type> _target;

    int _index;
    bool _rigid;

    static int _count;
};

struct Symbol;

class ValueConstructor
{
public:
    ValueConstructor(Symbol* symbol, const std::vector<std::shared_ptr<Type>>& memberTypes, const std::vector<std::string>& memberNames = {});

    virtual std::string name() const;

    Symbol* symbol() const
    {
        return _symbol;
    }

    struct MemberDesc
    {
        MemberDesc(const std::string& name, std::shared_ptr<Type> type, size_t location)
        : name(name), type(type), location(location)
        {}

        std::string name;
        std::shared_ptr<Type> type;
        size_t location;
    };

    std::vector<MemberDesc>& members()
    {
        return _members;
    }

private:
    Symbol* _symbol;
    std::vector<MemberDesc> _members;
};

class TypeScheme
{
public:
    TypeScheme(const std::shared_ptr<Type>& type, const std::set<TypeVariable*>& quantified)
    : _type(type)
    {
        for (auto& elem : quantified)
        {
            _quantified.emplace(elem);
        }
    }

    TypeScheme(const std::shared_ptr<Type>& type, std::initializer_list<TypeVariable*> quantified)
    : _type(type)
    {
        for (auto& elem : quantified)
        {
            _quantified.emplace(elem);
        }
    }

    static std::shared_ptr<TypeScheme> trivial(const std::shared_ptr<Type>& type)
    {
        return std::shared_ptr<TypeScheme>(new TypeScheme(type, {}));
    }

    static std::shared_ptr<TypeScheme> make(const std::shared_ptr<Type>& type, std::initializer_list<TypeVariable*> quantified)
    {
        return std::shared_ptr<TypeScheme>(new TypeScheme(type, quantified));
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

    const std::shared_ptr<Type>& type() const
    {
        return _type;
    }

    std::set<TypeVariable*> freeVars() const;

    const std::set<TypeVariable*>& quantified() const
    {
        return _quantified;
    }

private:
    std::shared_ptr<Type> _type;
    std::set<TypeVariable*> _quantified;
};

class TypeConstructor
{
public:
    static std::shared_ptr<TypeConstructor> create(const std::string& name, size_t parameters = 0)
    {
        return std::shared_ptr<TypeConstructor>(new TypeConstructor(name, parameters));
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

    // For easy access to commonly-used types
    static std::shared_ptr<TypeConstructor> Function;
    static std::shared_ptr<TypeConstructor> Array;

private:
    TypeConstructor(const std::string& name, size_t parameters = 0)
    : _name(name), _parameters(parameters)
    {}

    std::string _name;
    size_t _parameters;
    std::vector<ValueConstructor*> _valueConstructors;
};

#endif
