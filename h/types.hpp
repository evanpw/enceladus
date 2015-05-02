#ifndef TYPES_HPP
#define TYPES_HPP

#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class TypeName
{
public:
    TypeName(const std::string& name)
    : name_(name)
    {}

    TypeName(const char* name)
    : name_(name)
    {}

    const std::string& name() const { return name_; }
    const std::vector<std::unique_ptr<TypeName>>& parameters() const { return parameters_; }

    std::string str() const;

    void append(TypeName* parameter) { parameters_.emplace_back(parameter); }

private:
    const std::string name_;
    std::vector<std::unique_ptr<TypeName>> parameters_;
};

class ValueConstructor;
class TypeConstructor
{
public:
    TypeConstructor(const std::string& name, size_t parameters = 0)
    : name_(name), parameters_(parameters)
    {}

    const std::string& name() const { return name_; }
    size_t parameters() const { return parameters_; }

    virtual const std::vector<std::shared_ptr<ValueConstructor>>& valueConstructors() const { return _valueConstructors; }
    void addValueConstructor(ValueConstructor* valueConstructor) { _valueConstructors.emplace_back(valueConstructor); }

private:
    std::string name_;
    size_t parameters_;
    std::vector<std::shared_ptr<ValueConstructor>> _valueConstructors;
};

enum TypeTag {ttBase, ttFunction, ttVariable, ttConstructed};

class TypeImpl;
class TypeVariable;

class ValueConstructor;
class Type
{
public:
    Type(TypeImpl* impl)
    : _impl(impl)
    {}

    virtual ~Type() {}

    TypeTag tag() const;
    bool isBoxed() const;
    std::string name() const;
    bool isAlgebraic() const;

    const std::vector<std::shared_ptr<ValueConstructor>>& valueConstructors() const;
    void addValueConstructor(ValueConstructor* valueConstructor);

    std::set<TypeVariable*> freeVars() const;

    template <typename T>
    T* get() const { return dynamic_cast<T*>(_impl.get()); }

private:
    std::shared_ptr<TypeImpl> _impl;
};

class ValueConstructor
{
public:
    ValueConstructor(const std::string& name, const std::vector<std::shared_ptr<Type>>& memberTypes, const std::vector<std::string>& memberNames = {});

    virtual std::string name() const { return name_; }

    struct MemberDesc
    {
        MemberDesc(const std::string& name, std::shared_ptr<Type> type, size_t location)
        : name(name), type(type), location(location)
        {}

        std::string name;
        std::shared_ptr<Type> type;
        size_t location;
    };

    std::vector<MemberDesc>& members() { return members_; }

private:
    std::string name_;
    std::vector<MemberDesc> members_;
};

class TypeVariable;

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
    virtual TypeTag tag() const { return _type->tag(); }
    virtual bool isBoxed() const { return _type->isBoxed(); }
    const std::vector<std::shared_ptr<ValueConstructor>>& valueConstructors() const { return _type->valueConstructors(); }

    const std::shared_ptr<Type>& type() const { return _type; }
    std::set<TypeVariable*> freeVars() const;
    const std::set<TypeVariable*>& quantified() const { return _quantified; }

private:
    std::shared_ptr<Type> _type;
    std::set<TypeVariable*> _quantified;
};

// Base class of all types
class TypeImpl
{
public:
    TypeImpl(TypeTag tag)
    : _tag(tag)
    {}

    virtual ~TypeImpl() {}

    virtual TypeTag tag() const { return _tag; }
    virtual std::string name() const = 0;
    virtual bool isBoxed() const = 0;
    virtual bool isAlgebraic() const { return _valueConstructors.size() > 1; }

    virtual const std::vector<std::shared_ptr<ValueConstructor>>& valueConstructors() const { return _valueConstructors; }
    void addValueConstructor(ValueConstructor* valueConstructor) { _valueConstructors.emplace_back(valueConstructor); }

protected:
    std::vector<std::shared_ptr<ValueConstructor>> _valueConstructors;

private:
    TypeTag _tag;
};

