#include "types.hpp"

#include "ast.hpp"
#include "lib/library.h"
#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

std::shared_ptr<Type> Type::Int = BaseType::create("Int", true);
std::shared_ptr<Type> Type::Bool = BaseType::create("Bool", true);
std::shared_ptr<Type> Type::Unit = BaseType::create("Unit", true);
std::shared_ptr<Type> Type::String = BaseType::create("String", false, STRING_TAG);

std::shared_ptr<Type> unwrap(const std::shared_ptr<Type>& type)
{
    if (type->isVariable())
    {
        std::shared_ptr<Type> target = type->get<TypeVariable>()->target();
        if (target)
        {
            return unwrap(target);
        }
        else
        {
            return type;
        }
    }
    else
    {
        return type;
    }
}

std::set<TypeVariable*> Type::freeVars()
{
    std::set<TypeVariable*> result;

    switch (_tag)
    {
        case ttBase:
            break;

        case ttVariable:
        {
            TypeVariable* typeVar = dynamic_cast<TypeVariable*>(this);
            std::shared_ptr<Type> target = typeVar->deref();

            if (target)
            {
                result += target->freeVars();
            }
            else
            {
                result.insert(typeVar);
            }
            break;
        }

        case ttFunction:
        {
            FunctionType* functionType = dynamic_cast<FunctionType*>(this);

            for (auto& input : functionType->inputs())
            {
                result += input->freeVars();
            }
            result += functionType->output()->freeVars();
            break;
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = dynamic_cast<ConstructedType*>(this);
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

std::pair<size_t, ValueConstructor*> Type::getValueConstructor(const std::string& name) const
{
    for (size_t i = 0; i < _valueConstructors.size(); ++i)
    {
        if (_valueConstructors[i]->name() == name)
        {
            return std::make_pair(i, _valueConstructors[i]);
        }
    }

    return std::make_pair(0, nullptr);
}

std::string FunctionType::name() const
{
    std::stringstream ss;

    if (_inputs.empty())
    {
        ss << "Unit";
    }
    else if (_inputs.size() == 1)
    {
        ss << _inputs[0]->name();
    }
    else
    {
        ss << "|";

        for (size_t i = 0; i < _inputs.size(); ++i)
        {
            ss << _inputs[i]->name();

            if (i + 1 < _inputs.size())
                ss << ", ";
        }

        ss << "|";
    }

    ss << " -> " << _output->name();

    return ss.str();
}

ConstructedType::ConstructedType(const TypeConstructor* typeConstructor, std::initializer_list<std::shared_ptr<Type>> typeParameters)
: Type(ttConstructed), _typeConstructor(typeConstructor)
{
    for (const std::shared_ptr<Type>& parameter : typeParameters)
    {
        _typeParameters.push_back(parameter);
    }

    for (auto& valueConstructor : typeConstructor->valueConstructors())
    {
        addValueConstructor(valueConstructor);
    }
}

ConstructedType::ConstructedType(const TypeConstructor* typeConstructor, const std::vector<std::shared_ptr<Type>> typeParameters)
: Type(ttConstructed), _typeConstructor(typeConstructor)
{
    for (const std::shared_ptr<Type>& parameter : typeParameters)
    {
        _typeParameters.push_back(parameter);
    }

    for (auto& valueConstructor : typeConstructor->valueConstructors())
    {
        addValueConstructor(valueConstructor);
    }
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
        ss << _typeConstructor->name();
        for (const std::shared_ptr<Type>& type : _typeParameters)
        {
            ss << " " << type->name();
        }
    }

    return ss.str();
}

int TypeVariable::_count;

ValueConstructor::ValueConstructor(const std::string& name, const std::vector<std::shared_ptr<Type>>& memberTypes, const std::vector<std::string>& memberNames)
: _name(name)
{
    assert(memberNames.empty() || (memberNames.size() == memberTypes.size()));

    for (size_t i = 0; i < memberTypes.size(); ++i)
    {
        std::string memberName = memberNames.empty() ? "_" : memberNames[i];
        std::shared_ptr<Type> type = memberTypes[i];
        _members.emplace_back(memberName, type, i);
    }
}
