#ifndef TYPES_HPP
#define TYPES_HPP

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class TypeName
{
public:
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

// Represents a fully-instantiated value constructor, with no free type variables
class Type;
class ValueConstructor
{
public:
    ValueConstructor(const std::string& name, const std::vector<const Type*>& members)
    : name_(name), members_(members) {}

    const std::string& name() const { return name_; }
    const std::vector<const Type*>& members() const { return members_; }

private:
    std::string name_;
    std::vector<const Type*> members_;
};

class TypeConstructor;
class Type
{
public:
    Type(const TypeConstructor* typeConstructor, const std::vector<const Type*>& typeParameters)
    : typeConstructor_(typeConstructor), typeParameters_(typeParameters)
    {}

    const TypeConstructor* typeConstructor() const { return typeConstructor_; }
    const std::vector<const Type*>& typeParameters() const { return typeParameters_; }

    void addValueConstructor(ValueConstructor* valueConstructor) { valueConstructors_.emplace_back(valueConstructor); }
    const std::vector<std::unique_ptr<ValueConstructor>>& valueConstructors() const { return valueConstructors_; }

    // Determines whether this type is a subtype of the rhs. Not symmetric
    bool is(const Type* rhs) const;

    // For convenience
    bool isSimple() const;
    const std::string& name() const;
    std::string longName() const;
    std::string mangledName() const;

private:
    Type(const Type& rhs) = delete;
    Type& operator=(const Type& rhs) = delete;

    const TypeConstructor* typeConstructor_;
    std::vector<const Type*> typeParameters_;
    std::vector<std::unique_ptr<ValueConstructor>> valueConstructors_;
};

class TypeName;

// Represents a type constructor (e.g. List) which when given type parameters (possibly
// zero of them), produces a concrete type (e.g., [Int]). This class owns all concrete
// types instantiated from this type constructor
class ConstructorSpec;
class TypeTable;
class TypeConstructor
{
public:
    TypeConstructor(const std::string& name, bool isSimple)
    : table_(nullptr), name_(name), isSimple_(isSimple)
    {}

    TypeConstructor(const std::string& name, bool isSimple, const std::vector<std::string> parameters)
    : table_(nullptr), name_(name), isSimple_(isSimple), parameters_(parameters)
    {}

    void setTable(TypeTable* table) { table_ = table; }

    void addValueConstructor(const ConstructorSpec* valueConstructor) { valueConstructors_.push_back(valueConstructor); }
    const std::vector<const ConstructorSpec*>& valueConstructors() { return valueConstructors_; }

    // Properties of the type constructor
    const std::string& name() const { return name_; }
    const std::vector<std::string>& parameters() const { return parameters_; }

    // Properties of the instantiated types
    bool isSimple() const { return isSimple_; }

private:
    friend class TypeTable;
    const Type* lookup();
    const Type* lookup(const std::vector<const Type*>& parameters);

    const Type* instantiate();
    const Type* instantiate(const std::vector<const Type*>& parameters);

    // Keep a back-reference to the type table so that we can do lookups of value constructor
    // parameters
    TypeTable* table_;

    std::string name_;
    bool isSimple_;

    std::vector<std::string> parameters_;

    std::vector<const ConstructorSpec*> valueConstructors_;

    // Maps type parameters to the concrete instantiated type (which is owned by this
    // object)
    // TODO: Add a hash function and convert to unordered_map
    std::map<std::vector<const Type*>, std::unique_ptr<Type>> instantiations_;
};

// Stores all type constructors defined in the program, and allows the compiler to
// lookup types based on their description, or by name if there are no type parameters.
// This class owns all of the type constructors.
class TypeTable
{
public:
    TypeTable();

    const Type* lookup(const std::string& name);
    const Type* lookup(const TypeName* name);
    const Type* lookup(const TypeName* name, const std::unordered_map<std::string, const Type*>& context);

    void insert(const std::string& name, TypeConstructor* typeConstructor);

    const std::vector<const Type*> allTypes() const { return concreteTypes_; }

private:
    std::unordered_map<std::string, std::unique_ptr<TypeConstructor>> typeConstructors_;
    std::vector<const Type*> concreteTypes_;
};

#endif
