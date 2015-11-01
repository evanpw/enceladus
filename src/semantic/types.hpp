#ifndef TYPES_HPP
#define TYPES_HPP

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class BaseType;
class ConstructedType;
class FunctionType;
class Trait;
class Type;
class TypeTable;
class TypeVariable;
class ValueConstructor;


enum TypeTag {ttBase, ttFunction, ttVariable, ttConstructed};


// Base class of all types
class TypeImpl
{
public:
    TypeImpl(TypeTable* table, TypeTag tag)
    : _table(table), _tag(tag)
    {}

    virtual ~TypeImpl() {}
    virtual std::string str() const = 0;
    virtual bool isBoxed() const = 0;

    TypeTable* table() const
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

    // Do nothing except for on type variables
    virtual void addReference(Type* parent) {}

protected:
    TypeTable* _table;
    TypeTag _tag;
    std::vector<ValueConstructor*> _valueConstructors;
};


// Shared public interface for all types
class Type
{
public:
    Type(const std::shared_ptr<TypeImpl>& impl)
    : _impl(impl)
    {
        _impl->addReference(this);
    }

    std::string str() const
    {
        return _impl->str();
    }

    // True if variables of this type are or may be stored on the heap. False
    // for immediate values, like Int or Bool
    bool isBoxed() const
    {
        return _impl->isBoxed();
    }

    TypeTable* table() const
    {
        return _impl->table();
    }

    TypeTag tag() const
    {
        return _impl->tag();
    }

    bool isVariable() const
    {
        return _impl->isVariable();
    }

    const std::vector<ValueConstructor*>& valueConstructors() const
    {
        return _impl->valueConstructors();
    }

    std::pair<size_t, ValueConstructor*> getValueConstructor(const std::string& name) const
    {
        return _impl->getValueConstructor(name);
    }

    void addValueConstructor(ValueConstructor* valueConstructor)
    {
        _impl->addValueConstructor(valueConstructor);
    }

    bool equals(const Type* rhs) const
    {
        return _impl == rhs->_impl;
    }

    template <class T>
    T* get() const
    {
        return dynamic_cast<T*>(_impl.get());
    }

private:
    friend class TypeVariable;
    std::shared_ptr<TypeImpl> _impl;
};

// Represents a bottom-level basic types (Int, Bool, ...)
class BaseType : public TypeImpl
{
public:
    const std::string& name() const
    {
        return _name;
    }

    virtual std::string str() const
    {
        return _name;
    }

    virtual bool isBoxed() const
    {
        return !_primitive;
    }

    // Size of underlying representation, in bits (only valid for unboxed types)
    size_t size() const
    {
        assert(!isBoxed());
        return _size;
    }

    // Always false except for integer types
    bool isSigned() const
    {
        assert(!isBoxed());
        return _signed;
    }

private:
    friend TypeTable;

    BaseType(TypeTable* table, const std::string& name, bool primitive, size_t size, bool isSigned)
    : TypeImpl(table, ttBase), _name(name), _primitive(primitive), _size(size), _signed(isSigned)
    {
    }

    std::string _name;
    bool _primitive;
    size_t _size;
    bool _signed;
};


// The type of a function from one type to another
class FunctionType : public TypeImpl
{
public:
    virtual std::string str() const;

    virtual bool isBoxed() const
    {
        return true;
    }

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
    : TypeImpl(table, ttFunction), _inputs(inputs), _output(output)
    {
    }

    std::vector<Type*> _inputs;
    Type* _output;
};


// The type of any type constructed from other types
class ConstructedType : public TypeImpl
{
public:
    const std::string& name() const
    {
        return _name;
    }

    virtual std::string str() const;

    virtual bool isBoxed() const
    {
        return true;
    }

    const std::vector<Type*>& typeParameters() const
    {
        return _typeParameters;
    }

    const ConstructedType* prototype() const
    {
        return _prototype;
    }

    Type* instantiate(std::vector<Type*>&& typeParameters) const;

    // Passed through the prototype
    virtual const std::vector<ValueConstructor*>& valueConstructors() const;
    virtual std::pair<size_t, ValueConstructor*> getValueConstructor(const std::string& name) const;

    // Valid only for prototype
    virtual void addValueConstructor(ValueConstructor* valueConstructor);

private:
    friend TypeTable;

    ConstructedType(TypeTable* table, const std::string& name, std::vector<Type*>&& typeParameters, const ConstructedType* prototype);

    std::string _name;
    std::vector<Type*> _typeParameters;

    // Set to this for the prototypical constructed type. This is what defines
    // identity for constructed types: types with the same prototype have the
    // same type constructor
    const ConstructedType* _prototype;
};

// A variable which can be substituted with a type. Used for polymorphism.
class TypeVariable : public TypeImpl
{
public:
    virtual std::string str() const;

    virtual bool isVariable() const
    {
        return true;
    }

    virtual bool isBoxed() const
    {
        assert(false);
    }

    virtual void addReference(Type* parent)
    {
        _references.push_back(parent);
    }

    void assign(const Type* rhs);

    int index() const
    {
        return _index;
    }

    bool quantified() const
    {
        return _quantified;
    }

    const std::set<Trait*>& constraints() const
    {
        return _constraints;
    }

    void addConstraint(Trait* trait)
    {
        _constraints.insert(trait);
    }

private:
    friend TypeTable;

