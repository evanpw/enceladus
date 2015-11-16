#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "semantic/subtype.hpp"
#include "semantic/type_functions.hpp"
#include "semantic/unify_trait.hpp"

TEST_CASE("subtype checks", "[isSubtype]")
{
    TypeTable* table = new TypeTable;

    // For concrete base types, the subtype relation is the same as equality
    REQUIRE(isSubtype(table->Int, table->Int));
    REQUIRE(!isSubtype(table->Bool, table->Int));

    // Every type is a subtype of a quantified type variable
    Type* S = table->createTypeVariable("S", true);
    Type* T = table->createTypeVariable("T", true);
    REQUIRE(isSubtype(T, T));
    REQUIRE(isSubtype(table->Int, T));
    REQUIRE(!isSubtype(T, table->Int));
    REQUIRE(isSubtype(S, T));

    // Instances of a trait are subtypes of a quantified type variable with
    // that trait as constraint
    Type* V = table->createTypeVariable("V", true);
    V->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(isSubtype(table->Int, V));
    REQUIRE(isSubtype(table->UInt, V));
    Trait* Signed = table->createTrait("Signed");
    V->get<TypeVariable>()->addConstraint(Signed);
    Signed->addInstance(table->Int);
    REQUIRE(isSubtype(table->Int, V));
    REQUIRE(!isSubtype(table->UInt, V));

    // Constrained (quantified) type variables are subtypes of type variables
    // with fewer constraints
    S->get<TypeVariable>()->addConstraint(Signed);
    REQUIRE(isSubtype(S, T));
    REQUIRE(!isSubtype(T, S));
    T->get<TypeVariable>()->addConstraint(Signed);
    S->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(isSubtype(S, T));
    REQUIRE(!isSubtype(T, S));

    // Unquantified type variables are subtypes of every type
    Type* U = table->createTypeVariable();
    REQUIRE(isSubtype(U, table->Int));

    // Constrained quantified type variables are subtypes of any type which
    // is an instance of every constraint
    U->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(isSubtype(U, table->Int));
    REQUIRE(!isSubtype(U, table->Bool));
}

TEST_CASE("concrete-type subtype checks", "[isSubtype-concrete]")
{
    TypeTable* table = new TypeTable;

    // Base types are compatible only when equal
    REQUIRE(isSubtype(table->Int, table->Int));
    REQUIRE(!isSubtype(table->Int, table->UInt));

    // Function types are compatible when they have the same valence, and
    // compatible inputs and outputs
    Type* binaryFn1 = table->createFunctionType({table->Int, table->Int}, table->Int);
    Type* binaryFn2 = table->createFunctionType({table->Int, table->Int}, table->Int);
    Type* binaryFn3 = table->createFunctionType({table->Int, table->Bool}, table->Int);
    Type* unaryFn = table->createFunctionType({table->Int}, table->Int);
    REQUIRE(isSubtype(binaryFn1, binaryFn2));
    REQUIRE(!isSubtype(binaryFn1, binaryFn3));
    REQUIRE(!isSubtype(binaryFn1, unaryFn));

    // Constructed types are compatible when they have the same type constructor
    // and compatible type parameters
    Type* T = table->createTypeVariable("T", true);
    ConstructedType* List = table->createConstructedType("List", {T})->get<ConstructedType>();

    Type* intList1 = List->instantiate({table->Int});
    Type* intList2 = List->instantiate({table->Int});
    Type* stringList = List->instantiate({table->Bool});
    REQUIRE(isSubtype(intList1, intList2));
    REQUIRE(!isSubtype(intList1, stringList));

    Type* S = table->createTypeVariable("S", true);
    ConstructedType* Pair = table->createConstructedType("Pair", {S, T})->get<ConstructedType>();
    Type* t1 = Pair->instantiate({
        Pair->instantiate({table->Int, table->Int}),
        table->Int
    });
    Type* t2 = Pair->instantiate({
        Pair->instantiate({table->Int, table->Int}),
        table->Int
    });
    REQUIRE(isSubtype(t1, t2));

    delete table;
}

