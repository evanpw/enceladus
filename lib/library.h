#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdint.h>
#include <stdlib.h>

#define MAX_STRUCTURED_TAG      ((1L << 32) - 1)
#define STRING_TAG              (MAX_STRUCTURED_TAG + 1)
#define ARRAY_TAG               (MAX_STRUCTURED_TAG + 2)

#define SplObject_HEAD \
    size_t constructorTag; \
    uint64_t numPointers;

typedef struct SplObject
{
    SplObject_HEAD
} SplObject;

// WARNING: This layout is only valid for structures with T an unboxed type
typedef struct List
{
    SplObject_HEAD
    struct List* next;
    void* value;
} List;

typedef SplObject String;
typedef SplObject Array;

extern void* Some(void* value) asm("Some$A6StringE");
extern void* None() asm("None$A6StringE");

//extern void* Some(void* value);
//extern void* None();

extern void* splcall0(void* f) asm("splcall0");
extern void* splcall1(void* f, void* p1) asm("splcall1");
extern void* splcall2(void* f, void* p1, void* p2) asm("splcall2");
extern void* splcall3(void* f, void* p1, void* p2, void* p3) asm("splcall3");
extern void* splcall4(void* f, void* p1, void* p2, void* p3, void* p4) asm("splcall4");
extern void* splcall5(void* f, void* p1, void* p2, void* p3, void* p4, void* p5) asm("splcall5");

extern void addRoot(uint64_t* array, void** root) asm("addRoot");
extern void removeRoots(uint64_t* array) asm("removeRoots");

extern void* gcAllocate(size_t) asm("gcAllocateFromC");

#endif
