#ifndef LIBRARY_H
#define LIBRARY_H

void* mymalloc(size_t size);
void myfree(void* p);

struct SplObject;

#define MAX_STRUCTURED_TAG      ((1L << 32) - 1)
#define STRING_TAG              (1L << 32)

#define SplObject_HEAD \
    int64_t refCount; \
    size_t constructorTag; \
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

typedef struct Tree
{
    SplObject_HEAD
    struct Tree* left;
    struct Tree* right;
    int64_t value;
    int64_t count;
} Tree;

typedef SplObject String;

#endif
