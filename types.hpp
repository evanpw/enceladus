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

class TypeConstructor;
class Type
{
public:
    Type(const TypeConstructor* typeConstructor, const std::vector<const Type*>& typeParameters)
    : typeConstructor_(typeConstructor), typeParameters_(typeParameters)
    {}

    const TypeConstructor* typeConstructor() const { return typeConstructor_; }
    const std::vector<const Type*>& typeParameters() const { return typeParameters_; }

    // Determines whether this type is a subtype of the rhs. Not symmetric
    bool is(const Type* rhs) const;

    // For convenience
    bool isSimple() const;
    const std::string& name() const;
    std::string longName() const;
    size_t constructorCount() const;

private:
    Type(const Type& rhs) = delete;
    Type& operator=(const Type& rhs) = delete;

    const TypeConstructor* typeConstructor_;
    std::vector<const Type*> typeParameters_;
};

class TypeName;

// Represents a type constructor (e.g. List) which when given type parameters (possibly
// zero of them), produces a concrete type (e.g., [Int]). This class owns all concrete
// types instantiated from this type constructor
class TypeConstructor
{
public:
    TypeConstructor(const std::string& name, bool isSimple, size_t parameterCount = 0)
    : name_(name), isSimple_(isSimple), parameterCount_(parameterCount)
    {}

    const Type* instantiate();
    const Type* instantiate(const std::vector<const Type*>& parameters);

    // Properties of the type constructor
    const std::string& name() const { return name_; }
    size_t parameterCount() const { return parameterCount_; }

    // Properties of the instantiated types
    size_t valueConstructorCount() const { return 1; }  // TODO: Allow multiple value constructors
    bool isSimple() const { return isSimple_; }

private:
    std::string name_;
    bool isSimple_;
    size_t parameterCount_;

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

    void insert(const std::string& name, TypeConstructor* typeConstructor);

private:
    std::unordered_map<std::string, std::unique_ptr<TypeConstructor>> table_;
};

#endif
