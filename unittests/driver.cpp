#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "semantic/type_functions.hpp"

TEST_CASE("subtype checks", "[isSubtype]")
{
    TypeTable* table = new TypeTable;

    // For concrete base types, the subtype relation is the same as equality
    REQUIRE(isSubtype(table->Int, table->Int));
    REQUIRE(!isSubtype(table->String, table->Int));

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
    REQUIRE(!isSubtype(U, table->String));
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
    Type* binaryFn3 = table->createFunctionType({table->Int, table->String}, table->Int);
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
    Type* stringList = List->instantiate({table->String});
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
    ConstructedType* List = table->createConstructedType("List", {T})->get<ConstructedType>();
    ConstructedType* Pair = table->createConstructedType("Pair", {S, T})->get<ConstructedType>();
    ConstructedType* P3 = table->createConstructedType("P3", {S, T, U})->get<ConstructedType>();
    ConstructedType* P4 = table->createConstructedType("P4", {S, T, U, V})->get<ConstructedType>();

    // Recursive matching within constructed types
    Type* intList = List->instantiate({table->Int});
    Type* genericList = List->instantiate({S});
    REQUIRE(isSubtype(intList, genericList));
    REQUIRE(!isSubtype(genericList, intList));

    // Variables which match in one place must match with the same type in
    // every other place (as if they were substituted)
    Type* equalPair = Pair->instantiate({S, S});
    Type* unequalPair = Pair->instantiate({T, U});
    REQUIRE(isSubtype(equalPair, unequalPair));
    REQUIRE(!isSubtype(unequalPair, equalPair));

    Type* twoInts = Pair->instantiate({table->Int, table->Int});
    Type* intString = Pair->instantiate({table->Int, table->String});
    REQUIRE(isSubtype(twoInts, equalPair));
    REQUIRE(!isSubtype(intString, equalPair));
    REQUIRE(isSubtype(twoInts, unequalPair));
    REQUIRE(isSubtype(intString, unequalPair));

    // First two slots require T1=T2 and T3=T2. Next two test whether the
    // compatibility check correctly determines that T1=T3
    Type* type1 = P4->instantiate({S, V, S, U});
    Type* type2 = P4->instantiate({T, T, table->Int, table->String});
    Type* type3 = P4->instantiate({table->Int, table->String, table->Int, table->String});
    Type* type4 = P4->instantiate({table->String, table->String, table->Int, table->String});
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
    REQUIRE(!isSubtype(table->String, varNum));

    // Multiple constraints
    Type* varSignedNum = table->createTypeVariable("T", true);
    varSignedNum->get<TypeVariable>()->addConstraint(table->Num);
    varSignedNum->get<TypeVariable>()->addConstraint(Signed);
    REQUIRE(isSubtype(table->Int, varSignedNum));
    REQUIRE(!isSubtype(table->UInt, varSignedNum));

    Type* type1a = Pair->instantiate({varNum, varNum});
    Type* type2a = Pair->instantiate({table->Int, table->Int});
    Type* type3a = Pair->instantiate({table->String, table->String});
    REQUIRE(isSubtype(type2a, type1a));
    REQUIRE(!isSubtype(type3a, type1a));
    REQUIRE(!isSubtype(type1a, type2a));
    REQUIRE(!isSubtype(type1a, type3a));

    delete table;
}

TEST_CASE("type overlap checks", "[type-overlap]")
{
    TypeTable* table = new TypeTable;

    Type* S = table->createTypeVariable("S", true);
    Type* T = table->createTypeVariable("T", true);
    Type* U = table->createTypeVariable("U", true);
    Type* V = table->createTypeVariable("V", true);

    // For base types, overlap <=> equality
    REQUIRE(overlap(table->Int, table->Int));
    REQUIRE(!overlap(table->Int, table->String));

    // A type variable matches every type, even if constrained
    REQUIRE(overlap(T, table->Int));
    REQUIRE(overlap(table->Int, T));
    REQUIRE(overlap(S, T));
    T->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(overlap(T, table->String));
    REQUIRE(overlap(table->String, T));
    REQUIRE(overlap(S, T));

    // Must be a *consistent* choice of type variables
    ConstructedType* Pair = table->createConstructedType("Pair", {S, T})->get<ConstructedType>();
    Type* t1 = Pair->instantiate({T, T});
    Type* t2 = Pair->instantiate({table->Int, table->String});
    REQUIRE(!overlap(t1, t2));
    Type* t3 = Pair->instantiate({table->Int, table->Int});
    REQUIRE(overlap(t1, t3));

    ConstructedType* P3 = table->createConstructedType("P3", {S, T, U})->get<ConstructedType>();
    Type* t4 = P3->instantiate({S, T, S});
    Type* t5 = P3->instantiate({U, U, table->Int});
    REQUIRE(overlap(t4, t5));

    ConstructedType* P4 = table->createConstructedType("P4", {S, T, U, V})->get<ConstructedType>();
    Type* t6 = P4->instantiate({S, T, S, T});
    Type* t7 = P4->instantiate({U, U, table->Int, table->String});
    REQUIRE(!overlap(t6, t7));
}