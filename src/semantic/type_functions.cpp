#include "semantic/type_functions.hpp"
#include "semantic/subtype.hpp"

const Trait* instantiate(const Trait* trait)
{
    // TODO: Can we just check trait->prototype() == trait to see if it needs
    // instantiating?

    bool anyChanged = false;

    std::vector<Type*> params;
    for (Type* param : trait->parameters())
    {
        if (param->isVariable() && param->get<TypeVariable>()->quantified())
        {
            params.push_back(trait->table()->createTypeVariable());
            anyChanged = true;
        }
        else
        {
            params.push_back(param);
        }
    }

    const Trait* result;
    if (anyChanged)
    {
        result = trait->instantiate(std::move(params));
    }
    else
    {
        result = trait;
    }

    return result;
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
                    for (const Trait* constraint : typeVariable->constraints())
                    {
                        // TODO: Do we need to check replacements?
                        newVar->addConstraint(instantiate(constraint));
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
    for (const Trait::Instance& instance : trait->instances())
    {
        // TODO: Allow one type to be an instance of multiple instantiations of a trait

        if (overlap(type, instance.type))
            return true;
    }

    return false;
}

std::pair<bool, std::string> bindVariable(TypeVariable* lhs, const Type* value)
{

    // Check to see if the value is actually the same type variable, and don't
    // rebind
    if (value->tag() == ttVariable)
    {
        TypeVariable* rhs = value->get<TypeVariable>();
        if (lhs == rhs)
            return {true, ""};

        std::vector<const Trait*> missingConstraints;

        auto& rhsConstraints = rhs->constraints();

        for (const Trait* constraint : lhs->constraints())
        {
            bool good = false;

            for (const Trait* rhsConstraint : rhsConstraints)
            {

                if (isSubtype(constraint, rhsConstraint))
                {
                    good = true;
                    break;
                }
            }


            if (!good)
            {
                // A quantified type variable can't acquire any new constraints in the
                // process of unification (see overrideType test)
                if (rhs->quantified())
                {
                    std::stringstream ss;
                    ss << "Can't bind variable " << value->str()
                       << " to quantified type variable " << lhs->str()
                       << ", because the latter isn't constrained by trait " << constraint->str();

                    return {false, ss.str()};
                }
                else
                {
                    missingConstraints.push_back(constraint);
                }
            }
        }

        for (const Trait* missing : missingConstraints)
        {
            assert(!rhs->quantified());
            rhs->addConstraint(missing);
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
                ss << "Can't bind variable " << lhs->str() << " to type " << value->str()
                   << " because it isn't an instance of trait " << constraint->str();

                return {false, ss.str()};
            }
        }

        if (occurs(lhs, value))
        {
            std::stringstream ss;
            ss << "variable " << lhs->str() << " already occurs in " << value->str();

            return {false, ss.str()};
        }
    }


    lhs->assign(value);
    return {true, ""};
}

std::pair<bool, std::string> bindVariable(Type* variable, const Type* value)
{
    assert(variable->tag() == ttVariable);
    TypeVariable* lhs = variable->get<TypeVariable>();

    return bindVariable(lhs, value);
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

bool occurs(const TypeVariable* variable, const Type* value)
{
    const Type* rhs = value;

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

static const Type* lookup(const Type* type, const std::unordered_map<TypeVariable*, const Type*>& context)
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

bool overlap(const Type *lhs, const Type* rhs, std::unordered_map<TypeVariable*, const Type*>& subs)
{
    lhs = lookup(lhs, subs);
    rhs = lookup(rhs, subs);

    if (lhs->isVariable())
    {
        assert(lhs->get<TypeVariable>()->quantified());

        if (equals(lhs, rhs))
            return true;

        TypeVariable* lhsVariable = lhs->get<TypeVariable>();

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
    std::unordered_map<TypeVariable*, const Type*> subs;
    return overlap(lhs, rhs, subs);
}