TEST_CASE("generic-type subtype checks", "[isSubtype-generic]")
{
    TypeTable* table = new TypeTable;

    Type* S = table->createTypeVariable("S", true);
    Type* T = table->createTypeVariable("T", true);
    Type* U = table->createTypeVariable("U", true);
    Type* V = table->createTypeVariable("V", true);
    Type* W = table->createTypeVariable();
    Type* X = table->createTypeVariable();
    ConstructedType* List = table->createConstructedType("List", {T})->get<ConstructedType>();
    ConstructedType* Pair = table->createConstructedType("Pair", {S, T})->get<ConstructedType>();
    ConstructedType* P3 = table->createConstructedType("P3", {S, T, U})->get<ConstructedType>();
    ConstructedType* P4 = table->createConstructedType("P4", {S, T, U, V})->get<ConstructedType>();

    // Recursive matching within constructed types
    Type* intList = List->instantiate({table->Int});
    Type* genericList = List->instantiate({S});
    REQUIRE(isSubtype(intList, genericList));
    REQUIRE(!isSubtype(genericList, intList));

    // ['W] <= [S]
    REQUIRE(isSubtype(List->instantiate({W}), List->instantiate({S})));

    // Variables which match in one place must match with the same type in
    // every other place (as if they were substituted)
    Type* equalPair = Pair->instantiate({S, S});
    Type* unequalPair = Pair->instantiate({T, U});
    REQUIRE(isSubtype(equalPair, unequalPair));
    REQUIRE(!isSubtype(unequalPair, equalPair));

    Type* twoInts = Pair->instantiate({table->Int, table->Int});
    Type* intString = Pair->instantiate({table->Int, table->Bool});
    REQUIRE(isSubtype(twoInts, equalPair));
    REQUIRE(!isSubtype(intString, equalPair));
    REQUIRE(isSubtype(twoInts, unequalPair));
    REQUIRE(isSubtype(intString, unequalPair));

    // For unquantified variables, isSubtype answers the question: is there any
    // assignment to the unquantified free variables which makes lhs a subtype
    // of rhs?
    Type* unequalPairUnQ = Pair->instantiate({W, X});
    REQUIRE(isSubtype(unequalPairUnQ, equalPair));

    Type* equalPairUnQ = Pair->instantiate({W, W});
    REQUIRE(isSubtype(equalPairUnQ, twoInts));
    REQUIRE(!isSubtype(equalPairUnQ, intString));
    REQUIRE(isSubtype(equalPairUnQ, unequalPair));

    // First two slots require T1=T2 and T3=T2. Next two test whether the
    // compatibility check correctly determines that T1=T3
    Type* type1 = P4->instantiate({S, V, S, U});
    Type* type2 = P4->instantiate({T, T, table->Int, table->Bool});
    Type* type3 = P4->instantiate({table->Int, table->Bool, table->Int, table->Bool});
    Type* type4 = P4->instantiate({table->Bool, table->Bool, table->Int, table->Bool});
    REQUIRE(!isSubtype(type1, type2));
    REQUIRE(!isSubtype(type2, type1));
    REQUIRE(isSubtype(type3, type1));
    REQUIRE(isSubtype(type4, type2));

    // Make sure the checker doesn't go around in circles
    Type* t12 = Pair->instantiate({S, T});
    Type* t21 = Pair->instantiate({T, S});
    REQUIRE(isSubtype(t12, t21));

    Type* t121 = P3->instantiate({S, T, S});
    Type* t21i = P3->instantiate({T, S, table->Int});
    REQUIRE(!isSubtype(t121, t21i));

    delete table;
}

