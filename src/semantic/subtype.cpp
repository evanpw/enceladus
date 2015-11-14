#include "semantic/subtype.hpp"
#include "semantic/type_functions.hpp"

Type* TypeComparer::lookup(Type* type, const TypeAssignment& context)
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

Type* TypeComparer::lookupLeft(Type* type)
{
    return lookup(type, _lhsSubs);
}

Type* TypeComparer::lookupRight(Type* type)
{
    return lookup(type, _rhsSubs);
}

Type* TypeComparer::lookupBoth(Type* type)
{
    return lookupLeft(lookupRight(type));
}

std::set<Trait*> TypeComparer::getConstraints(TypeVariable* var)
{
    std::set<Trait*> result = var->constraints();

    auto i = _newConstraints.find(var);
    if (i != _newConstraints.end())
    {
        for (Trait* trait : i->second)
        {
            result.insert(trait);
        }
    }

    return result;
}

TypeComparer::Transaction::Transaction(TypeComparer* comparer)
: _comparer(comparer)
{
    // Copy current state
    _lhsSubs = _comparer->_lhsSubs;
    _rhsSubs = _comparer->_rhsSubs;
    _newConstraints = _comparer->_newConstraints;
}

void TypeComparer::Transaction::rollback()
{
    assert(!_accepted);

    _comparer->_lhsSubs = _lhsSubs;
    _comparer->_rhsSubs = _rhsSubs;
    _comparer->_newConstraints = _newConstraints;

    // This transaction has ended, so don't do another rollback at destruction
    _accepted = true;
}

TypeComparer::Transaction::~Transaction()
{
    if (!_accepted)
    {
        rollback();
    }
}

bool isSubtype(Trait* lhs, Trait* rhs)
{
    TypeComparer comparer;
    return comparer.compare(lhs, rhs);
}

bool isSubtype(Type* lhs, Trait* trait)
{
    TypeComparer comparer;
    return comparer.compare(lhs, trait);
}

bool isSubtype(Type* lhs, Type* rhs)
{
    TypeComparer comparer;
    return comparer.compare(lhs, rhs);
}

bool overlap(Type* lhs, Type* rhs)
{
    TypeComparer comparer;
    return comparer.overlap(lhs, rhs);
}

bool isSubtype(TypeVariable* lhs, Trait* trait)
{
    // Slightly hacky way to get a Type which refers to lhs
    Type* lhsType = lhs->references().at(0);
    return isSubtype(lhsType, trait);
}

//// Trait <= Trait ////////////////////////////////////////////////////////////

bool TypeComparer::compare(Trait* lhs, Trait* rhs)
{
    //std::cerr << "compareTraitTrait: " << lhs->str() << ", " << rhs->str() << std::endl;

    if (lhs->prototype() != rhs->prototype())
        return false;

    assert(lhs->parameters().size() == rhs->parameters().size());

    for (size_t i = 0; i < lhs->parameters().size(); ++i)
    {
        if (!compare(lhs->parameters()[i], rhs->parameters()[i]))
            return false;
    }

    return true;
}


//// Type <= Trait /////////////////////////////////////////////////////////////

bool TypeComparer::compare(Type* lhs, Trait* trait)
{
    //std::cerr << "compareTypeTrait: " << lhs->str() << ", " << trait->str() << std::endl;

    lhs = lookupBoth(lhs);

    Transaction transaction(this);

    if (lhs->isVariable())
    {
        TypeVariable* lhsVariable = lhs->get<TypeVariable>();

        if (!lhsVariable->quantified())
        {
            _newConstraints[lhsVariable].insert(trait);
            transaction.accept();
            return true;
        }
        else
        {
            // Case 1: T: Trait1 <= Trait1 directly
            for (Trait* constraint : getConstraints(lhsVariable))
            {
                if (compare(constraint, trait))
                {
                    transaction.accept();
                    return true;
                }
            }

            // Case 2: impl<T: Trait1> Trait2 for T makes T: Trait1 <= Trait2,
            // just like with a concrete type. Fall through for that check
        }
    }

    for (const Trait::Instance& instance : trait->instances())
    {
        Transaction innerTransaction(this);

        if (compare(lhs, instance.type))
        {
            assert(instance.traitParams.size() == trait->parameters().size());

            bool good = true;
            for (size_t i = 0; i < instance.traitParams.size(); ++i)
            {
                Type* x = instance.traitParams[i];
                Type* y = trait->parameters()[i];

                //if (compare(trait->parameters()[i], instance.traitParams[i]))
                if (!compare(instance.traitParams[i], trait->parameters()[i]))
                {
                    good = false;
                    break;
                }
            }

            if (good)
            {
                innerTransaction.accept();
                transaction.accept();
                return true;
            }
        }
    }

    return false;
}


//// Type <= Type //////////////////////////////////////////////////////////////

