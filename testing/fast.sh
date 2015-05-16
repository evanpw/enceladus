#!/bin/bash
set -e

if [ $(uname -s) == "Linux" ]; then
    platform=linux
else
    platform=osx
fi

# Fast tests (< 100ms)
testing/test.sh $platform euler1 233168 || echo "Project Euler 1 failed!"
testing/test.sh $platform euler2 4613732 || echo "Project Euler 2 failed!"
testing/test.sh $platform euler3 6857 || echo "Project Euler 3 failed!"
testing/test.sh $platform euler5 232792560 || echo "Project Euler 5 failed!"
testing/test.sh $platform euler6 25164150 || echo "Project Euler 6 failed!"
testing/test.sh $platform euler7 104743 || echo "Project Euler 7 failed!"
testing/test.sh $platform euler8 40824 || echo "Project Euler 8 failed!"
testing/test.sh $platform euler9 31875000 || echo "Project Euler 9 failed!"
testing/test.sh $platform euler11 70600674 || echo "Project Euler 11 failed!"

$platform/build.sh euler13 > /dev/null && value=$(cat testing/euler13.txt | build/euler13)
[ "x$value" == "x5537376230" ] || echo "Project Euler 13 failed!"

testing/test.sh $platform euler15 137846528820 || echo "Project Euler 15 failed!"
testing/test.sh $platform euler16 1366 || echo "Project Euler 16 failed!"
testing/test.sh $platform euler17 21124 || echo "Project Euler 17 failed!"

$platform/build.sh euler18 > /dev/null && value=$(cat testing/euler18.txt | build/euler18)
[ "x$value" == "x1074" ] || echo "Project Euler 18 failed!"

testing/test.sh $platform euler20 648 || echo "Project Euler 20 failed!"
testing/test.sh $platform euler21 31626 || echo "Project Euler 21 failed!"

$platform/build.sh euler22 > /dev/null && value=$(cat testing/names.txt | build/euler22)
[ "x$value" == "x871198282" ] || echo "Project Euler 22 failed!"

testing/test.sh $platform poly 4 || echo "Polymorphism test failed!"
testing/test.sh $platform poly2 False || echo "Polymorphism test 2 failed!"
testing/test.sh $platform fail "\*\*\* Exception: Called head on empty list" || echo "Exception test failed!"
testing/test.sh $platform unaryMinus "\-5" || echo "Unary minus test failed!"
testing/test.sh $platform localVar 3 || echo "Local variable test failed!"
testing/test.sh $platform unnamed 19 || echo "Unnamed variable test failed!"
testing/test.sh $platform typeConstructor 7 || echo "Type constructor test failed!"
testing/test.sh $platform associativity 10 || echo "Associativity test failed!"
testing/test.sh $platform closure 7 || echo "Closure test failed!"
testing/testError.sh $platform structMembers1 'Error: Near line 3, column 5: symbol "x" is already defined' || echo "Struct members test 1 failed!"
testing/testError.sh $platform structMembers2 'Error: Near line 7, column 5: symbol "y" is already defined' || echo "Struct members test 2 failed!"
testing/testError.sh $platform structMembers3 'Error: Near line 5, column 1: symbol "x" is already defined in this scope' || echo "Struct members test 3 failed!"
testing/test.sh $platform structInference 7 || echo "Struct type inference test failed!"
#testing/test.sh $platform bigList '^$' || echo "Big list test failed!"
testing/test.sh $platform multiRef 6 || echo "Multiple reference test failed!"
testing/test.sh $platform shortCircuit Hello || echo "Short-circuit test failed!"
testing/test.sh $platform localopt 22 || echo "Local optimization test failed!"
testing/test.sh $platform functionArg 12 || echo "Function argument test failed!"
testing/test.sh $platform functionArg2 12 || echo "Function argument test 2 failed!"
testing/testError.sh $platform importSemantic 'Error: Near line 4, column 1: error: cannot unify types Bool and Int' || echo "Import + semantic error test failed!"
testing/testError.sh $platform syntaxError 'Error: Near line 1, column 2: expected tEOL, but got =' || echo "Syntax error test failed!"
testing/testError.sh $platform constructorMismatch 'Error: Near line 3, column 10: Expected 1 parameter\(s\) to type constructor MyPair, but got 2' || echo "Constructor mismatch test failed!"
testing/testError.sh $platform overrideType 'Error: Near line 2, column 16: error: cannot unify types Int and a\d+' || echo "Rigid type variable test failed!"
testing/test.sh $platform noReturn 1 || echo "No-return test 1 failed!"
testing/testError.sh $platform noReturn2 'Error: Near line 1, column 0: error: cannot unify types Unit and Int' || echo "No-return test 2 failed!"
testing/testError.sh $platform noReturn3 'Error: Near line 1, column 0: error: cannot unify types Unit and Int' || echo "No-return test 3 failed!"
testing/test.sh $platform implicitReturn 4 || echo "Implicit return test 1 failed!"
testing/test.sh $platform implicitReturn2 10 || echo "Implicit return test 2 failed!"
testing/testFile.sh $platform fizzBuzz testing/fizzBuzz.correct || echo "FizzBuzz test failed!"
testing/test.sh $platform adt1 5 || echo "ADT test 1 failed!"
testing/test.sh $platform adt2 2 || echo "ADT test 2 failed!"
testing/testError.sh $platform adt3 'Error: Near line 6, column 5: cannot repeat constructors in match statement' || echo "ADT test 3 failed!"
testing/testError.sh $platform adt4 'Error: Near line 4, column 1: switch statement is not exhaustive' || echo "ADT test 4 failed!"
