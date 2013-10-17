#!/bin/bash
set -x
set -e

mkdir -p build

./simplec testing/$1.spl > build/$1.asm
nasm -fmacho64 build/$1.asm -o build/$1.o
nasm -fmacho64 library.asm -o build/library.o
gcc -m64 -Wl,-no_pie build/$1.o build/library.o -o build/$1
