#!/bin/bash
set -e

if [ $(uname -s) == "Linux" ]; then
    platform=linux
else
    platform=osx
fi

testing/euler.sh $platform 1 233168 || echo "Project Euler 1 Failed!"
testing/euler.sh $platform 2 4613732 || echo "Project Euler 2 Failed!"
testing/euler.sh $platform 3 6857 || echo "Project Euler 3 Failed!"
testing/euler.sh $platform 4 906609 || echo "Project Euler 4 Failed!"
testing/euler.sh $platform 5 232792560 || echo "Project Euler 5 Failed!"
testing/euler.sh $platform 6 25164150 || echo "Project Euler 6 Failed!"
testing/euler.sh $platform 7 104743 || echo "Project Euler 7 Failed!"
testing/euler.sh $platform 8 40824 || echo "Project Euler 8 Failed!"

echo "All tests successful!"
