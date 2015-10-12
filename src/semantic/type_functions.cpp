#include "type_functions.hpp"

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
                    for (const Trait* constraint : typeVariable->constraints())
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

            return constructedType->prototype()->instantiate(std::move(params));
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

bool hasOverlappingInstance(Trait* trait, Type* type)
{
    for (Type* instance : trait->instances())
    {
        if (overlap(type, instance))
            return true;
    }

    return false;
}

std::pair<bool, std::string> bindVariable(Type* variable, Type* value)
{
    assert(variable->tag() == ttVariable);
    TypeVariable* lhs = variable->get<TypeVariable>();

    // Check to see if the value is actually the same type variable, and don't
    // rebind
    if (value->tag() == ttVariable)
    {
        TypeVariable* rhs = value->get<TypeVariable>();
        if (lhs == rhs)
            return {true, ""};

        // A quantified type variable can't acquire any new constraints in the
        // process of unification (see overrideType test)
        if (rhs->quantified())
        {
            auto& rhsConstraints = rhs->constraints();

            for (const Trait* constraint : lhs->constraints())
            {
                if (rhsConstraints.find(constraint) == rhsConstraints.end())
                {
                    std::stringstream ss;
                    ss << "Can't bind variable " << value->str()
                       << " to quantified type variable " << variable->str()
                       << ", because the latter isn't constrained by trait " << constraint->str();

                    return {false, ss.str()};
                }
            }
        }
    }
    else
    {
        // Otherwise, check that the rhs type meets all of the constraints on
        // the lhs variable
        for (const Trait* constraint : lhs->constraints())
        {
            if (!isSubtype(value, constraint))
            {
                std::stringstream ss;
                ss << "Can't bind variable " << variable->str() << " to type " << value->str()
                   << " because it isn't an instance of trait " << constraint->str();

                return {false, ss.str()};
            }
        }

        if (occurs(lhs, value))
        {
            std::stringstream ss;
            ss << "variable " << variable->str() << " already occurs in " << value->str();

            return {false, ss.str()};
        }
    }

    variable->assign(value);
    return {true, ""};
}

std::pair<bool, std::string> tryUnify(Type* lhs, Type* rhs)
{
    assert(lhs && rhs);

    if (lhs->tag() == ttBase && rhs->tag() == ttBase)
    {
        // Two base types can be unified only if equal (we don't have inheritance)
        if (lhs->equals(rhs))
            return {true, ""};
    }
    else if (lhs->tag() == ttVariable)
    {
        // Non-quantified type variables can always be bound
        if (!lhs->get<TypeVariable>()->quantified())
        {
            return bindVariable(lhs, rhs);
        }
        else
        {
            // Trying to unify a quantified type variable with a type that is not a
            // variable is always an error
            if (rhs->tag() == ttVariable)
            {
                // A quantified type variable unifies with itself
                if (lhs->equals(rhs))
                {
                    return {true, ""};
                }

                // And non-quantified type variables can be bound to quantified ones
                else if (!rhs->get<TypeVariable>()->quantified())
                {
                    return bindVariable(rhs, lhs);
                }
             }
        }
    }
    else if (rhs->tag() == ttVariable && !rhs->get<TypeVariable>()->quantified())
    {
        return bindVariable(rhs, lhs);
    }
    else if (lhs->tag() == ttFunction && rhs->tag() == ttFunction)
    {
        FunctionType* lhsFunction = lhs->get<FunctionType>();
        FunctionType* rhsFunction = rhs->get<FunctionType>();

        if (lhsFunction->inputs().size() == rhsFunction->inputs().size())
        {
            for (size_t i = 0; i < lhsFunction->inputs().size(); ++i)
            {
                auto result = tryUnify(lhsFunction->inputs().at(i), rhsFunction->inputs().at(i));
                if (!result.first)
                    return result;
            }

            return tryUnify(lhsFunction->output(), rhsFunction->output());
        }
    }
    else if (lhs->tag() == ttConstructed && rhs->tag() == ttConstructed)
    {
        ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
        ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

        if (lhsConstructed->name() == rhsConstructed->name())
        {
            assert(lhsConstructed->typeParameters().size() == rhsConstructed->typeParameters().size());

            for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
            {
                auto result = tryUnify(lhsConstructed->typeParameters().at(i), rhsConstructed->typeParameters().at(i));
                if (!result.first)
                    return result;
            }

            return {true, ""};
        }
    }

    return {false, ""};
}