TEST_CASE("pair subtype checks", "[isSubtype-pairs]")
{
    TypeTable* table = new TypeTable;

    Type* S = table->createTypeVariable();
    Type* T = table->createTypeVariable();
    Type* U = table->createTypeVariable("U", true);
    Type* V = table->createTypeVariable("V", true);
    Type* W = table->createTypeVariable("W", true);
    Type* X = table->createTypeVariable("X", true);
    ConstructedType* Pair = table->createConstructedType("Pair", {W, X})->get<ConstructedType>();

    // Pair<'S, 'S> <= Pair<U, V>?
    CHECK(isSubtype(Pair->instantiate({S, S}), Pair->instantiate({U, V})));

    // Pair<'S, 'T> <= Pair<U, U>?
    CHECK(isSubtype(Pair->instantiate({S, S}), Pair->instantiate({U, U})));

    // Pair<'S, 'T> <= Pair<Int, Int>?
    CHECK(isSubtype(Pair->instantiate({S, T}), Pair->instantiate({table->Int, table->Int})));

    // Pair<'S, 'S> <= Pair<Int, UInt>?
    CHECK(!isSubtype(Pair->instantiate({S, S}), Pair->instantiate({table->Int, table->UInt})));

    delete table;
}

TEST_CASE("constrained pair subtype checks", "[isSubtype-constrained-pairs]")
{
    TypeTable* table = new TypeTable;

    Type* S = table->createTypeVariable();
    Type* T = table->createTypeVariable("T", true);
    T->get<TypeVariable>()->addConstraint(table->Num);
    Type* U = table->createTypeVariable("U", true);
    Type* V = table->createTypeVariable("V", true);
    ConstructedType* Pair = table->createConstructedType("Pair", {U, V})->get<ConstructedType>();

    // Pair<'S, 'S> <= Pair<T: Num, String>?
    CHECK(!isSubtype(Pair->instantiate({S, S}), Pair->instantiate({T, table->Bool})));

    // Pair<'S, 'S> <= Pair<T: Num, T: Num>?
    CHECK(isSubtype(Pair->instantiate({S, S}), Pair->instantiate({T, T})));

    // Pair<'S, 'S> <= Pair<T: Num, Int>?
    CHECK(isSubtype(Pair->instantiate({S, S}), Pair->instantiate({T, table->Int})));

    delete table;
}

TEST_CASE("constrained generic-type subtype checks", "[isSubtype-constrained]")
{
    TypeTable* table = new TypeTable;

    // Check setup
    CHECK(isSubtype(table->Int, table->Num));
    CHECK(isSubtype(table->UInt, table->Num));

    Type* varNum = table->createTypeVariable("S", true);
    ConstructedType* Pair = table->createConstructedType("Pair", {table->createTypeVariable("", true), table->createTypeVariable("", true)})->get<ConstructedType>();
    Trait* Signed = table->createTrait("Signed");
    Signed->addInstance(table->Int);

    // Constrained variables should not match types which aren't instances
    // of the constraining type
    varNum->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(isSubtype(table->Int, varNum));
    REQUIRE(!isSubtype(table->Bool, varNum));

    // Multiple constraints
    Type* varSignedNum = table->createTypeVariable("T", true);
    varSignedNum->get<TypeVariable>()->addConstraint(table->Num);
    varSignedNum->get<TypeVariable>()->addConstraint(Signed);
    REQUIRE(isSubtype(table->Int, varSignedNum));
    REQUIRE(!isSubtype(table->UInt, varSignedNum));

    Type* type1a = Pair->instantiate({varNum, varNum});
    Type* type2a = Pair->instantiate({table->Int, table->Int});
    Type* type3a = Pair->instantiate({table->Bool, table->Bool});
    REQUIRE(isSubtype(type2a, type1a));
    REQUIRE(!isSubtype(type3a, type1a));
    REQUIRE(!isSubtype(type1a, type2a));
    REQUIRE(!isSubtype(type1a, type3a));

    delete table;
}

