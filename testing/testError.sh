#!/bin/bash

platform=$1
program=$2
correct="$3"

$platform/build.sh $program 2>&1 | ggrep -q -P "$correct"
