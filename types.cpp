#include <cstring>
#include <sstream>
#include "ast.hpp"
#include "types.hpp"

template<class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

bool Type::is(const Type* rhs) const
{
    // TODO: Do a better job of this
    return (this == rhs);
}

bool Type::isSimple() const
{
    return typeConstructor_->isSimple();
}

const std::string& Type::name() const
{
    return typeConstructor_->name();
}

std::string Type::longName() const
{
    if (typeParameters_.size() == 0)
    {
        return typeConstructor_->name();
    }
    else
    {
        std::stringstream ss;
        ss << "(";
        ss << typeConstructor_->name();

        for (size_t i = 0; i < typeParameters_.size(); ++i)
        {
            ss << " " << typeParameters_[i]->longName();
        }

        ss << ")";

        return ss.str();
    }
}

size_t Type::constructorCount() const
{
    return typeConstructor_->valueConstructorCount();
}

TypeTable::TypeTable()
{
    // Built-in types
    table_.emplace(std::make_pair("Void", make_unique<TypeConstructor>("Void", true)));
    table_.emplace(std::make_pair("Int", make_unique<TypeConstructor>("Int", true)));
    table_.emplace(std::make_pair("Bool", make_unique<TypeConstructor>("Bool", true)));
    table_.emplace(std::make_pair("List", make_unique<TypeConstructor>("List", false, 1)));
    table_.emplace(std::make_pair("Tree", make_unique<TypeConstructor>("Tree", false)));
}

void TypeTable::insert(const std::string& name, TypeConstructor* typeConstructor)
{
    table_.emplace(std::make_pair(name, std::unique_ptr<TypeConstructor>(typeConstructor)));
}

// TODO: Produce a semantic error when there is a problem looking up a type
const Type* TypeTable::lookup(const TypeName* name)
{
    // First, find the type constructor referred to by this TypeName
    auto i = table_.find(name->name());
    if (i == table_.end()) return nullptr;

    std::unique_ptr<TypeConstructor>& typeConstructor = i->second;
    assert(typeConstructor->parameterCount() == name->parameters().size());

    // Then, recursively look up all of the type parameters
    std::vector<const Type*> typeParameters;
    for (const std::unique_ptr<TypeName>& parameter : name->parameters())
    {
        const Type* parameterType = lookup(parameter.get());
        if (parameterType == nullptr) return nullptr;

        typeParameters.push_back(parameterType);
    }

    // Finally, instantiate (or retrieve, if already instantiated) the type
    // given by the constructor and the type parameters
    return typeConstructor->instantiate(typeParameters);
}

const Type* TypeTable::lookup(const std::string& name)
{
    TypeName dummy(name.c_str());
    return lookup(&dummy);
}

const Type* TypeConstructor::instantiate()
{
    return instantiate(std::vector<const Type*>());
}

const Type* TypeConstructor::instantiate(const std::vector<const Type*>& parameters)
{
    // First, check the cache to see if a type has already been instantiated with these
    // parameters
    auto i = instantiations_.find(parameters);
    if (i != instantiations_.end())
    {
        return i->second.get();
    }

    // Otherwise, create a new type from the template
    Type* newType = new Type(this, parameters);
    instantiations_.emplace(std::make_pair(parameters, std::unique_ptr<Type>(newType)));

    return newType;
}