TEST_CASE("complex subtype checks", "[isSubtype-complex]")
{
    TypeTable* table = new TypeTable;

    Type* V = table->createTypeVariable("V", true);
    Trait* Iterator = table->createTrait("Iterator", {V});

    Type* S = table->createTypeVariable("S");
    Type* T = table->createTypeVariable("T", true);
    Type* U = table->createTypeVariable("U", true);
    T->get<TypeVariable>()->addConstraint(Iterator->instantiate({U}));

    Type* T1 = table->createTypeVariable("T1");
    T1->get<TypeVariable>()->addConstraint(Iterator->instantiate({table->Int}));

    Type* Y = table->createTypeVariable("Y", true);
    Type* Z = table->createTypeVariable("Z", true);
    Y->get<TypeVariable>()->addConstraint(Iterator->instantiate({Z}));

    Type* W = table->createTypeVariable("U", true);
    Type* X = table->createTypeVariable("V", true);
    ConstructedType* Pair = table->createConstructedType("Pair", {W, X})->get<ConstructedType>();

    // Make Bool: Iterator<Char> for the purposes of this test
    Iterator->addInstance(table->Bool, {table->UInt8});

    // 'S <= T: Iterator<U>
    CHECK(isSubtype(S, T));

    // Pair<'S, 'S> <= Pair<T: Iterator<U>, String>?
    CHECK(isSubtype(Pair->instantiate({S, S}), Pair->instantiate({T, table->Bool})));

    // 'T1: Iterator<Int> <= T: Iterator<U>?
    CHECK(isSubtype(T1, T));

    // Pair<'S, 'S> <= Pair<T: Iterator<U>, Y: Iterator<Z>>?
    CHECK(isSubtype(Pair->instantiate({S, S}), Pair->instantiate({T, Y})));

    delete table;
}

TEST_CASE("type-trait unification checks", "[unify-type-trait]")
{
    TypeTable* table = new TypeTable;

    // Type Array<S>
    Type* S = table->createTypeVariable("S", true);
    ConstructedType* Array = table->createConstructedType("Array", {S})->get<ConstructedType>();

    // Trait Index<T>
    Type* T = table->createTypeVariable("T", true);
    Trait* Index = table->createTrait("Index", {T});

    // Make Array<U> an instance of Index<U>
    Type* U = table->createTypeVariable("U", true);
    Type* ArrayU = Array->instantiate({U});
    Index->addInstance(ArrayU, {U});

    SECTION("quantified-lhs")
    {
        // Array<V> and Index<'T1>
        Type* V = table->createTypeVariable("V", true);
        Type* ArrayV = Array->instantiate({V});
        Type* T1 = table->createTypeVariable();
        Trait* IndexT1 = Index->instantiate({T1});

        // Should unify, with 'T1 = V
        auto result = tryUnify(ArrayV, IndexT1);
        CHECK(result.first);
        CHECK(equals(T1, V));
    }

    SECTION("constrained-lhs")
    {
        // Array<'T1: Num>
        Type* T1 = table->createTypeVariable();
        T1->get<TypeVariable>()->addConstraint(table->Num);
        Type* ArrayT1 = Array->instantiate({T1});

        // Index<'T2>
        Type* T2 = table->createTypeVariable();
        Trait* IndexT2 = Index->instantiate({T2});

        // Should unify, with 'T2 = 'T1
        auto result = tryUnify(ArrayT1, IndexT2);

        CHECK(result.first);
        CHECK(T1->isVariable());
        CHECK(!T1->get<TypeVariable>()->quantified());
        CHECK(isSubtype(T1, table->Num));
        CHECK(equals(T1, T2));
    }

    delete table;
}

