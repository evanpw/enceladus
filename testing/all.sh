#!/bin/bash
set -e

if [ $(uname -s) == "Linux" ]; then
    platform=linux
else
    platform=osx
fi

$(dirname $0)/fast.sh
$(dirname $0)/medium.sh

# Slow tests (>= 1s)
testing/test.sh $platform euler10 142913828922 || echo "Project Euler 10 failed!"
testing/test.sh $platform euler14 837799 || echo "Project Euler 14 failed!"
testing/test.sh $platform euler23 4179871 || echo "Project Euler 23 failed!"
testing/test.sh $platform euler24 2783915460 || echo "Project Euler 24 failed!"
testing/test.sh $platform euler27 -59231 || echo "Project Euler 27 failed!"
