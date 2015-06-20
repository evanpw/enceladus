#!/bin/bash
set -e

mkdir -p build

./simplec testing/$1.spl > build/$1.asm
nasm -fmacho64 build/$1.asm -o build/$1.o
gcc -O2 -c lib/library.c -o build/library.o -O -DNDEBUG
nasm -fmacho64 lib/gc.asm -d__APPLE__ -o build/gc.o
gcc -m64 -Wl,-no_pie build/$1.o build/library.o build/gc.o -o build/$1
