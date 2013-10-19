#!/bin/bash
set -x
set -e

mkdir -p build

./simplec testing/$1.spl > build/$1.asm
nasm -felf64 build/$1.asm -o build/$1.o
nasm -felf64 linux/library.asm -o build/library.o
gcc build/$1.o build/library-linux.o -o build/$1
