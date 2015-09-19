#include "semantic/types.hpp"

#include "ast/ast.hpp"
#include "lib/library.h"
#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

TypeTable::TypeTable()
{
    Int = createBaseType("Int", true);
    Bool = createBaseType("Bool", true);
    Unit = createBaseType("Unit", true);
    String = createBaseType("String", false);

    Function = createTypeConstructor("Function", 1);
    Array = createTypeConstructor("Array", 1);
}

std::pair<size_t, ValueConstructor*> TypeImpl::getValueConstructor(const std::string& name) const
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

ConstructedType::ConstructedType(TypeTable* table, TypeConstructor* typeConstructor, std::initializer_list<Type*> typeParameters)
: TypeImpl(table, ttConstructed), _typeConstructor(typeConstructor)
{
    for (Type* parameter : typeParameters)
    {
        _typeParameters.push_back(parameter);
    }

    for (auto& valueConstructor : typeConstructor->valueConstructors())
    {
        addValueConstructor(valueConstructor);
    }
}

ConstructedType::ConstructedType(TypeTable* table, TypeConstructor* typeConstructor, const std::vector<Type*>& typeParameters)
: TypeImpl(table, ttConstructed), _typeConstructor(typeConstructor)
{
    for (Type* parameter : typeParameters)
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
        for (const Type* type : _typeParameters)
        {
            ss << " " << type->name();
        }
    }

    return ss.str();
}

int TypeVariable::_count;

ValueConstructor::ValueConstructor(Symbol* symbol, const std::vector<Type*>& memberTypes,
                                   const std::vector<std::string>& memberNames)
: _symbol(symbol)
{
    assert(memberNames.empty() || (memberNames.size() == memberTypes.size()));

    for (size_t i = 0; i < memberTypes.size(); ++i)
    {
        std::string memberName = memberNames.empty() ? "" : memberNames[i];
        Type* type = memberTypes[i];
        _members.emplace_back(memberName, type, i);
    }
}

std::string ValueConstructor::name() const
{
    return _symbol->name;
}
