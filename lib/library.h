#ifndef LIBRARY_H
#define LIBRARY_H

void* mymalloc(size_t size);
void myfree(void* p);

struct SplObject;

#define MAX_STRUCTURED_TAG      ((1L << 32) - 1)
#define STRING_TAG              (1L << 32)
#define FREE_BLOCK_TAG          INT64_MAX

#define SplObject_HEAD \
    size_t constructorTag; \
    int64_t refCount; \
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

#endif
