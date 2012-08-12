./parser testing/euler1.spl > euler1.asm
nasm -fmacho euler1.asm -o euler1.o
nasm -fmacho library.asm -o library.o
gcc -m32 -Wl,-no_pie euler1.o library.o -o euler1
