#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__

extern void _main(void);

int main(void)
{
    _main();
    return 0;
}

#else

extern void __main(void);

int main(void)
{
    __main();
    return 0;
}

#endif


#define ERR_HEAD_EMPTY  0
#define ERR_TAIL_EMPTY  1
#define ERR_REF_NEG     2
#define ERR_TOP_EMPTY   3
#define ERR_LEFT_EMPTY  4
#define ERR_RIGHT_EMPTY 5

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

//// I/O ///////////////////////////////////////////////////////////////////////

long read()
{
    long result;
    scanf("%ld", &result);

    return (result << 1) + 1;
}

void print(long value)
{
    printf("%ld\n", (value >> 1));
}

//// Reference counting ////////////////////////////////////////////////////////

void _incref(long* p)
{
    if (p == NULL || ((long)p & 0x3)) return;

    ++(*p);
}

long _decrefNoFree(long* p)
{
    if (p == NULL || ((long)p & 0x3)) return 1;

    --(*p);

    if (*p < 0)
    {
        _die(ERR_REF_NEG);
    }

    return *p;
}

void _decref(long* object)
{
    if (object == NULL || ((long)object & 0x3)) return;

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

long* Cons(long* value, long* next)
{
    long* newCell = (long*)malloc(32);

    *(newCell + 0) = 0;                // Reference count
    *(newCell + 1) = (2L << 32) + 0;   // Number of pointers; non-pointers
    *(newCell + 2) = (long)next;
    *(newCell + 3) = (long)value;

    _incref(next);
    _incref(value);

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

    long leftCount = count(left) >> 1;
    long rightCount = count(right) >> 1;
    long myCount = ((1 + leftCount + rightCount) << 1) + 1;
    *(newTree + 5) = myCount;        // Nodes in this subtree

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

void _Tree_decref(long* tree)
{
startOver:
    if (tree == NULL) return;

    if (_decrefNoFree(tree) == 0)
    {
        long* leftChild = left(tree);
        long* rightChild = right(tree);

        free(tree);

        // This hacked-in tail recursion is necessary to avoid a stack overflow
        // when the tree is very large and lopsided
        if (leftChild == NULL)
        {
            tree = rightChild;
            goto startOver;
        }
        else if (rightChild == NULL)
        {
            tree = leftChild;
            goto startOver;
        }
        else
        {
            _Tree_decref(leftChild);
            _Tree_decref(rightChild);
        }
    }
}
