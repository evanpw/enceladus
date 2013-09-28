#!/bin/bash
set -x

mkdir -p build

./parser testing/$1.spl > build/$1.asm
nasm -fmacho build/$1.asm -o build/$1.o
nasm -fmacho library.asm -o build/library.o
gcc -m32 -Wl,-no_pie build/$1.o build/library.o -o build/$1
