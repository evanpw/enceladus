#include "types.hpp"
#include "ast.hpp"

#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

std::string TypeName::str() const
{
    std::stringstream ss;

    ss << name_;
    for (auto& param : parameters())
    {
        ss << " " << param->str();
    }

    return ss.str();
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
        case ttStruct:
            break;

        case ttVariable:
        {
            result.insert(get<TypeVariable>());
            break;
        }

        case ttFunction:
        {
            FunctionType* functionType = get<FunctionType>();

            for (auto& input : functionType->inputs())
            {
                result += input->freeVars();
            }
            result += functionType->output()->freeVars();
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

    for (auto& input : _inputs)
    {
        if (input->tag() == ttFunction)
        {
            ss << "(" << input->name() << ")";
        }
        else
        {
            ss << input->name();
        }

        ss << " -> ";
    }

    ss << _output->name();

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
    boxedMembers_ = 0;
    unboxedMembers_ = 0;

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

StructType::StructType(const std::string& name, StructDefNode* node)
: TypeImpl(ttStruct), name_(name)
{
    boxedMembers_ = 0;
    unboxedMembers_ = 0;

    // First pass -> just count the number of boxed / unboxed members
    for (auto& member : *node->members)
    {
        if (member->memberType->isBoxed())
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
    for (auto& member : *node->members)
    {
        if (member->memberType->isBoxed())
        {
            members_[member->name] = { member->memberType, nextBoxed++ };
        }
        else
        {
            members_[member->name] = { member->memberType, nextUnboxed++ };
        }
    }
}
