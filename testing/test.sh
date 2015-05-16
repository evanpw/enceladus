#!/bin/bash

platform=$1
program=$2
correct="$3"

$platform/build.sh $program || exit 1
build/$program | ggrep -P -q "^$correct$"
