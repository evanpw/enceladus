#include "semantic/types.hpp"

#include "ast/ast.hpp"
#include "lib/library.h"
#include "utility.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

void Type::assign(Type* rhs)
{
    assert(isVariable());
    get<TypeVariable>()->assign(rhs);
}

void TypeVariable::assign(Type* rhs)
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
        _members.emplace_back(memberName, type, i);
    }
}

// Two (possibly polymorphic) types are compatible iff there is at least one
// monomorphic type which unifies with both
bool isCompatible(Type* lhs, Type* rhs)
{
    std::unordered_map<TypeVariable*, Type*> context;
    return isCompatible(lhs, rhs, context);
}

bool isCompatible(Type* lhs, Type* rhs, std::unordered_map<TypeVariable*, Type*>& context)
{
    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        return lhs->equals(rhs);
    }
    else if (lhs->tag() == ttVariable)
    {
        if (rhs->tag() != ttVariable || !lhs->equals(rhs))
        {
            context[lhs->get<TypeVariable>()] = rhs;
        }

        return true;
    }
    else if (rhs->tag() == ttVariable)
    {
        context[rhs->get<TypeVariable>()] = lhs;
        return true;
    }
    else if (lhs->tag() == ttFunction && rhs->tag() == ttFunction)
    {
        FunctionType* lhsFunction = lhs->get<FunctionType>();
        FunctionType* rhsFunction = rhs->get<FunctionType>();

        if (lhsFunction->inputs().size() != rhsFunction->inputs().size())
            return false;

        for (size_t i = 0; i < lhsFunction->inputs().size(); ++i)
        {
            Type* leftParam = lhsFunction->inputs().at(i);
            Type* rightParam = rhsFunction->inputs().at(i);

            if (!isCompatible(leftParam, rightParam, context))
                return false;
        }

        if (!isCompatible(lhsFunction->output(), rhsFunction->output(), context))
            return false;

        return true;
    }
    else if (lhs->tag() == ttConstructed && rhs->tag() == ttConstructed)
    {
        ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
        ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

        if (lhsConstructed->typeConstructor() != rhsConstructed->typeConstructor())
            return false;

        assert(lhsConstructed->typeParameters().size() == rhsConstructed->typeParameters().size());

        for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
        {
            Type* leftParam = lhsConstructed->typeParameters().at(i);
            Type* rightParam = rhsConstructed->typeParameters().at(i);

            if (!isCompatible(leftParam, rightParam, context))
                return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}