/*
TEST_CASE("unification checks", "[unify]")
{
    TypeTable* table = new TypeTable;

    Type* S = table->createTypeVariable("S");
    S->get<TypeVariable>()->addConstraint(table->Num);

    Type* T = table->createTypeVariable("T");
    T->get<TypeVariable>()->addConstraint(table->Num);

    tryUnify(S, T);

    CHECK(S->get<TypeVariable>());
    CHECK(S->get<TypeVariable>() == T->get<TypeVariable>());
    CHECK(S->get<TypeVariable>()->constraints().size() == 1);
    CHECK(*(S->get<TypeVariable>()->constraints().begin()) == table->Num);

    delete table;
}
*/

TEST_CASE("type overlap checks", "[type-overlap]")
{
    TypeTable* table = new TypeTable;

    Type* S = table->createTypeVariable("S", true);
    Type* T = table->createTypeVariable("T", true);
    Type* U = table->createTypeVariable("U", true);
    Type* V = table->createTypeVariable("V", true);

    // For base types, overlap <=> equality
    REQUIRE(overlap(table->Int, table->Int));
    REQUIRE(!overlap(table->Int, table->Bool));

    // A type variable matches every type that meets its constraints
    REQUIRE(overlap(T, table->Int));
    REQUIRE(overlap(table->Int, T));
    REQUIRE(overlap(S, T));
    T->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(!overlap(T, table->Bool));
    REQUIRE(!overlap(table->Bool, T));
    REQUIRE(overlap(S, T));

    // Must be a *consistent* choice of type variables
    ConstructedType* Pair = table->createConstructedType("Pair", {S, T})->get<ConstructedType>();
    Type* t1 = Pair->instantiate({T, T});
    Type* t2 = Pair->instantiate({table->Int, table->Bool});
    REQUIRE(!overlap(t1, t2));
    Type* t3 = Pair->instantiate({table->Int, table->Int});
    REQUIRE(overlap(t1, t3));

    ConstructedType* P3 = table->createConstructedType("P3", {S, T, U})->get<ConstructedType>();
    Type* t4 = P3->instantiate({S, T, S});
    Type* t5 = P3->instantiate({U, U, table->Int});
    REQUIRE(overlap(t4, t5));

    ConstructedType* P4 = table->createConstructedType("P4", {S, T, U, V})->get<ConstructedType>();
    Type* t6 = P4->instantiate({S, T, S, T});
    Type* t7 = P4->instantiate({U, U, table->Int, table->Bool});
    REQUIRE(!overlap(t6, t7));

    delete table;
}

