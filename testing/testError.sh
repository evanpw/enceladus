#!/bin/bash

platform=$1
program=$2
correct=$3

value=$($platform/build.sh $program 2>&1)

[ "x$value" == "x$correct" ]
