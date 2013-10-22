#!/bin/bash
set -e
set -o pipefail

platform=$1
index=$2
correct=$3

$platform/build.sh euler$index > /dev/null
value=$(build/euler$index)

[ "x$value" == "x$correct" ]
