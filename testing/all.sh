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
testing/test.sh $platform fail "*** Exception: Called head on empty list" || echo "Exception test failed!"
