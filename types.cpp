#include <cstring>
#include <sstream>
#include "ast.hpp"
#include "types.hpp"

template<class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

const Type Type::any_(nullptr, std::vector<const Type*>());

bool Type::is(const Type* rhs) const
{
    // TODO: Do a better job of this
    return (rhs == any() || this == rhs);
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

std::string Type::mangledName() const
{
    if (typeParameters_.size() == 0)
    {
        return typeConstructor_->name();
    }
    else
    {
        std::stringstream ss;

        ss << typeConstructor_->name();
        ss << "_s";
        for (size_t i = 0; i < typeParameters_.size(); ++i)
        {
            ss << typeParameters_[i]->mangledName();
            if (i != typeParameters_.size() - 1) ss << "_n";
        }

        ss << "_e";

        return ss.str();
    }
}

TypeTable::TypeTable()
{
    // Built-in types
    typeConstructors_.emplace(std::make_pair("Void", make_unique<TypeConstructor>("Void", true)));
    typeConstructors_.emplace(std::make_pair("Int", make_unique<TypeConstructor>("Int", true)));
    typeConstructors_.emplace(std::make_pair("Bool", make_unique<TypeConstructor>("Bool", true)));
    typeConstructors_.emplace(std::make_pair("Tree", make_unique<TypeConstructor>("Tree", false)));

    std::vector<std::string> listParams = {"a"};
    typeConstructors_.emplace(std::make_pair("List", make_unique<TypeConstructor>("List", false, listParams)));
}

void TypeTable::insert(const std::string& name, TypeConstructor* typeConstructor)
{
    typeConstructor->setTable(this);
    typeConstructors_.emplace(std::make_pair(name, std::unique_ptr<TypeConstructor>(typeConstructor)));
}

// TODO: Produce a semantic error when there is a problem looking up a type
const Type* TypeTable::lookup(const TypeName* name, const std::unordered_map<std::string, const Type*>& context)
{
    // First, find the type constructor referred to by this TypeName
    auto i = typeConstructors_.find(name->name());
    if (i == typeConstructors_.end())
    {
        auto j = context.find(name->name());
        if (j == context.end())
        {
            return nullptr;
        }

        assert(name->parameters().size() == 0);
        return j->second;
    }

    std::unique_ptr<TypeConstructor>& typeConstructor = i->second;
    assert(typeConstructor->parameters().size() == name->parameters().size());

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
    const Type* type = typeConstructor->lookup(typeParameters);
    if (type != nullptr)
    {
        return type;
    }
    else
    {
        type = typeConstructor->instantiate(typeParameters);
        concreteTypes_.push_back(type);

        return type;
    }
}

const Type* TypeTable::lookup(const TypeName* name)
{
    std::unordered_map<std::string, const Type*> emptyContext;
    return lookup(name, emptyContext);
}

const Type* TypeTable::lookup(const std::string& name)
{
    TypeName dummy(name.c_str());
    return lookup(&dummy);
}

const Type* TypeConstructor::lookup()
{
    return lookup(std::vector<const Type*>());
}

const Type* TypeConstructor::lookup(const std::vector<const Type*>& parameters)
{
    auto i = instantiations_.find(parameters);
    if (i != instantiations_.end())
    {
        return i->second.get();
    }

    return nullptr;
}

const Type* TypeConstructor::instantiate()
{
    return instantiate(std::vector<const Type*>());
}

const Type* TypeConstructor::instantiate(const std::vector<const Type*>& parameters)
{
    assert(lookup(parameters) == nullptr);

    // Create a new type from the template
    Type* newType = new Type(this, parameters);

    // And create concrete constructors for each generic value constructor
    assert(parameters.size() == parameters_.size());
    std::unordered_map<std::string, const Type*> context;
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        context[parameters_[i]] = parameters[i];
    }

    for (auto& genericConstructor : valueConstructors_)
    {
        std::vector<const Type*> constructorMembers;
        for (const std::unique_ptr<TypeName>& member : genericConstructor->members())
        {
            constructorMembers.push_back(table_->lookup(member.get(), context));
        }

        newType->addValueConstructor(new ValueConstructor(genericConstructor->name(), constructorMembers));
    }

    instantiations_.emplace(std::make_pair(parameters, std::unique_ptr<Type>(newType)));

    return newType;
}

ValueConstructor::ValueConstructor(const std::string& name, const std::vector<const Type*>& members)
: name_(name), members_(members)
{
    // First pass -> just count the number of boxed / unboxed members
    for (size_t i = 0; i < members.size(); ++i)
    {
        if (members[i]->isSimple())
        {
            ++unboxedMembers_;
        }
        else
        {
            ++boxedMembers_;
        }
    }

    // Second pass -> determine the actual layout
    size_t nextBoxed = 0, nextUnboxed = boxedMembers_;
    for (size_t i = 0; i < members.size(); ++i)
    {
        if (members[i]->isSimple())
        {
            memberLocations_.push_back(nextUnboxed++);
        }
        else
        {
            memberLocations_.push_back(nextBoxed++);
        }
    }
}
