#ifndef LIBRARY_H
#define LIBRARY_H

#ifdef __APPLE__
void* mymalloc(size_t) asm("mymalloc");
#endif

void* mymalloc(size_t);
void myfree(void* p);

struct SplObject;

#define MAX_STRUCTURED_TAG      ((1L << 32) - 1)
#define STRING_TAG              (1L << 32)
#define FREE_BLOCK_TAG          INT64_MAX

#define SplObject_HEAD \
    size_t constructorTag; \
    void* gcNext; \
    uint64_t markBit; \
    uint64_t pointerFields;

#define IS_TAGGED(p) ((int64_t)p & 0x3)

typedef struct SplObject
{
    SplObject_HEAD
} SplObject;

typedef struct List
{
    SplObject_HEAD
    void* value;
    struct List* next;
} List;

typedef SplObject String;

extern void* Some(void* value) asm("_Z4Some");
extern void* None() asm("_Z4None");

extern void* splcall0(void* f) asm("splcall0");
extern void* splcall1(void* f, void* p1) asm("splcall1");
extern void* splcall2(void* f, void* p1, void* p2) asm("splcall2");
extern void* splcall3(void* f, void* p1, void* p2, void* p3) asm("splcall3");
extern void* splcall4(void* f, void* p1, void* p2, void* p3, void* p4) asm("splcall4");
extern void* splcall5(void* f, void* p1, void* p2, void* p3, void* p4, void* p5) asm("splcall5");

extern void addRoot(uint64_t* array, void** root) asm("addRoot");
extern void removeRoots(uint64_t* array) asm("removeRoots");

#endif
