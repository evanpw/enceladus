#!/bin/bash
set -e
set -o pipefail

platform=$1
program=$2
correct=$3

$platform/build.sh $program > /dev/null
value=$(build/$program)

[ "x$value" == "x$correct" ]
