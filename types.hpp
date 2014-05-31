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

    void append(TypeName* parameter) { parameters_.emplace_back(parameter); }

private:
    const std::string name_;
    std::vector<std::unique_ptr<TypeName>> parameters_;
};

std::ostream& operator<<(std::ostream& out, const TypeName& typeName);

class TypeConstructor
{
public:
    TypeConstructor(const std::string& name, size_t parameters = 0)
    : name_(name), parameters_(parameters)
    {}

    const std::string& name() const { return name_; }
    size_t parameters() const { return parameters_; }

private:
    std::string name_;
    size_t parameters_;
};

// Owns all type constructors used in the program, and keeps track of primitive
// types as well.
class Type;
class TypeTable
{
public:
    TypeTable();

    const TypeConstructor* getTypeConstructor(const std::string& name);
    std::shared_ptr<Type> getBaseType(const std::string& name);

    void insert(const std::string& name, TypeConstructor* typeConstructor);
    void insert(const std::string& name, std::shared_ptr<Type> baseType);

    std::shared_ptr<Type> nameToType(const TypeName* typeName);

    // Convenient access to frequently-referenced types
    static std::shared_ptr<Type> Int;
    static std::shared_ptr<Type> Bool;
    static std::shared_ptr<Type> Unit;

private:
    std::unordered_map<std::string, std::unique_ptr<TypeConstructor>> typeConstructors_;
    std::unordered_map<std::string, std::shared_ptr<Type>> baseTypes_;
};

enum TypeTag {ttBase, ttFunction, ttVariable, ttConstructed, ttStruct};

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

    const std::vector<std::unique_ptr<ValueConstructor>>& valueConstructors() const;
    void addValueConstructor(ValueConstructor* valueConstructor);

    std::set<TypeVariable*> freeVars() const;

    template <typename T>
    T* get() const { return dynamic_cast<T*>(_impl.get()); }

private:
    std::shared_ptr<TypeImpl> _impl;
};

// Represents a fully-instantiated value constructor, with no free type variables
class ValueConstructor
{
public:
    ValueConstructor(const std::string& name, const std::vector<std::shared_ptr<Type>>& members);

    const std::string& name() const { return name_; }
    const std::vector<std::shared_ptr<Type>>& members() const { return members_; }
    const std::vector<size_t>& memberLocations() const { return memberLocations_; }
    size_t boxedMembers() const { return boxedMembers_; }
    size_t unboxedMembers() const { return unboxedMembers_; }

private:
    std::string name_;
    std::vector<std::shared_ptr<Type>> members_;
    std::vector<size_t> memberLocations_;
    size_t boxedMembers_, unboxedMembers_;
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

    std::string name() const;

    // Convenience redirections to the underlying type
    virtual TypeTag tag() const { return _type->tag(); }
    virtual bool isBoxed() const { return _type->isBoxed(); }
    const std::vector<std::unique_ptr<ValueConstructor>>& valueConstructors() const { return _type->valueConstructors(); }

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

    virtual const std::vector<std::unique_ptr<ValueConstructor>>& valueConstructors() const { return valueConstructors_; }
    void addValueConstructor(ValueConstructor* valueConstructor) { valueConstructors_.emplace_back(valueConstructor); }

protected:
    std::vector<std::unique_ptr<ValueConstructor>> valueConstructors_;

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
            valueConstructors_.emplace_back(new ValueConstructor(name, {}));
        }
    }

    std::string name_;
    bool primitive_;
    std::vector<std::unique_ptr<ValueConstructor>> valueConstructors_;
};

class StructDefNode;

// Represents a compound type (a struct)
class StructType : public TypeImpl
{
public:
    static std::shared_ptr<Type> create(const std::string& name, StructDefNode* node)
    {
        return std::make_shared<Type>(new StructType(name, node));
    }

    virtual std::string name() const { return name_; }
    virtual bool isBoxed() const { return true; }

    size_t boxedMembers() const { return boxedMembers_; }
    size_t unboxedMembers() const { return unboxedMembers_; }

    struct MemberDesc
    {
        std::shared_ptr<Type> type;
        size_t location;
    };

    const std::unordered_map<std::string, MemberDesc>& members() { return members_; }

private:
    StructType(const std::string& name, StructDefNode* node);

    std::string name_;
    std::unordered_map<std::string, MemberDesc> members_;

    size_t boxedMembers_, unboxedMembers_;
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
    }

    // TODO: Build value constructors
    ConstructedType(const TypeConstructor* typeConstructor, const std::vector<std::shared_ptr<Type>> typeParameters)
    : TypeImpl(ttConstructed), _typeConstructor(typeConstructor)
    {
        for (const std::shared_ptr<Type>& parameter : typeParameters)
        {
            _typeParameters.push_back(parameter);
        }
    }

    const TypeConstructor* _typeConstructor;
    std::vector<std::shared_ptr<Type>> _typeParameters;
    std::vector<std::unique_ptr<ValueConstructor>> _valueConstructors;
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
