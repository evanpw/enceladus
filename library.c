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

void _List_sInt_e_decref(long* list)
{
    if (list == NULL) return;

    while (_decrefNoFree(list) == 0)
    {
        long* next = (long*)*(list + 2);
        free(list);
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

    return newCell;
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
        return *(tree + 4);
    }
}

long* Node(long value, long* left, long* right)
{
    long* newTree = (long*)malloc(40);

    *newTree = 0;
    *(newTree + 1) = value;
    *(newTree + 2) = (long)left;
    *(newTree + 3) = (long)right;
    *(newTree + 4) = 1 + count(left) + count(right);

    _incref(left);
    _incref(right);

    return newTree;
}

long* Empty()
{
    return NULL;
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
