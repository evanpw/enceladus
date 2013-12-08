#include "types.hpp"

#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

TypeTable::TypeTable()
{
    baseTypes_["Int"] = BaseType::create("Int", true);
    baseTypes_["Bool"] = BaseType::create("Bool", true);
    baseTypes_["Unit"] = BaseType::create("Unit", true);
    baseTypes_["Tree"] = BaseType::create("Tree", false);

    typeConstructors_["List"] = std::unique_ptr<TypeConstructor>(new TypeConstructor("List", 1));
}

const TypeConstructor* TypeTable::getTypeConstructor(const std::string& name)
{
    auto i = typeConstructors_.find(name);
    if (i == typeConstructors_.end())
    {
        std::cerr << "No type constructor " << name << std::endl;
        return nullptr;
    }
    else
    {
        return i->second.get();
    }
}

std::shared_ptr<Type> TypeTable::getBaseType(const std::string& name)
{
    auto i = baseTypes_.find(name);
    if (i == baseTypes_.end())
    {
        std::cerr << "No primitive type " << name << std::endl;
        return std::shared_ptr<Type>();
    }
    else
    {
        return i->second;
    }
}

void TypeTable::insert(const std::string& name, TypeConstructor* typeConstructor)
{
    typeConstructors_.emplace(std::make_pair(name, std::unique_ptr<TypeConstructor>(typeConstructor)));
}

void TypeTable::insert(const std::string& name, std::shared_ptr<Type> baseType)
{
    baseTypes_.emplace(std::make_pair(name, baseType));
}

std::shared_ptr<Type> TypeTable::nameToType(const TypeName* typeName)
{
    if (typeName->parameters().empty())
    {
        return getBaseType(typeName->name());
    }
    else
    {
        const TypeConstructor* typeConstructor = getTypeConstructor(typeName->name());

        std::vector<std::shared_ptr<Type>> typeParameters;
        for (const std::unique_ptr<TypeName>& parameter : typeName->parameters())
        {
            std::shared_ptr<Type> parameterType = nameToType(parameter.get());
            if (!parameterType) return std::shared_ptr<Type>();

            typeParameters.push_back(parameterType);
        }

        return ConstructedType::create(typeConstructor, typeParameters);
    }
}

TypeTag Type::tag() const
{
    return _impl->tag();
}

std::string Type::name() const
{
    return _impl->name();
}

bool Type::isBoxed() const
{
    return _impl->isBoxed();
}

const std::vector<std::unique_ptr<ValueConstructor>>& Type::valueConstructors() const
{
    return _impl->valueConstructors();
}

bool equals(const std::shared_ptr<Type>& lhs, const std::shared_ptr<Type>& rhs)
{
    // Two base types can be unified only if equal (we don't have inheritance)
    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        return lhs->name() == rhs->name();
    }
    else if (lhs->tag() == ttVariable && rhs->tag() == ttVariable)
    {
        return lhs->get<TypeVariable>() == rhs->get<TypeVariable>();
    }
    else if (lhs->tag() == ttFunction && rhs->tag() == ttFunction)
    {
        FunctionType* lhsFunction = lhs->get<FunctionType>();
        FunctionType* rhsFunction = rhs->get<FunctionType>();

        return equals(lhsFunction->domain(), rhsFunction->domain()) &&
               equals(lhsFunction->range(), rhsFunction->range());
    }
    else if (lhs->tag() == ttConstructed && rhs->tag() == ttConstructed)
    {
        ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
        ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

        if (lhsConstructed->typeConstructor() != rhsConstructed->typeConstructor())
        {
            return false;
        }

        assert(lhsConstructed->typeParameters().size() == rhsConstructed->typeParameters().size());

        for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
        {
            if (!equals(lhsConstructed->typeParameters().at(i), rhsConstructed->typeParameters().at(i)))
                return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}

void Type::addValueConstructor(ValueConstructor* valueConstructor)
{
    _impl->addValueConstructor(valueConstructor);
}

std::set<TypeVariable*> Type::freeVars() const
{
    std::set<TypeVariable*> result;

    switch (_impl->tag())
    {
        case ttBase:
            break;

        case ttVariable:
        {
            result.insert(get<TypeVariable>());
            break;
        }

        case ttFunction:
        {
            FunctionType* functionType = get<FunctionType>();
            result += functionType->domain()->freeVars();
            result += functionType->range()->freeVars();
            break;
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = get<ConstructedType>();
            for (const std::shared_ptr<Type>& parameter : constructedType->typeParameters())
            {
                result += parameter->freeVars();
            }

            break;
        }
    }

    return result;
}

std::string TypeScheme::name() const
{
    std::stringstream ss;
    if (_quantified.size() > 0)
    {
        ss << "forall ";
        for (auto& typeVar : _quantified)
        {
            ss << typeVar->name() << " ";
        }
        ss << ". ";
    }

    ss << _type->name();

    return ss.str();
}

std::set<TypeVariable*> TypeScheme::freeVars() const
{
    std::set<TypeVariable*> allVars = _type->freeVars();
    for (TypeVariable* boundVar : _quantified)
    {
        allVars.erase(boundVar);
    }

    return allVars;
}

std::string FunctionType::name() const
{
    std::stringstream ss;
    if (_domain->tag() == ttFunction)
    {
        ss << "(" << _domain->name() << ")";
    }
    else
    {
        ss << _domain->name();
    }

    ss << " -> " << _range->name();
    return ss.str();
}

std::string ConstructedType::name() const
{
    std::stringstream ss;

    if (_typeConstructor->name() == "List")
    {
        assert(_typeParameters.size() == 1);
        ss << "[" << _typeParameters[0]->name() << "]";
    }
    else
    {
        ss << _typeConstructor;
        for (const std::shared_ptr<Type>& type : _typeParameters)
        {
            ss << " " << type->name();
        }
    }

    return ss.str();
}

int TypeVariable::_count;

std::string TypeVariable::name() const
{
    std::stringstream ss;
    ss << "a" << _index;
    return ss.str();
}

ValueConstructor::ValueConstructor(const std::string& name, const std::vector<std::shared_ptr<Type>>& members)
: name_(name), members_(members)
{
    // First pass -> just count the number of boxed / unboxed members
    for (size_t i = 0; i < members.size(); ++i)
    {
        if (members[i]->isBoxed())
        {
            ++boxedMembers_;
        }
        else
        {
            ++unboxedMembers_;
        }
    }

    // Second pass -> determine the actual layout
    size_t nextBoxed = 0, nextUnboxed = boxedMembers_;
    for (size_t i = 0; i < members.size(); ++i)
    {
        if (members[i]->isBoxed())
        {
            memberLocations_.push_back(nextBoxed++);
        }
        else
        {
            memberLocations_.push_back(nextUnboxed++);
        }
    }
}
