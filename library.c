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

void _incref(long* p)
{
    if (p == NULL) return;

    long* refCount = p - 1;
    ++(*refCount);
}

long _decrefNoFree(long* p)
{
    if (p == NULL) return 1;

    long* refCount = p - 1;
    --(*refCount);

    if (*refCount < 0)
    {
        _die(ERR_REF_NEG);
    }

    return *refCount;
}

void _List_decref(long* list)
{
    if (list == NULL) return;

    while (_decrefNoFree(list) == 0)
    {
        long* next = (long*)*(list + 1);
        free(list - 1);
        list = next;
    }
}

long* Cons(long value, long* next)
{
    long* newCell = (long*)malloc(24);

    *newCell = 0;
    *(newCell + 1) = value;
    *(newCell + 2) = (long)next;

    _incref(next);

    return newCell + 1;
}

long* Node(long value, long* left, long* right)
{
    long* newTree = (long*)malloc(32);

    *newTree = 0;
    *(newTree + 1) = value;
    *(newTree + 2) = (long)left;
    *(newTree + 3) = (long)right;

    _incref(left);
    _incref(right);

    return newTree + 1;
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

    return *tree;
}

long* left(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_LEFT_EMPTY);
    }

    return (long*)*(tree + 1);
}

long* right(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_RIGHT_EMPTY);
    }

    return (long*)*(tree + 2);
}

void _Tree_decref(long* tree)
{
startOver:
    if (tree == NULL) return;

    if (_decrefNoFree(tree) == 0)
    {
        long* leftChild = left(tree);
        long* rightChild = right(tree);

        free(tree - 1);

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
