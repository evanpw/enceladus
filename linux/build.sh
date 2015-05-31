#!/bin/bash
set -e

mkdir -p build

./simplec testing/$1.spl > build/$1.asm
nasm -felf64 build/$1.asm -o build/$1.o
gcc -std=gnu99 -c lib/library.c -o build/library.o
nasm -felf64 lib/gc.asm -o build/gc.o
gcc build/$1.o build/library.o build/gc.o -o build/$1
