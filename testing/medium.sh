#!/bin/bash
set -e

if [ $(uname -s) == "Linux" ]; then
    platform=linux
else
    platform=osx
fi

# Medium tests (>= 100ms, < 1s)
testing/test.sh $platform euler4 906609 || echo "Project Euler 4 failed!"
testing/test.sh $platform euler12 76576500 || echo "Project Euler 12 failed!"
testing/test.sh $platform euler19 171 || echo "Project Euler 19 failed!"
testing/test.sh $platform euler19-2 171 || echo "Project Euler 19-2 failed!"
testing/test.sh $platform euler26 983 || echo "Project Euler 26 failed!"
testing/test.sh $platform euler25 4782 || echo "Project Euler 25 failed!"
