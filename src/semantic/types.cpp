#include "semantic/types.hpp"

#include "ast/ast.hpp"
#include "lib/library.h"
#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>
#include <unordered_set>

static std::string toString(const Trait* trait, std::unordered_set<const Type*>& varStack);

static std::string toString(const Type* type, std::unordered_set<const Type*>& varStack)
{
    switch (type->tag())
    {
        case ttBase:
            return type->get<BaseType>()->name();

        case ttFunction:
        {
            FunctionType* functionType = type->get<FunctionType>();

            std::stringstream ss;

            if (functionType->inputs().empty())
            {
                ss << "||";
            }
            else
            {
                ss << "|";

                bool first = true;
                for (Type* input : functionType->inputs())
                {
                    if (!first) ss << ", ";
                    ss << toString(input, varStack);
                    first = false;
                }

                ss << "|";
            }

            ss << " -> " << toString(functionType->output(), varStack);

            return ss.str();
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = type->get<ConstructedType>();

            std::stringstream ss;

            if (constructedType->name() == "List")
            {
                assert(constructedType->typeParameters().size() == 1);
                ss << "[" << toString(constructedType->typeParameters()[0]) << "]";
            }
            else
            {
                ss << constructedType->name();
                ss << "<";

                bool first = true;
                for (const Type* param : constructedType->typeParameters())
                {
                    if (!first) ss << ", ";
                    ss << toString(param, varStack);
                    first = false;
                }
                ss << ">";
            }

            return ss.str();
        }

        case ttVariable:
        {
            TypeVariable* var = type->get<TypeVariable>();

            if (var->constraints().empty())
            {
                return var->name();
            }
            else
            {
                std::stringstream ss;
                ss << var->name();

                if (varStack.find(type) == varStack.end())
                {
                    varStack.emplace(type);

                    ss << ": ";

                    bool first = true;
                    for (const Trait* constraint : var->constraints())
                    {
                        if (!first) ss << " + ";
                        ss << toString(constraint, varStack);
                        first = false;
                    }

                    varStack.erase(type);
                }

                return ss.str();
            }
        }
    }

    assert(false);
}

std::string toString(const Type* type)
{
    std::unordered_set<const Type*> varStack;
    return toString(type, varStack);
}

static std::string toString(const Trait* trait, std::unordered_set<const Type*>& varStack)
{
    std::stringstream ss;

    ss << trait->name();
    if (!trait->parameters().empty())
    {
        ss << "<";

        bool first = true;
        for (Type* param : trait->parameters())
        {
            if (!first) ss << ", ";
            ss << toString(param, varStack);
            first = false;
        }

        ss << ">";
    }

    return ss.str();
}

std::string toString(const Trait* trait)
{
    std::unordered_set<const Type*> varStack;
    return toString(trait, varStack);
}

std::string TypeVariable::name() const
{
    std::stringstream ss;

    if (!_name.empty())
    {
        ss << _name;
    }
    else
    {
        if (!_quantified)
            ss << "'";

        ss << "T" << _index;
    }

    return ss.str();
}

void TypeVariable::assign(const Type* rhs)
{
    assert(!_quantified);
    assert(!_references.empty());

    // This is needed to keep myself alive until the end of this function,
    // because the for-loop below will reduce the reference count down to zero
    std::shared_ptr<TypeImpl> keepAlive = _references[0]->_impl;

    for (Type* reference : _references)
    {
        reference->_impl = rhs->_impl;
        rhs->_impl->addReference(reference);
    }

    _references.clear();
}

TypeTable::TypeTable()
{
    Int = createBaseType("Int", true, 64, true);
    UInt = createBaseType("UInt", true, 64, false);
    UInt8 = createBaseType("UInt8", true, 8, false);

    Num = createTrait("Num");
    Num->addInstance(Int);
    Num->addInstance(UInt);
    Num->addInstance(UInt8);

    Bool = createBaseType("Bool", true);
    Unit = createBaseType("Unit", true);

    Function = createConstructedType("Function", {createTypeVariable("T", true)});
    Array = createConstructedType("Array", {createTypeVariable("T", true)});
}

std::pair<size_t, ValueConstructor*> TypeImpl::getValueConstructor(const std::string& name) const
{
    for (size_t i = 0; i < _valueConstructors.size(); ++i)
    {
        if (_valueConstructors[i]->str() == name)
        {
            return std::make_pair(i, _valueConstructors[i]);
        }
    }

    return std::make_pair(0, nullptr);
}

ConstructedType::ConstructedType(TypeTable* table, const std::string& name, std::vector<Type*>&& typeParameters, const ConstructedType* prototype)
: TypeImpl(table, ttConstructed), _name(name), _typeParameters(typeParameters), _prototype(prototype)
{
    if (!_prototype)
    {
        _prototype = this;
    }
}

Type* ConstructedType::instantiate(std::vector<Type*>&& typeParameters) const
{
    // Can only instantiate prototypical types
    assert(_prototype == this);

    return _table->createConstructedType(_name, std::move(typeParameters), this);
}

// Passed through the prototype
const std::vector<ValueConstructor*>& ConstructedType::valueConstructors() const
{
    if (_prototype == this)
    {
        return TypeImpl::valueConstructors();
    }
    else
    {
        return _prototype->valueConstructors();
    }
}

std::pair<size_t, ValueConstructor*> ConstructedType::getValueConstructor(const std::string& name) const
{
    if (_prototype == this)
    {
        return TypeImpl::getValueConstructor(name);
    }
    else
    {
        return _prototype->getValueConstructor(name);
    }
}

void ConstructedType::addValueConstructor(ValueConstructor* valueConstructor)
{
    assert(_prototype == this);
    TypeImpl::addValueConstructor(valueConstructor);
}

int TypeVariable::_count;

ValueConstructor::ValueConstructor(const std::string& name,
                                   size_t constructorTag,
                                   const std::vector<Type*>& memberTypes,
                                   const std::vector<std::string>& memberNames)
: _name(name)
, _constructorTag(constructorTag)
{
    assert(memberNames.empty() || (memberNames.size() == memberTypes.size()));

    for (size_t i = 0; i < memberTypes.size(); ++i)
    {
        std::string memberName = memberNames.empty() ? "" : memberNames[i];
        Type* type = memberTypes[i];
        _members.emplace_back(memberName, type);
    }
}

Trait* Trait::instantiate(std::vector<Type*>&& params)
{
    // Can only instantiate prototypical types
    assert(_prototype == this);

    return _table->createTrait(_name, std::move(params), this);
}
