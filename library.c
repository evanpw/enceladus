#include <stdio.h>
#include <stdlib.h>

void _main(void);

int main(void)
{
    _main();
    return 0;
}

static const long ERR_HEAD_EMPTY = 0;
static const long ERR_TAIL_EMPTY = 1;
static const long ERR_REF_NEG = 2;
static const long ERR_TOP_EMPTY = 3;
static const long ERR_LEFT_EMPTY = 4;
static const long ERR_RIGHT_EMPTY = 5;

void _die(long errorCode)
{
    switch(errorCode)
    {
        case ERR_HEAD_EMPTY:
            puts("*** Exception: Called head on empty list");
            break;

        case ERR_TAIL_EMPTY:
            puts("*** Exception: Called tail on empty list");
            break;

        case ERR_REF_NEG:
            puts("*** Exception: Reference count is negative");
            break;

        case ERR_TOP_EMPTY:
            puts("*** Exception: Called top on empty tree");
            break;

        case ERR_LEFT_EMPTY:
            puts("*** Exception: Called left on empty tree");
            break;

        case ERR_RIGHT_EMPTY:
            puts("*** Exception: Called right on empty tree");
            break;

        default:
            puts("*** Exception: Unknown error");
            break;
    }

    exit(1);
}

long read()
{
    long result;
    scanf("%ld", &result);

    return result;
}

void print(long value)
{
    printf("%ld\n", value);
}

//// Reference counting ////////////////////////////////////////////////////////

void _incref(long* p)
{
    if (p == NULL) return;

    ++(*p);
}

long _decrefNoFree(long* p)
{
    if (p == NULL) return 1;

    --(*p);

    if (*p < 0)
    {
        _die(ERR_REF_NEG);
    }

    return *p;
}

void _decref(long* object)
{
    if (object == NULL) return;

    if (_decrefNoFree(object) == 0)
    {
        long pointerCount = *(object + 1) >> 32;

        long* child = object + 2;
        for (int i = 0; i < pointerCount; ++i)
        {
            _decref((long*)*child);
            ++child;
        }

        free(object);
    }
}

//// Lists /////////////////////////////////////////////////////////////////////

long* Cons(long value, long* next)
{
    long* newCell = (long*)malloc(32);

    *(newCell + 0) = 0;               // Reference count
    *(newCell + 1) = (1L << 32) + 1;   // Number of pointers; non-pointers
    *(newCell + 2) = (long)next;
    *(newCell + 3) = value;

    _incref(next);

    return newCell;
}

//// Trees /////////////////////////////////////////////////////////////////////

long count(long*);

long* Node(long value, long* left, long* right)
{
    long* newTree = (long*)malloc(48);

    *(newTree + 0) = 0;              // Reference count
    *(newTree + 1) = (2L << 32) + 2; // Number of pointsers; non-pointers
    *(newTree + 2) = (long)left;     // Left child
    *(newTree + 3) = (long)right;    // Right child
    *(newTree + 4) = value;          // Value of this node
    *(newTree + 5) = 1 + count(left) + count(right); // Nodes in this subtree

    _incref(left);
    _incref(right);

    return newTree;
}

long* Empty()
{
    return NULL;
}

long top(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_TOP_EMPTY);
    }

    return *(tree + 4);
}

long* left(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_LEFT_EMPTY);
    }

    return (long*)*(tree + 2);
}

long* right(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_RIGHT_EMPTY);
    }

    return (long*)*(tree + 3);
}

long count(long* tree)
{
    if (tree == NULL)
    {
        return 0;
    }
    else
    {
        return *(tree + 5);
    }
}