TEST_CASE("generic trait checks", "[generic-trait]")
{
    TypeTable* table = new TypeTable;

    SECTION("sub-constraint")
    {
        Type* S = table->createTypeVariable("S", true);
        Trait* Iterator = table->createTrait("Iterator", {S});

        Type* T = table->createTypeVariable("T", true);
        T->get<TypeVariable>()->addConstraint(Iterator->instantiate({table->UInt8}));

        Type* T1 = table->createTypeVariable();
        Type* T2 = table->createTypeVariable();
        T1->get<TypeVariable>()->addConstraint(Iterator->instantiate({T2}));

        // Unify T: Iterator<UInt8> with 'T1: Iterator<'T2>
        // Expecting T: Iterator<UInt8>
        tryUnify(T, T1);
        CHECK(T->get<TypeVariable>() == T1->get<TypeVariable>());
        CHECK(T->get<TypeVariable>()->constraints().size() == 1);
    }

    SECTION("string-iterator")
    {
        Type* S = table->createTypeVariable("S", true);
        Trait* Iterator = table->createTrait("Iterator", {S});

        // StringIterator: Iterator<UInt8>
        Type* StringIterator = table->createBaseType("StringIterator");
        Iterator->addInstance(StringIterator, {table->UInt8});

        // StringIterator <= Iterator<T>?
        Type* T = table->createTypeVariable("T", true);
        CHECK(isSubtype(StringIterator, Iterator->instantiate({T})));
    }

    SECTION("multiple-params")
    {
        Type* T1 = table->createTypeVariable("T1", true);
        Type* T2 = table->createTypeVariable("T2", true);
        Trait* Map = table->createTrait("Map", {T1, T2});

        Type* S = table->createTypeVariable("S", true);
        Type* T = table->createTypeVariable("T", true);
        Type* U = table->createTypeVariable("U", true);

        Type* V = table->createTypeVariable("V", true);
        Type* W = table->createTypeVariable("V", true);

        V->get<TypeVariable>()->addConstraint(Map->instantiate({S, S}));
        W->get<TypeVariable>()->addConstraint(Map->instantiate({T, U}));

        // V: Map<S, S> <= W: Map<T, U>?
        CHECK(isSubtype(V, W));

        // W: Map<T, U> <= V: Map<S, S>?
        CHECK(!isSubtype(W, V));
    }

    SECTION("var-vs-generic")
    {
        Type* T = table->createTypeVariable("T", true);
        Type* List = table->createConstructedType("List", {T});

        Type* S = table->createTypeVariable("S", true);
        Trait* Iterator = table->createTrait("Iterator", {S});

        // List<T> is an instance of Iterator<T>
        Iterator->addInstance(List, {T});

        Type* T1 = table->createTypeVariable();
        Type* T2 = table->createTypeVariable();
        T1->get<TypeVariable>()->addConstraint(Iterator->instantiate({T2}));

        Type* W = table->createTypeVariable("W", true);
        Type* genericList = List->get<ConstructedType>()->instantiate({W});

        // Unify 'T1: Iterator<'T2> with [W]
        // Expecting [W], with 'T2 getting assigned a value of W
        auto result = tryUnify(T1, genericList);
        CHECK(result.first);
        CHECK(equals(T1, genericList));
        CHECK(equals(T2, W));
    }

    SECTION("iterator-iterable")
    {
        // trait Iterator<A>
        Type* A = table->createTypeVariable("A", true);
        Trait* Iterator = table->createTrait("Iterator", {A});

        // trait Iterable<B>
        Type* B = table->createTypeVariable("B", true);
        Trait* Iterable = table->createTrait("Iterable", {B});

        // impl Iterable<C> for D where D: Iterator<C>
        Type* D = table->createTypeVariable("D", true);
        Type* C = table->createTypeVariable("C", true);
        Trait* IteratorC = Iterator->instantiate({C});
        D->get<TypeVariable>()->addConstraint(IteratorC);
        Iterable->addInstance(D, {C});

        // impl Iterator<Int> for Int
        Iterator->addInstance(table->Int, {table->Int});

        // Unify 'T1: Iterable<'T2> with Int
        // Expecting 'T1 = Int, 'T2 = Int
        Type* T1 = table->createTypeVariable("T1");
        Type* T2 = table->createTypeVariable("T2");
        Trait* IterableT2 = Iterable->instantiate({T2});
        T1->get<TypeVariable>()->addConstraint(IterableT2);

        auto result = tryUnify(T1, table->Int);
        CHECK(result.first);
        CHECK(equals(T1, table->Int));
        CHECK(equals(T2, table->Int));
    }

    SECTION("iterator-iterable2")
    {
        // trait Iterator<A>
        Type* A = table->createTypeVariable("A", true);
        Trait* Iterator = table->createTrait("Iterator", {A});

        // trait Iterable<B>
        Type* B = table->createTypeVariable("B", true);
        Trait* Iterable = table->createTrait("Iterable", {B});

        // impl Iterable<C> for D where D: Iterator<C>
        Type* D = table->createTypeVariable("D", true);
        Type* C = table->createTypeVariable("C", true);
        Trait* IteratorC = Iterator->instantiate({C});
        D->get<TypeVariable>()->addConstraint(IteratorC);
        Iterable->addInstance(D, {C});

        // struct Container<E>
        Type* E = table->createTypeVariable("E", true);
        Type* Container = table->createConstructedType("Container", {E});

        // impl Iterator<F> for Container<F>
        Type* F = table->createTypeVariable("F", true);
        Type* ContainerF = Container->get<ConstructedType>()->instantiate({F});
        Iterator->addInstance(ContainerF, {F});

        // Unify Container<Int> with Iterable<'T1>
        // Expecting 'T1 = Int
        Type* ContainerInt = Container->get<ConstructedType>()->instantiate({table->Int});
        Type* T1 = table->createTypeVariable("T1");
        Trait* IterableT1 = Iterable->instantiate({T1});

        std::cerr << std::endl;
        auto result = tryUnify(ContainerInt, IterableT1);
        CHECK(result.first);
        CHECK(equals(T1, table->Int));
    }

    SECTION("iterator-iterable3")
    {
        // trait Iterator<A>
        Type* A = table->createTypeVariable("A", true);
        Trait* Iterator = table->createTrait("Iterator", {A});

        // trait Iterable<B>
        Type* B = table->createTypeVariable("B", true);
        Trait* Iterable = table->createTrait("Iterable", {B});

        // impl Iterable<C> for D where D: Iterator<C>
        Type* D = table->createTypeVariable("D", true);
        Type* C = table->createTypeVariable("C", true);
        Trait* IteratorC = Iterator->instantiate({C});
        D->get<TypeVariable>()->addConstraint(IteratorC);
        Iterable->addInstance(D, {C});

        // S: Iterator<T: Num>
        Type* T = table->createTypeVariable("T", true);
        T->get<TypeVariable>()->addConstraint(table->Num);
        Trait* IteratorT = Iterator->instantiate({T});
        Type* S = table->createTypeVariable("S", true);
        S->get<TypeVariable>()->addConstraint(IteratorT);

        // Iterable<'T1>
        Type* T1 = table->createTypeVariable("T1");
        Trait* IterableT1 = Iterable->instantiate({T1});

        CHECK(isSubtype(S, IterableT1));
    }

    SECTION("iterator-iterable4")
    {
        // trait Iterator<A>
        Type* A = table->createTypeVariable("A", true);
        Trait* Iterator = table->createTrait("Iterator", {A});

        // trait Iterable<B>
        Type* B = table->createTypeVariable("B", true);
        Trait* Iterable = table->createTrait("Iterable", {B});

        // impl Iterable<C> for D where D: Iterator<C>
        Type* D = table->createTypeVariable("D", true);
        Type* C = table->createTypeVariable("C", true);
        Trait* IteratorC = Iterator->instantiate({C});
        D->get<TypeVariable>()->addConstraint(IteratorC);
        Iterable->addInstance(D, {C});

        // struct Fib
        Type* Fib = table->createBaseType("Fib");

        // impl Iterator<Int> for Fib
        Iterator->addInstance(Fib, {table->Int});

        // struct TakeWhile<F> where F: Iterator<E>
        Type* E = table->createTypeVariable("E", true);
        Trait* IteratorE = Iterator->instantiate({E});
        Type* F = table->createTypeVariable("F", true);
        F->get<TypeVariable>()->addConstraint(IteratorE);
        Type* TakeWhile = table->createConstructedType("TakeWhile", {F});

        // impl Iterator<G> for TakeWhile<H> where H: Iterator<G>
        Type* G = table->createTypeVariable("G", true);
        Trait* IteratorG = Iterator->instantiate({G});
        Type* H = table->createTypeVariable("H", true);
        H->get<TypeVariable>()->addConstraint(IteratorG);
        Type* TakeWhileH = TakeWhile->get<ConstructedType>()->instantiate({H});
        Iterator->addInstance(TakeWhileH, {G});

        // lhs = TakeWhile<Fib>
        Type* lhs = TakeWhile->get<ConstructedType>()->instantiate({Fib});

        // rhs = I: Iterator<J: Num>
        Type* J = table->createTypeVariable("J", true);
        J->get<TypeVariable>()->addConstraint(table->Num);
        Trait* IteratorJ = Iterator->instantiate({J});
        Type* I = table->createTypeVariable("I", true);
        I->get<TypeVariable>()->addConstraint(IteratorJ);
        Type* rhs = I;

        CHECK(isSubtype(lhs, rhs));
    }

    delete table;
}

