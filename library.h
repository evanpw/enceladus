#ifndef LIBRARY_H
#define LIBRARY_H

struct SplObject;

#define SplObject_HEAD \
    int64_t refCount; \
    void (*destructor)(struct SplObject* object);

#define IS_TAGGED(p) ((int64_t)p & 0x3)

typedef struct SplObject
{
    SplObject_HEAD
} SplObject;

typedef struct List
{
    SplObject_HEAD
    struct List* next;
    void* value;
} List;

typedef struct Tree
{
    SplObject_HEAD
    struct Tree* left;
    struct Tree* right;
    int64_t value;
    int64_t count;
} Tree;

typedef List String;

#endif