bool occurs(TypeVariable* variable, Type* value)
{
    Type* rhs = value;

    switch (rhs->tag())
    {
        case ttBase:
            return false;

        case ttVariable:
        {
            TypeVariable* typeVariable = rhs->get<TypeVariable>();
            return typeVariable == variable;
        }

        case ttFunction:
        {
            FunctionType* functionType = rhs->get<FunctionType>();

            for (auto& input : functionType->inputs())
            {
                if (occurs(variable, input)) return true;
            }

            return occurs(variable, functionType->output());
        }

        case ttConstructed:
        {
            ConstructedType* constructedType = rhs->get<ConstructedType>();
            for (Type* parameter : constructedType->typeParameters())
            {
                if (occurs(variable, parameter)) return true;
            }

            return false;
        }
    }

    assert(false);
}


bool isSubtype(const Type* lhs, const Trait* trait)
{
    if (lhs->isVariable())
    {
        const TypeVariable* lhsVariable = lhs->get<TypeVariable>();
        if (!lhsVariable->quantified())
        {
            return true;
        }
        else
        {
            return lhsVariable->constraints().count(trait) > 0;
        }
    }

    for (const Type* instance : trait->instances())
    {
        if (isSubtype(lhs, instance))
            return true;
    }

    return false;
}

bool equals(const Type* lhs, const Type* rhs)
{
    if (lhs->tag() != rhs->tag())
        return false;

    switch (lhs->tag())
    {
        case ttBase:
            return lhs->get<BaseType>() == rhs->get<BaseType>();

        case ttVariable:
            return lhs->get<TypeVariable>() == rhs->get<TypeVariable>();

        case ttFunction:
        {
            const FunctionType* lhsFunction = lhs->get<FunctionType>();
            const FunctionType* rhsFunction = rhs->get<FunctionType>();

            if (lhsFunction->inputs().size() != rhsFunction->inputs().size())
                return false;

            for (size_t i = 0; i < lhsFunction->inputs().size(); ++i)
            {
                if (!equals(lhsFunction->inputs()[i], rhsFunction->inputs()[i]))
                    return false;
            }

            if (!equals(lhsFunction->output(), rhsFunction->output()))
                return false;

            return true;
        }

        case ttConstructed:
        {
            const ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
            const ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

            if (lhsConstructed->prototype() != rhsConstructed->prototype())
                return false;

            for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
            {
                if (!equals(lhsConstructed->typeParameters()[i], rhsConstructed->typeParameters()[i]))
                    return false;
            }

            return true;
        }
    }

    assert(false);
}