TEST_CASE("recursive type checks", "[recursive-type]")
{
    TypeTable* table = new TypeTable;

    Type* S = table->createTypeVariable("S", true);
    Trait* Iterator = table->createTrait("Iterator", {S});

    Type* T = table->createTypeVariable("T", true);
    T->get<TypeVariable>()->addConstraint(Iterator->instantiate({T}));

    CHECK(T->str() == "T: Iterator<T>");

    delete table;
}

TEST_CASE("implied substitutions", "[implied-subs]")
{
    TypeTable* table = new TypeTable;

    SECTION("on-subsitute")
    {
        // trait Iterator<S>
        Type* S = table->createTypeVariable("S", true);
        Trait* Iterator = table->createTrait("Iterator", {S});

        // impl Iterator<Int> for Int
        Iterator->addInstance(table->Int, {table->Int});

        // T: Iterator<U>
        Type* T = table->createTypeVariable("T", true);
        Type* U = table->createTypeVariable("U", true);
        Trait* IteratorU = Iterator->instantiate({U});
        T->get<TypeVariable>()->addConstraint(IteratorU);

        // Function type: |T: Iterator<U>| -> U
        Type* nextType = table->createFunctionType({T}, U);

        // Make assignment T -> Int in nextType
        TypeAssignment assignment;
        assignment[T->get<TypeVariable>()] = table->Int;
        Type* resultType = substitute(nextType, assignment);

        // Check that the implied subsitution U -> Int also got made
        CHECK(equals(resultType->get<FunctionType>()->output(), table->Int));
    }

    SECTION("on-bind-variable")
    {
        // trait Iterator<Z>
        Type* Z = table->createTypeVariable("Z", true);
        Trait* Iterator = table->createTrait("Iterator", {Z});

        // S: Iterator<T>
        Type* S = table->createTypeVariable("S", true);
        Type* T = table->createTypeVariable("T", true);
        Trait* IteratorT = Iterator->instantiate({T});
        S->get<TypeVariable>()->addConstraint(IteratorT);

        // 'T1: Iterator<'T2>
        Type* T1 = table->createTypeVariable("T1");
        Type* T2 = table->createTypeVariable("T2");
        Trait* IteratorT2 = Iterator->instantiate({T2});
        T1->get<TypeVariable>()->addConstraint(IteratorT2);

        // assign 'T1 -> S, and hope that we get 'T2 -> T for free
        bindVariable(T1, S);
        CHECK(equals(T2, T));
    }

    SECTION("on-instantiate")
    {
        // trait Iterator<S>
        Type* S = table->createTypeVariable("S", true);
        Trait* Iterator = table->createTrait("Iterator", {S});

        // impl Iterator<Int> for Int
        Iterator->addInstance(table->Int, {table->Int});

        // T: Iterator<U>
        Type* T = table->createTypeVariable("T", true);
        Type* U = table->createTypeVariable("U", true);
        Trait* IteratorU = Iterator->instantiate({U});
        T->get<TypeVariable>()->addConstraint(IteratorU);

        // Function type: |T: Iterator<U>| -> U
        Type* nextType = table->createFunctionType({T}, U);

        // Make assignment T -> Int in nextType
        TypeAssignment assignment;
        assignment[T->get<TypeVariable>()] = table->Int;
        Type* resultType = instantiate(nextType, assignment);

        // Check that the implied subsitution U -> Int also got made
        CHECK(equals(resultType->get<FunctionType>()->output(), table->Int));
    }

    delete table;
}
