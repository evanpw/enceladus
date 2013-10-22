#!/bin/bash
set -e

platform=$1
index=$2
correct=$3

$platform/build.sh euler$index > /dev/null 2>&1
value=$(build/euler$index)

if [ $value -ne $correct ]; then
    echo "Project Euler $index Failed!";
    exit 1
fi
