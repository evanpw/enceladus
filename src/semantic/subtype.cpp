#include "semantic/subtype.hpp"
#include "semantic/type_functions.hpp"

class TypeComparer
{
public:
    bool compare(const Trait* lhs, const Trait* rhs);
    bool compare(const Type* lhs, const Trait* trait);
    bool compare(const Type* lhs, const Type* rhs);

private:
    // Special cases
    bool compare(TypeVariable* lhs, const Type* rhs);
    bool compare(const Type* lhs, TypeVariable* rhs);

    std::set<const Trait*> getConstraints(TypeVariable* var);

    // Saves the state of comparer at initialization, and rolls it back at
    // destruction unless accept() is called first
    class Transaction
    {
    public:
        Transaction(TypeComparer* comparer);
        ~Transaction();

        void rollback();
        void accept() { _accepted = true; }

    private:
        bool _accepted = false;

        TypeComparer* _comparer;

        std::unordered_map<TypeVariable*, const Type*> _lhsSubs;
        std::unordered_map<TypeVariable*, const Type*> _rhsSubs;
        std::unordered_map<TypeVariable*, std::set<const Trait*>> _newConstraints;
    };

    friend class Transaction;

    std::unordered_map<TypeVariable*, const Type*> _lhsSubs;
    std::unordered_map<TypeVariable*, const Type*> _rhsSubs;
    std::unordered_map<TypeVariable*, std::set<const Trait*>> _newConstraints;

};

std::set<const Trait*> TypeComparer::getConstraints(TypeVariable* var)
{
    std::set<const Trait*> result = var->constraints();

    auto i = _newConstraints.find(var);
    if (i != _newConstraints.end())
    {
        for (const Trait* trait : i->second)
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

bool isSubtype(const Trait* lhs, const Trait* rhs)
{
    TypeComparer comparer;
    return comparer.compare(lhs, rhs);
}

bool isSubtype(const Type* lhs, const Trait* trait)
{
    TypeComparer comparer;
    return comparer.compare(lhs, trait);
}

bool isSubtype(const Type* lhs, const Type* rhs)
{
    TypeComparer comparer;
    return comparer.compare(lhs, rhs);
}

//// Trait <= Trait ////////////////////////////////////////////////////////////

bool TypeComparer::compare(const Trait* lhs, const Trait* rhs)
{
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

bool TypeComparer::compare(const Type* lhs, const Trait* trait)
{
    Transaction transaction(this);

    if (lhs->isVariable())
    {
        TypeVariable* lhsVariable = lhs->get<TypeVariable>();
        if (!lhsVariable->quantified())
        {
            transaction.accept();
            return true;
        }
        else
        {
            for (const Trait* constraint : getConstraints(lhsVariable))
            {
                if (compare(constraint, trait))
                {
                    transaction.accept();
                    return true;
                }
            }

            return false;
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
                if (!compare(instantiate(trait->parameters()[i]), instance.traitParams[i]))
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

bool TypeComparer::compare(TypeVariable* lhs, const Type* rhs)
{
    Transaction transaction(this);
    assert(!lhs->quantified());

    if (!rhs->isVariable())
    {
        for (const Trait* constraint : getConstraints(lhs))
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
            if (compare(rhs, i->second))
            {
                // New assignment is more specific
                _lhsSubs[lhs] = rhs;
                transaction.accept();
                return true;
            }
            else if (compare(i->second, rhs))
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
        for (const Trait* rhsConstraint : rhs->get<TypeVariable>()->constraints())
        {
            // Don't worry about redundant constraints: they won't be externally
            // visible anyway
            _newConstraints[lhs].insert(instantiate(rhsConstraint->prototype()));
        }

        transaction.accept();
        return true;
    }
}

bool TypeComparer::compare(const Type* lhs, TypeVariable* rhs)
{
    assert(rhs->quantified());

    auto i = _rhsSubs.find(rhs);
    if (i != _rhsSubs.end())
    {
        return i->second->equals(lhs);
    }

    Transaction transaction(this);

    for (const Trait* constraint : rhs->constraints())
    {
        if (!compare(lhs, constraint))
            return false;
    }

    _rhsSubs[rhs] = lhs;
    transaction.accept();
    return true;
}

bool TypeComparer::compare(const Type* lhs, const Type* rhs)
{
    Transaction transaction(this);

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
            if (!rhs->isVariable())
            {
                return false;
            }

            // Fall through
        }
    }

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