    TypeVariable(TypeTable* table, const std::string name, bool quantified)
    : TypeImpl(table, ttVariable), _name(name), _index(_count++), _quantified(quantified)
    {
    }

    std::string _name;
    int _index;
    bool _quantified;
    std::set<Trait*> _constraints;

    std::vector<Type*> _references;

    static int _count;
};

using TypeAssignment = std::unordered_map<TypeVariable*, Type*>;

class ValueConstructor
{
public:
    struct MemberDesc
    {
        MemberDesc(const std::string& name, Type* type)
        : name(name), type(type)
        {}

        std::string name;
        Type* type;
    };

    virtual std::string str() const
    {
        return _name;
    }

    std::vector<MemberDesc>& members()
    {
        return _members;
    }

    size_t constructorTag() const
    {
        return _constructorTag;
    }

private:
    friend TypeTable;

    ValueConstructor(const std::string& name, size_t constructorTag, const std::vector<Type*>& memberTypes, const std::vector<std::string>& memberNames = {});

    std::string _name;
    std::vector<MemberDesc> _members;
    size_t _constructorTag;
};

class Trait
{
public:
    struct Instance
    {
        std::vector<Type*> traitParams;
        Type* type;
    };

    std::string name() const
    {
        return _name;
    }

    std::string str() const
    {
        std::stringstream ss;

        ss << _name;
        if (!_params.empty())
        {
            ss << "<";

            bool first = true;
            for (Type* param : _params)
            {
                if (!first) ss << ", ";
                ss << param->str();
                first = false;
            }

            ss << ">";
        }

        return ss.str();
    }

    TypeTable* table() const
    {
        return _table;
    }

    const std::vector<Type*>& parameters() const
    {
        return _params;
    }

    Trait* prototype() const
    {
        return _prototype;
    }

    Trait* instantiate(std::vector<Type*>&& params);

    const std::vector<Instance>& instances() const
    {
        if (_prototype == this)
        {
            return _instances;
        }
        else
        {
            return _prototype->instances();
        }
    }

    void addInstance(Type* type, const std::vector<Type*>& traitParams = {})
    {
        assert(_prototype == this);
        _instances.push_back({traitParams, type});
    }

private:
    friend TypeTable;

    Trait(TypeTable* table, const std::string& name, std::vector<Type*>&& params, Trait* prototype)
    : _table(table), _name(name), _params(params), _prototype(prototype)
    {
        if (!_prototype)
            _prototype = this;
    }

    TypeTable* _table;
    std::string _name;
    std::vector<Type*> _params;
    std::vector<Instance> _instances;

    // Similar to constructed types, this is what defines identity for a trait.
    // Set to "this" for the prototypical trait
    Trait* _prototype;
};


// Exists only to own all type-related objects
class TypeTable
{
public:
    TypeTable();

    Type* createBaseType(const std::string& name, bool primitive = false, size_t size = 64, bool isSigned = true)
    {
        std::shared_ptr<BaseType> typeImpl(new BaseType(this, name, primitive, size, isSigned));
        Type* type = new Type(typeImpl);
        _types.emplace_back(type);

        return type;
    }

    Type* createFunctionType(const std::vector<Type*>& inputs, Type* output)
    {
        std::shared_ptr<FunctionType> typeImpl(new FunctionType(this, inputs, output));
        Type* type = new Type(typeImpl);
        _types.emplace_back(type);

        return type;
    }

    Type* createConstructedType(const std::string& name, std::vector<Type*>&& typeParameters, const ConstructedType* prototype = nullptr)
    {
        std::shared_ptr<ConstructedType> typeImpl(new ConstructedType(this, name, std::move(typeParameters), prototype));
        Type* type = new Type(typeImpl);
        _types.emplace_back(type);

        return type;
    }

    Type* createTypeVariable(const std::string& name = "", bool quantified=false)
    {
        std::shared_ptr<TypeVariable> typeImpl(new TypeVariable(this, name, quantified));
        Type* type = new Type(typeImpl);
        _types.emplace_back(type);

        return type;
    }

    ValueConstructor* createValueConstructor(const std::string& name, size_t constructorTag, const std::vector<Type*>& memberTypes, const std::vector<std::string>& memberNames = {})
    {
        ValueConstructor* valueConstructor = new ValueConstructor(name, constructorTag, memberTypes, memberNames);
        _valueConstructors.emplace_back(valueConstructor);

        return valueConstructor;
    }

    Trait* createTrait(const std::string& name, std::vector<Type*>&& params= {}, Trait* prototype = nullptr)
    {
        Trait* trait = new Trait(this, name, std::move(params), prototype);
        _traits.emplace_back(trait);

        return trait;
    }

    std::vector<Trait*> traits() const
    {
        std::vector<Trait*> result;
        for (auto& trait : _traits)
        {
            result.push_back(trait.get());
        }

        return result;
    }

    // For easy access to commonly-used types
    Type* Int;
    Type* UInt;
    Type* UInt8;
    Type* Bool;
    Type* Unit;
    Type* String;

    Trait* Num;

    Type* Function;
    Type* Array;

private:
    std::vector<std::unique_ptr<Type>> _types;
    std::vector<std::unique_ptr<ValueConstructor>> _valueConstructors;
    std::vector<std::unique_ptr<Trait>> _traits;
};

#endif