// Represents a bottom-level basic types (Int, Bool, ...)
class BaseType : public TypeImpl
{
public:
    static std::shared_ptr<Type> create(const std::string& name, bool primitive = false)
    {
        return std::make_shared<Type>(new BaseType(name, primitive));
    }

    virtual std::string name() const { return name_; }
    virtual bool isBoxed() const { return !primitive_; }

private:
    BaseType(const std::string& name, bool primitive)
    : TypeImpl(ttBase), name_(name), primitive_(primitive)
    {
        if (!primitive)
        {
            //_valueConstructors.emplace_back(new ValueConstructor(name, {}));
        }
    }

    std::string name_;
    bool primitive_;
};

// The type of a function from one type to another
class FunctionType : public TypeImpl
{
public:
    static std::shared_ptr<Type> create(const std::vector<std::shared_ptr<Type>>& inputs, const std::shared_ptr<Type>& output)
    {
        return std::make_shared<Type>(new FunctionType(inputs, output));
    }

    virtual std::string name() const;
    virtual bool isBoxed() const { return true; }

    const std::vector<std::shared_ptr<Type>>& inputs() const { return _inputs; }
    const std::shared_ptr<Type>& output() const { return _output; }

private:
    FunctionType(const std::vector<std::shared_ptr<Type>>& inputs, const std::shared_ptr<Type>& output)
    : TypeImpl(ttFunction), _inputs(inputs), _output(output)
    {}

    std::vector<std::shared_ptr<Type>> _inputs;
    std::shared_ptr<Type> _output;
};

// The type of any type constructed from other types
class ConstructedType : public TypeImpl
{
public:
    static std::shared_ptr<Type> create(const TypeConstructor* typeConstructor, std::initializer_list<std::shared_ptr<Type>> typeParameters)
    {
        return std::make_shared<Type>(new ConstructedType(typeConstructor, typeParameters));
    }

    static std::shared_ptr<Type> create(const TypeConstructor* typeConstructor, const std::vector<std::shared_ptr<Type>> typeParameters)
    {
        return std::make_shared<Type>(new ConstructedType(typeConstructor, typeParameters));
    }

    virtual std::string name() const;
    virtual bool isBoxed() const { return true; }

    const TypeConstructor* typeConstructor() const { return _typeConstructor; }
    const std::vector<std::shared_ptr<Type>>& typeParameters() const { return _typeParameters; }

private:
    // TODO: Build value constructors
    ConstructedType(const TypeConstructor* typeConstructor, std::initializer_list<std::shared_ptr<Type>> typeParameters)
    : TypeImpl(ttConstructed), _typeConstructor(typeConstructor)
    {
        for (const std::shared_ptr<Type>& parameter : typeParameters)
        {
            _typeParameters.push_back(parameter);
        }

        for (auto& valueConstructor : typeConstructor->valueConstructors())
        {
            _valueConstructors.push_back(valueConstructor);
        }
    }

    // TODO: Build value constructors
    ConstructedType(const TypeConstructor* typeConstructor, const std::vector<std::shared_ptr<Type>> typeParameters)
    : TypeImpl(ttConstructed), _typeConstructor(typeConstructor)
    {
        for (const std::shared_ptr<Type>& parameter : typeParameters)
        {
            _typeParameters.push_back(parameter);
        }

        for (auto& valueConstructor : typeConstructor->valueConstructors())
        {
            _valueConstructors.push_back(valueConstructor);
        }
    }

    const TypeConstructor* _typeConstructor;
    std::vector<std::shared_ptr<Type>> _typeParameters;
};

// A variable which can be substituted with a type. Used for polymorphism.
class TypeVariable : public TypeImpl
{
public:
    static std::shared_ptr<Type> create()
    {
        return std::make_shared<Type>(new TypeVariable);
    }

    virtual bool isBoxed() const { return true; }
    virtual std::string name() const;

    int index() const { return _index; }

private:
    TypeVariable()
    : TypeImpl(ttVariable), _index(_count++)
    {}

    int _index;
    static int _count;
};

#endif