bool TypeComparer::compare(TypeVariable* lhs, Type* rhs)
{
    //std::cerr << "compareVariableType: " << toString(lhs) << ", " << rhs->str() << std::endl;

    Transaction transaction(this);
    assert(!lhs->quantified());

    if (!rhs->isVariable())
    {
        for (Trait* constraint : getConstraints(lhs))
        {
            if (!compare(rhs, constraint))
                return false;
        }

        auto i = _lhsSubs.find(lhs);
        if (i == _lhsSubs.end())
        {
            _lhsSubs[lhs] = rhs;
            transaction.accept();
            return true;
        }
        else
        {
            Type* newValue = i->second;

            if (compare(rhs, newValue))
            {
                // New assignment is more specific
                _lhsSubs[lhs] = rhs;
                transaction.accept();
                return true;
            }
            else if (compare(newValue, rhs))
            {
                // No new substitution needed
                transaction.accept();
                return true;
            }
            else
            {
                // Incompatible assignment
                return false;
            }
        }
    }
    else
    {
        // If rhs is a variable, then don't make a substitution, but inherit
        // all constraints
        for (Trait* rhsConstraint : rhs->get<TypeVariable>()->constraints())
        {
            // Don't worry about redundant constraints: they won't be externally
            // visible anyway
            _newConstraints[lhs].insert(rhsConstraint);
        }

        transaction.accept();
        return true;
    }
}

bool TypeComparer::compare(Type* lhs, TypeVariable* rhs)
{
    //std::cerr << "compareTypeVariable: " << lhs->str() << ", " << toString(rhs) << std::endl;

    Transaction transaction(this);

    //assert(rhs->quantified());

    auto i = _rhsSubs.find(rhs);
    if (i != _rhsSubs.end())
    {
        if (i->second->equals(lhs))
        {
            transaction.accept();
            return true;
        }
        else
        {
            // One possible saving throw: if either the new or the old
            // assignment are unquantified type variables, we may be able
            // to substitute

            if (i->second->isVariable())
            {
                TypeVariable* var = i->second->get<TypeVariable>();

                if (!var->quantified())
                {
                    if (compare(var, lhs))
                    {
                        transaction.accept();
                        return true;
                    }
                }
            }

            if (lhs->isVariable())
            {
                TypeVariable* var = lhs->get<TypeVariable>();

                if (!var->quantified())
                {
                    if (compare(var, i->second))
                    {
                        transaction.accept();
                        return true;
                    }
                }
            }

            return false;
        }
    }

    for (Trait* constraint : rhs->constraints())
    {
        if (!compare(lhs, constraint))
            return false;
    }

    _rhsSubs[rhs] = lhs;
    transaction.accept();

    //std::cerr << "compareTypeVariable: " << lhs->str() << ", " << toString(rhs) << " = true" << std::endl;
    return true;
}

bool TypeComparer::compare(Type* lhs, Type* rhs)
{
    //std::cerr << "compareTypeType: " << lhs->str() << ", " << rhs->str() << std::endl;

    Transaction transaction(this);

    if (rhs->isVariable())
    {
        bool result = compare(lhs, rhs->get<TypeVariable>());
        if (result)
        {
            transaction.accept();
            return true;
        }
        else
        {
            return false;
        }
    }

    if (lhs->isVariable())
    {
        TypeVariable* lhsVariable = lhs->get<TypeVariable>();

        if (!lhsVariable->quantified())
        {
            bool result = compare(lhsVariable, rhs);

            if (result)
            {
                transaction.accept();
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            // If lhs is a quantified type variable, then lhs <= rhs is possible
            // only when rhs is also a type variable
            if (rhs->isVariable())
            {
                // TODO: Check constraints
                transaction.accept();
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    // If we get to this point, then lhs and rhs are not variables

    switch (lhs->tag())
    {
        case ttBase:
        {
            if (rhs->tag() != ttBase)
                return false;

            return lhs->get<BaseType>() == rhs->get<BaseType>();
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
                if (!compare(lhsFunction->inputs()[i], rhsFunction->inputs()[i]))
                    return false;
            }

            if (!compare(lhsFunction->output(), rhsFunction->output()))
                return false;

            transaction.accept();
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
                if (!compare(lhsConstructed->typeParameters()[i], rhsConstructed->typeParameters()[i]))
                    return false;
            }

            transaction.accept();
            return true;
        }
    }

    assert(false);
}


//// Overlap ///////////////////////////////////////////////////////////////////

bool TypeComparer::overlap(Type* lhs, Type* rhs)
{
    lhs = lookupLeft(lhs);
    rhs = lookupRight(rhs);

    if (lhs->isVariable())
    {
        assert(lhs->get<TypeVariable>()->quantified());

        if (equals(lhs, rhs))
            return true;

        TypeVariable* lhsVariable = lhs->get<TypeVariable>();

        assert(_lhsSubs.find(lhsVariable) == _lhsSubs.end());

        for (Trait* constraint : lhsVariable->constraints())
        {
            if (rhs->isVariable())
            {
                _newConstraints[rhs->get<TypeVariable>()].insert(constraint);
            }
            else
            {
                if (!isSubtype(rhs, constraint))
                    return false;
            }
        }

        _lhsSubs[lhs->get<TypeVariable>()] = rhs;

        return true;
    }
    else if (rhs->isVariable())
    {
        assert(rhs->get<TypeVariable>()->quantified());
        assert(_rhsSubs.find(rhs->get<TypeVariable>()) == _rhsSubs.end());

        for (Trait* constraint : rhs->get<TypeVariable>()->constraints())
        {
            if (!isSubtype(lhs, constraint))
                return false;
        }

        _rhsSubs[rhs->get<TypeVariable>()] = lhs;

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
                if (!overlap(lhsFunction->inputs()[i], rhsFunction->inputs()[i]))
                    return false;
            }

            if (!overlap(lhsFunction->output(), rhsFunction->output()))
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
                if (!overlap(lhsConstructed->typeParameters()[i], rhsConstructed->typeParameters()[i]))
                    return false;
            }

            return true;
        }
    }

    assert(false);
}
