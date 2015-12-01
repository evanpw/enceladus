#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdint.h>
#include <stdlib.h>

#define MAX_STRUCTURED_TAG      ((1L << 32) - 1)
#define UNBOXED_ARRAY_TAG       (MAX_STRUCTURED_TAG + 1)
#define BOXED_ARRAY_TAG         (MAX_STRUCTURED_TAG + 2)

#define SplObject_HEAD \
    uint64_t constructorTag; \
    uint64_t refMask;

typedef struct SplObject
{
    SplObject_HEAD
} SplObject;

typedef struct List
{
    SplObject_HEAD
    struct List* next;
    uint8_t value;
} List;

typedef struct Array
{
    uint64_t constructorTag;    // Either BOXED_ARRAY_TAG or UNBOXED_ARRAY_TAG
    uint64_t numElements;       // Number of elements in the array (instead of refMask)
} Array;

typedef Array String;

extern void* enccall0(void* f) asm("enccall0");
extern void* enccall1(void* f, void* p1) asm("enccall1");
extern void* enccall2(void* f, void* p1, void* p2) asm("enccall2");
extern void* enccall3(void* f, void* p1, void* p2, void* p3) asm("enccall3");
extern void* enccall4(void* f, void* p1, void* p2, void* p3, void* p4) asm("enccall4");
extern void* enccall5(void* f, void* p1, void* p2, void* p3, void* p4, void* p5) asm("enccall5");

extern void addRoot(uint64_t* array, void** root) asm("addRoot");
extern void removeRoots(uint64_t* array) asm("removeRoots");

extern void* gcAllocate(size_t) asm("gcAllocateFromC");

#endif

