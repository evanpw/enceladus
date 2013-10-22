#!/bin/bash
set -e

if [ $(uname -s) == "Linux" ]; then
    platform=linux
else
    platform=osx
fi

testing/euler.sh $platform 1 233168
testing/euler.sh $platform 2 4613732
testing/euler.sh $platform 3 6857
testing/euler.sh $platform 4 906609
testing/euler.sh $platform 5 232792560
testing/euler.sh $platform 6 25164150
testing/euler.sh $platform 7 104743

echo "All tests successful!"
