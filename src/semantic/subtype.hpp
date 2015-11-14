#ifndef SUBTYPE_HPP
#define SUBTYPE_HPP

#include "types.hpp"
#include <unordered_map>


//// TODO: Allow unquantified type variables in the rhs, and add some tests for
////       those cases. This is unavoidable when evaluating cases like
////            'T1: MyIterator<'T2>  <=   MyContainer<F>?
////       We have to check whether MyContainer<F> is a subtype of
////       MyIterator<'T2>.

/*
The subtype relation lhs <= rhs is defined as follows:

1. If lhs and rhs are concrete types (containing no type variables), then
   lhs <= rhs iff lhs and rhs are exactly equal

2. If a type contains quantified type variables, then consider it as a set
   of concrete types. The subtype relation is then equivalent to the subset
   relation.

3. If lhs contains unquantified type variables, then lhs <= rhs iff there is an
   assignment to all unquantified variables in lhs that makes the relation true

4. If rhs = T: Trait, then lhs <= rhs iff lhs is an instance of Trait

5. If lhs = 'T: Trait and rhs is concrete, then lhs <= rhs iff rhs is an
   instance of trait

6. It is an error for lhs and rhs to have any type variables in common

7. It is an error for the rhs to contain unquantified type variables

8. It is an error for an unquantified type variable to contain a quantified
   type variable in a trait bound

9. When matching constraints, only those trait impl's which have already been
   encountered are considered. For example: String <= T: Num is false, even
   though someone later could impl Num for String. This is because the subtype
   relation is used for method resolution, and we want to resolve based only
   on methods that already exists.


Algorithmic Details:
1. When testing lhs <= T, assign T to lhs. If T is already assigned to something
   else, then return false

2. When testing 'T <= rhs and rhs is concrete, assign 'T to rhs

3. When testing 'T <= S, then do nothing. This imposes no new constraints

4. When testing 'T <= S: Trait, add Trait as a constraint to 'T, but don't
   make any substitutions

5. When testing 'T <= S: Trait<U>, add Trait<'V> as a constraint to 'T,
   where 'V is a fresh type variable


Uses:
    The subtype relation is what determines method resolution. When encountering
    a call x.f(), with x: Lhs, the (hopefully unique) method f: |Rhs| -> ? is
    chosen for which Lhs <= Rhs.
*/


// Convenience functions
bool isSubtype(Trait* lhs, Trait* rhs);
bool isSubtype(Type* lhs, Trait* trait);
bool isSubtype(TypeVariable* lhs, Trait* trait);
bool isSubtype(Type* lhs, Type* rhs);
bool overlap(Type* lhs, Type* rhs);

// Full-power comparer
class TypeComparer
{
public:
    bool compare(Trait* lhs, Trait* rhs);
    bool compare(Type* lhs, Trait* trait);
    bool compare(Type* lhs, Type* rhs);
    bool overlap(Type* lhs, Type* rhs);

    const TypeAssignment& lhsSubs() const
    {
        return _lhsSubs;
    }

    const TypeAssignment& rhsSubs() const
    {
        return _rhsSubs;
    }

    const std::unordered_map<TypeVariable*, std::set<Trait*>>& newConstraints() const
    {
        return _newConstraints;
    }

private:
    // Special cases
    bool compare(TypeVariable* lhs, Type* rhs);
    bool compare(Type* lhs, TypeVariable* rhs);

    std::set<Trait*> getConstraints(TypeVariable* var);

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

        TypeAssignment _lhsSubs;
        TypeAssignment _rhsSubs;
        std::unordered_map<TypeVariable*, std::set<Trait*>> _newConstraints;
    };

    friend class Transaction;

    TypeAssignment _lhsSubs;
    TypeAssignment _rhsSubs;
    std::unordered_map<TypeVariable*, std::set<Trait*>> _newConstraints;
};

#endif
