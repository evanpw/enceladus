#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "semantic/type_functions.hpp"

TEST_CASE("concrete-type compatibility checks", "[isCompatible-concrete]")
{
    TypeTable* table = new TypeTable;

    // Base types are compatible only when equal
    REQUIRE(isCompatible(table->Int, table->Int));
    REQUIRE(!isCompatible(table->Int, table->UInt));

    // Function types are compatible when they have the same valence, and
    // compatible inputs and outputs
    Type* binaryFn1 = table->createFunctionType({table->Int, table->Int}, table->Int);
    Type* binaryFn2 = table->createFunctionType({table->Int, table->Int}, table->Int);
    Type* binaryFn3 = table->createFunctionType({table->Int, table->String}, table->Int);
    Type* unaryFn = table->createFunctionType({table->Int}, table->Int);
    REQUIRE(isCompatible(binaryFn1, binaryFn2));
    REQUIRE(!isCompatible(binaryFn1, binaryFn3));
    REQUIRE(!isCompatible(binaryFn1, unaryFn));

    // Constructed types are compatible when they have the same type constructor
    // and compatible type parameters
    Type* intList1 = table->createConstructedType("List", {table->Int});
    Type* intList2 = table->createConstructedType("List", {table->Int});
    Type* stringList = table->createConstructedType("List", {table->String});
    REQUIRE(isCompatible(intList1, intList2));
    REQUIRE(!isCompatible(intList1, stringList));

    delete table;
}

TEST_CASE("generic-type compatibility checks", "[isCompatible-generic]")
{
    TypeTable* table = new TypeTable;

    // Unconstrained type variables are compatible with everything
    Type* var1 = table->createTypeVariable("S");
    Type* var2 = table->createTypeVariable("T");
    REQUIRE(isCompatible(var1, var2));
    REQUIRE(isCompatible(var1, table->Int));

    // Recursive matching within constructed types
    Type* intList = table->createConstructedType("List", {table->Int});
    Type* genericList = table->createConstructedType("List", {var1});
    REQUIRE(isCompatible(intList, genericList));

    // Variables which match in one place must match with the same type in
    // every other place (as if they were substituted)
    Type* equalPair = table->createConstructedType("Pair", {var1, var1});
    Type* unequalPair = table->createConstructedType("Pair", {var1, var2});
    Type* twoInts = table->createConstructedType("Pair", {table->Int, table->Int});
    Type* intString = table->createConstructedType("Pair", {table->Int, table->String});
    REQUIRE(isCompatible(equalPair, twoInts));
    REQUIRE(!isCompatible(equalPair, intString));
    REQUIRE(isCompatible(unequalPair, twoInts));
    REQUIRE(isCompatible(unequalPair, intString));

    // First two slots require T1=T2 and T3=T2. Next two test whether the
    // compatibility check correctly determines that T1=T3
    Type* var3 = table->createTypeVariable("V");
    Type* type1 = table->createConstructedType("P4", {var1, var3, var1, var3});
    Type* type2 = table->createConstructedType("P4", {var2, var2, table->Int, table->String});
    REQUIRE(!isCompatible(type1, type2));

    // Make sure the checker doesn't go around in circles
    Type* t12 = table->createConstructedType("Pair", {var1, var2});
    Type* t21 = table->createConstructedType("Pair", {var2, var1});
    REQUIRE(isCompatible(t12, t21));

    Type* t121 = table->createConstructedType("P3", {var1, var2, var1});
    Type* t21i = table->createConstructedType("P3", {var2, var1, table->Int});
    REQUIRE(isCompatible(t121, t21i));

    delete table;
}

TEST_CASE("constrained generic-type compatibility checks", "[isCompatible-constrained]")
{
    TypeTable* table = new TypeTable;

    // Check setup
    CHECK(isInstance(table->Int, table->Num));
    CHECK(isInstance(table->UInt, table->Num));

    // Constrained variables should not match types which aren't instances
    // of the constraining type
    Type* varNum = table->createTypeVariable();
    varNum->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(isCompatible(varNum, table->Int));
    REQUIRE(!isCompatible(varNum, table->String));

    // Should work in the opposite order as well
    REQUIRE(isCompatible(table->Int, varNum));
    REQUIRE(!isCompatible(table->String, varNum));

    // Multiple constraints
    Trait* Signed = table->createTrait("Signed");
    Signed->addInstance(table->Int);
    Type* varSignedNum = table->createTypeVariable();
    varSignedNum->get<TypeVariable>()->addConstraint(table->Num);
    varSignedNum->get<TypeVariable>()->addConstraint(Signed);
    REQUIRE(isCompatible(varSignedNum, table->Int));
    REQUIRE(!isCompatible(varSignedNum, table->UInt));

    // When unifying two variables, the constraints add
    Type* var1 = table->createTypeVariable();
    Type* type1a = table->createConstructedType("Pair", {var1, var1});
    Type* type2a = table->createConstructedType("Pair", {varNum, table->Int});
    Type* type3a = table->createConstructedType("Pair", {varNum, table->String});
    REQUIRE(isCompatible(type1a, type2a));
    REQUIRE(!isCompatible(type1a, type3a));
    REQUIRE(isCompatible(type2a, type1a));
    REQUIRE(!isCompatible(type3a, type1a));

    Type* type1b = table->createConstructedType("P3", {var1, var1, var1});
    Type* varSigned = table->createTypeVariable();
    varSigned->get<TypeVariable>()->addConstraint(Signed);
    Type* type2b = table->createConstructedType("P3", {varNum, varSigned, table->Int});
    Type* type3b = table->createConstructedType("P3", {varNum, varSigned, table->UInt});
    REQUIRE(isCompatible(type1b, type2b));
    REQUIRE(!isCompatible(type1b, type3b));
    REQUIRE(isCompatible(type2b, type1b));
    REQUIRE(!isCompatible(type3b, type1b));

    delete table;
}

TEST_CASE("quantified-type compatibility checks", "[isCompatible-quantified]")
{
    TypeTable* table = new TypeTable;

    // Unconstrained: quantified type variables unify with unquantified
    // variables, but no concrete types
    Type* quantified = table->createTypeVariable("S", true);
    Type* unquantified = table->createTypeVariable("T");
    REQUIRE(isCompatible(quantified, unquantified));
    REQUIRE(!isCompatible(quantified, table->Int));

    // Quantified variables don't unify with each other because they can't
    // be subsitutued
    Type* quantified2 = table->createTypeVariable("U", true);
    REQUIRE(!isCompatible(quantified, quantified2));

    // Constrained case: quantified type variables only unify with unquantified
    // type variables with no more constraints
    quantified->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(isCompatible(quantified, unquantified));
    unquantified->get<TypeVariable>()->addConstraint(table->Num);
    REQUIRE(isCompatible(quantified, unquantified));
    unquantified->get<TypeVariable>()->addConstraint(table->createTrait("Signed"));
    REQUIRE(!isCompatible(quantified, unquantified));
}