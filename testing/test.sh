#!/bin/bash

platform=$1
program=$2
correct=$3

$platform/build.sh $program > /dev/null || exit 1
value=$(build/$program)

[ "x$value" == "x$correct" ]
