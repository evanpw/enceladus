#!/bin/bash
set -e

mkdir -p build

./simplec testing/$1.spl > build/$1.asm
nasm -fmacho64 build/$1.asm -o build/$1.o
nasm -fmacho64 osx/library.asm -o build/library_asm.o
gcc -c osx/library.c -o build/library_c.o
gcc -m64 -Wl,-no_pie build/$1.o build/library_c.o build/library_asm.o -o build/$1
