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

std::string TypeVariable::name() const
{
    std::stringstream ss;

    if (!_name.empty())
    {
        ss << _name;
    }
    else
    {
        ss << "T" << _index;
    }

    if (!_constraints.empty())
    {
        ss << ": ";

        bool first = true;
        for (Trait* constraint : _constraints)
        {
            if (!first) ss << " + ";
            ss << constraint->name();
            first = false;
        }
    }

    return ss.str();
}

void TypeVariable::assign(Type* rhs)
{
    assert(!_quantified);
    assert(!_references.empty());

    // Assume that if assigning to a concrete type that we've already checked
    // all of the constraints.

    // The type variable obtained by unifying two type variables inherits the
    // contraints of both
    if (rhs->isVariable())
    {
        TypeVariable* rhsVariable = rhs->get<TypeVariable>();
        for (Trait* constraint : _constraints)
        {
            rhsVariable->addConstraint(constraint);
        }
    }

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

    Num = createTrait("Num");
    Num->addInstance(Int);
    Num->addInstance(UInt);

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
        ss << "||";
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
    assert(typeParameters.size() == typeConstructor->parameters());

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
        ss << "<";

        bool first = true;
        for (const Type* type : _typeParameters)
        {
            if (!first) ss << ", ";
            ss << type->name();
            first = false;
        }
        ss << ">";
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
        _members.emplace_back(memberName, type);
    }
}

bool isInstance(Type* type, Trait* trait)
{
    // TODO: Relax the exact-match condition
    for (Type* instance : trait->instances())
    {
        if (type->equals(instance))
            return true;
    }

    return false;
}

static Type* lookup(Type* type, const std::unordered_map<TypeVariable*, Type*>& context)
{
    while (type->isVariable())
    {
        TypeVariable* var = type->get<TypeVariable>();
        auto i = context.find(var);
        if (i == context.end())
        {
            return type;
        }
        else
        {
            type = i->second;
        }
    }

    return type;
}

bool isCompatible(Type* lhs, Type* rhs, std::unordered_map<TypeVariable*, Type*>& context, std::unordered_map<TypeVariable*, std::set<Trait*>>& constraints)
{
    lhs = lookup(lhs, context);
    rhs = lookup(rhs, context);

    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        return lhs->equals(rhs);
    }
    else if (lhs->tag() == ttVariable)
    {
        TypeVariable* lhsVar = lhs->get<TypeVariable>();

        if (rhs->tag() != ttVariable)
        {
            for (Trait* constraint : lhsVar->constraints())
            {
                if (!isInstance(rhs, constraint))
                    return false;
            }

            if (constraints.find(lhsVar) != constraints.end())
            {
                for (Trait* constraint : constraints[lhsVar])
                {
                    if (!isInstance(rhs, constraint))
                        return false;
                }
            }

            context[lhsVar] = rhs;
        }
        else if (!lhs->equals(rhs))
        {
            // Combining variables means that the lhs inherits the constraints
            // of the lhs
            TypeVariable* rhsVar = rhs->get<TypeVariable>();
            for (Trait* constraint : lhsVar->constraints())
            {
                constraints[rhsVar].insert(constraint);
            }

            if (constraints.find(lhsVar) != constraints.end())
            {
                for (Trait* constraint : constraints[lhsVar])
                {
                    constraints[rhsVar].insert(constraint);
                }
            }

            context[lhsVar] = rhs;
        }

        return true;
    }
    else if (rhs->tag() == ttVariable)
    {
        // Swap the order and continue
        return isCompatible(rhs, lhs, context, constraints);
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

            if (!isCompatible(leftParam, rightParam, context, constraints))
                return false;
        }

        if (!isCompatible(lhsFunction->output(), rhsFunction->output(), context, constraints))
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

            if (!isCompatible(leftParam, rightParam, context, constraints))
                return false;
        }

        return true;
    }
    else
    {
        return false;
    }
}

// Two (possibly polymorphic) types are compatible iff there is at least one
// monomorphic type which unifies with both
bool isCompatible(Type* lhs, Type* rhs)
{
    std::unordered_map<TypeVariable*, Type*> context;
    std::unordered_map<TypeVariable*, std::set<Trait*>> constraints;
    return isCompatible(lhs, rhs, context, constraints);
}