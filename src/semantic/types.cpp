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

std::string TypeVariable::str() const
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
            ss << constraint->str();
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

std::string FunctionType::str() const
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
            ss << _inputs[i]->str();

            if (i + 1 < _inputs.size())
                ss << ", ";
        }

        ss << "|";
    }

    ss << " -> " << _output->str();

    return ss.str();
}

ConstructedType::ConstructedType(TypeTable* table, const std::string& name, std::vector<Type*>&& typeParameters)
: TypeImpl(table, ttConstructed), _name(name), _typeParameters(typeParameters)
{
}

std::string ConstructedType::str() const
{
    std::stringstream ss;

    if (_name == "List")
    {
        assert(_typeParameters.size() == 1);
        ss << "[" << _typeParameters[0]->str() << "]";
    }
    else
    {
        ss << _name;
        ss << "<";

        bool first = true;
        for (const Type* type : _typeParameters)
        {
            if (!first) ss << ", ";
            ss << type->str();
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
    if (type->isVariable())
    {
        // TODO: Is it possible to create a completely generic trait
        // implementation?

        TypeVariable* var = type->get<TypeVariable>();
        return var->constraints().count(trait) > 0;
    }

    for (Type* instance : trait->instances())
    {
        if (isCompatible(type, instance))
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
            if (lhsVar->quantified())
                return false;

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
            TypeVariable* rhsVar = rhs->get<TypeVariable>();

            if (lhsVar->quantified() && rhsVar->quantified())
                return false;

            // Quantified variables can't be substituted, so they can unify
            // only with something that has no more constraints
            if (lhsVar->quantified())
            {
                for (Trait* constraint : rhsVar->constraints())
                {
                    if (lhsVar->constraints().find(constraint) == lhsVar->constraints().end())
                        return false;
                }
            }

            if (rhsVar->quantified())
            {
                for (Trait* constraint : lhsVar->constraints())
                {
                    if (rhsVar->constraints().find(constraint) == rhsVar->constraints().end())
                        return false;
                }
            }

            // Combining variables means that the rhs inherits the constraints
            // of the lhs
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

        if (lhsConstructed->name() != rhsConstructed->name())
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

Type* instantiate(Type* type, std::map<TypeVariable*, Type*>& replacements)
{
    switch (type->tag())
    {
        case ttBase:
            return type;

        case ttVariable:
        {
            TypeVariable* typeVariable = type->get<TypeVariable>();

            auto i = replacements.find(typeVariable);
            if (i != replacements.end())
            {
                return i->second;
            }
            else
            {
                if (typeVariable->quantified())
                {
                    Type* replacement = typeVariable->table()->createTypeVariable();

                    // Inherit type constraints
                    TypeVariable* newVar = replacement->get<TypeVariable>();
                    for (Trait* constraint : typeVariable->constraints())
                    {
                        newVar->addConstraint(constraint);
                    }

                    replacements[typeVariable] = replacement;

                    return replacement;
                }
                else
                {
                    return type;
                }
            }
        }

        case ttFunction:
        {
            FunctionType* functionType = type->get<FunctionType>();

            std::vector<Type*> newInputs;
            for (Type* input : functionType->inputs())
            {
                newInputs.push_back(instantiate(input, replacements));
            }

            return type->table()->createFunctionType(newInputs, instantiate(functionType->output(), replacements));
        }

        case ttConstructed:
        {
            std::vector<Type*> params;

            ConstructedType* constructedType = type->get<ConstructedType>();
            for (Type* parameter : constructedType->typeParameters())
            {
                params.push_back(instantiate(parameter, replacements));
            }

            Type* result = type->table()->createConstructedType(constructedType->name(), std::move(params));

            // Inherit value constructors
            for (ValueConstructor* constructor : constructedType->valueConstructors())
            {
                result->addValueConstructor(constructor);
            }

            return result;
        }
    }

    assert(false);
    return nullptr;
}

Type* instantiate(Type* type)
{
    std::map<TypeVariable*, Type*> replacements;
    return instantiate(type, replacements);
}