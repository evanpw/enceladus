#!/bin/bash
set -e

if [ $(uname -s) == "Linux" ]; then
    platform=linux
else
    platform=osx
fi

testing/test.sh $platform euler1 233168 || echo "Project Euler 1 failed!"
testing/test.sh $platform euler2 4613732 || echo "Project Euler 2 failed!"
testing/test.sh $platform euler3 6857 || echo "Project Euler 3 failed!"
testing/test.sh $platform euler4 906609 || echo "Project Euler 4 failed!"
testing/test.sh $platform euler5 232792560 || echo "Project Euler 5 failed!"
testing/test.sh $platform euler6 25164150 || echo "Project Euler 6 failed!"
testing/test.sh $platform euler7 104743 || echo "Project Euler 7 failed!"
testing/test.sh $platform euler8 40824 || echo "Project Euler 8 failed!"
testing/test.sh $platform euler9 31875000 || echo "Project Euler 9 failed!"
testing/test.sh $platform euler10 142913828922 || echo "Project Euler 10 failed!"
testing/test.sh $platform euler11 70600674 || echo "Project Euler 11 failed!"
testing/test.sh $platform euler12 76576500 || echo "Project Euler 12 failed!"

$platform/build.sh euler13 > /dev/null && value=$(cat testing/euler13.txt | build/euler13)
[ "x$value" == "x5537376230" ] || echo "Project Euler 13 failed!"

testing/test.sh $platform euler14 837799 || echo "Project Euler 13 failed!"
testing/test.sh $platform euler15 137846528820 || echo "Project Euler 15 failed!"
testing/test.sh $platform euler16 1366 || echo "Project Euler 16 failed!"
testing/test.sh $platform euler17 21124 || echo "Project Euler 17 failed!"

$platform/build.sh euler18 > /dev/null && value=$(cat testing/euler18.txt | build/euler18)
[ "x$value" == "x1074" ] || echo "Project Euler 18 failed!"

testing/test.sh $platform euler19 171 || echo "Project Euler 19 failed!"
testing/test.sh $platform euler20 648 || echo "Project Euler 20 failed!"

testing/test.sh $platform poly 4 || echo "Polymorphism test failed!"
testing/test.sh $platform poly2 "False" || echo "Polymorphism test 2 failed!"
testing/test.sh $platform fail "*** Exception: Called tail on empty list" || echo "Exception test failed!"
testing/test.sh $platform unaryMinus "-5" || echo "Unary minus test failed!"
testing/test.sh $platform localVar "3" || echo "Local variable test failed!"
testing/test.sh $platform unnamed "19" || echo "Unnamed variable test failed!"
testing/test.sh $platform typeConstructor "6" || echo "Type constructor test failed!"