bool isSubtype(const Type* lhs, const Type* rhs, std::unordered_map<const TypeVariable*, const Type*>& subs)
{
    if (rhs->isVariable())
    {
        const TypeVariable* rhsVariable = rhs->get<TypeVariable>();

        if (rhsVariable->quantified())
        {
            auto i = subs.find(rhsVariable);
            if (i != subs.end())
            {
                return i->second->equals(lhs);
            }

            for (const Trait* constraint : rhsVariable->constraints())
            {
                if (!isSubtype(lhs, constraint))
                    return false;
            }

            subs[rhsVariable] = lhs;
            return true;
        }
        else
        {
            assert(false);
        }
    }

    // If we get to this point, then rhs is not a variable

    switch (lhs->tag())
    {
        case ttBase:
        {
            if (rhs->tag() != ttBase)
                return false;

            return lhs->get<BaseType>() == rhs->get<BaseType>();
        }

        case ttVariable:
        {
            const TypeVariable* typeVariable = lhs->get<TypeVariable>();

            if (typeVariable->quantified())
            {
                // A quantified type variable cannot be a subtype of anything
                // but a type variable
                return false;
            }
            else
            {
                for (const Trait* constraint : typeVariable->constraints())
                {
                    if (!isSubtype(rhs, constraint))
                        return false;
                }

                return true;
            }
        }

        case ttFunction:
        {
            if (rhs->tag() != ttFunction)
                return false;

            const FunctionType* lhsFunction = lhs->get<FunctionType>();
            const FunctionType* rhsFunction = rhs->get<FunctionType>();

            if (lhsFunction->inputs().size() != rhsFunction->inputs().size())
                return false;

            for (size_t i = 0; i < lhsFunction->inputs().size(); ++i)
            {
                if (!isSubtype(lhsFunction->inputs()[i], rhsFunction->inputs()[i], subs))
                    return false;
            }

            if (!isSubtype(lhsFunction->output(), rhsFunction->output(), subs))
                return false;

            return true;
        }

        case ttConstructed:
        {
            if (rhs->tag() != ttConstructed)
                return false;

            const ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
            const ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

            if (lhsConstructed->prototype() != rhsConstructed->prototype())
                return false;

            for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
            {
                if (!isSubtype(lhsConstructed->typeParameters()[i], rhsConstructed->typeParameters()[i], subs))
                    return false;
            }

            return true;
        }
    }

    assert(false);
}

// Two (possibly polymorphic) types are compatible iff there is at least one
// monomorphic type which unifies with both
bool isSubtype(const Type* lhs, const Type* rhs)
{
    std::unordered_map<const TypeVariable*, const Type*> subs;
    return isSubtype(lhs, rhs, subs);
}

static const Type* lookup(const Type* type, const std::unordered_map<const TypeVariable*, const Type*>& context)
{
    while (type->isVariable())
    {
        const TypeVariable* var = type->get<TypeVariable>();
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

bool overlap(const Type *lhs, const Type* rhs, std::unordered_map<const TypeVariable*, const Type*>& subs)
{
    lhs = lookup(lhs, subs);
    rhs = lookup(rhs, subs);

    if (lhs->isVariable())
    {
        assert(lhs->get<TypeVariable>()->quantified());

        if (equals(lhs, rhs))
            return true;

        const TypeVariable* lhsVariable = lhs->get<TypeVariable>();

        assert(subs.find(lhsVariable) == subs.end());
        subs[lhs->get<TypeVariable>()] = rhs;

        return true;
    }
    else if (rhs->isVariable())
    {
        assert(rhs->get<TypeVariable>()->quantified());

        assert(subs.find(rhs->get<TypeVariable>()) == subs.end());
        subs[rhs->get<TypeVariable>()] = lhs;

        return true;
    }

    if (lhs->tag() != rhs->tag())
        return false;

    switch (lhs->tag())
    {
        case ttBase:
        {
            return lhs->get<BaseType>() == rhs->get<BaseType>();
        }

        case ttFunction:
        {
            const FunctionType* lhsFunction = lhs->get<FunctionType>();
            const FunctionType* rhsFunction = rhs->get<FunctionType>();

            if (lhsFunction->inputs().size() != rhsFunction->inputs().size())
                return false;

            for (size_t i = 0; i < lhsFunction->inputs().size(); ++i)
            {
                if (!overlap(lhsFunction->inputs()[i], rhsFunction->inputs()[i], subs))
                    return false;
            }

            if (!overlap(lhsFunction->output(), rhsFunction->output(), subs))
                return false;

            return true;
        }

        case ttConstructed:
        {
            const ConstructedType* lhsConstructed = lhs->get<ConstructedType>();
            const ConstructedType* rhsConstructed = rhs->get<ConstructedType>();

            if (lhsConstructed->prototype() != rhsConstructed->prototype())
                return false;

            for (size_t i = 0; i < lhsConstructed->typeParameters().size(); ++i)
            {
                if (!overlap(lhsConstructed->typeParameters()[i], rhsConstructed->typeParameters()[i], subs))
                    return false;
            }

            return true;
        }
    }

    assert(false);
}

bool overlap(const Type *lhs, const Type* rhs)
{
    std::unordered_map<const TypeVariable*, const Type*> subs;
    return overlap(lhs, rhs, subs);
}