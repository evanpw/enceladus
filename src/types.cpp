#include "types.hpp"
#include "ast.hpp"

#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

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

const std::vector<std::shared_ptr<ValueConstructor>>& Type::valueConstructors() const
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

std::string TypeVariable::name() const
{
    std::stringstream ss;
    ss << "a" << _index;
    return ss.str();
}

ValueConstructor::ValueConstructor(const std::string& name, const std::vector<std::shared_ptr<Type>>& memberTypes, const std::vector<std::string>& memberNames)
: name_(name)
{
    assert(memberNames.empty() || (memberNames.size() == memberTypes.size()));

    for (size_t i = 0; i < memberTypes.size(); ++i)
    {
        std::string memberName = memberNames.empty() ? "_" : memberNames[i];
        members_.emplace_back(memberName, memberTypes[i], i);
    }
}
