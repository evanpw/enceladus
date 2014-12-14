#!/bin/bash
set -e

mkdir -p build

./simplec testing/$1.spl > build/$1.asm
nasm -fmacho64 build/$1.asm -o build/$1.o
gcc -c lib/library.c -o build/library.o
gcc -m64 -Wl,-no_pie build/$1.o build/library.o -o build/$1
