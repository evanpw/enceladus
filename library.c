#include <stdio.h>
#include <stdlib.h>

extern void _main(void) asm("__main");

int main(void)
{
    _main();
    return 0;
}

#define ERR_HEAD_EMPTY  0
#define ERR_TAIL_EMPTY  1
#define ERR_REF_NEG     2
#define ERR_TOP_EMPTY   3
#define ERR_LEFT_EMPTY  4
#define ERR_RIGHT_EMPTY 5

extern void _die(long) asm ("__die");
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

extern long read() asm ("_read");
long read()
{
    long result;
    scanf("%ld", &result);

    return result;
}

extern void print(long) asm ("_print");
void print(long value)
{
    printf("%ld\n", value);
}

extern void _incref(long*) asm ("__incref");
void _incref(long* p)
{
    if (p == NULL) return;

    ++(*p);
}

extern long _decrefNoFree(long*) asm ("__decrefNoFree");
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

extern void _List_decref(long*) asm ("__List_decref");
void _List_decref(long* list)
{
    if (list == NULL) return;

    while (_decrefNoFree(list) == 0)
    {
        long* next = (long*)*(list + 2);
        free(list);
        list = next;
    }
}

extern long* Cons(long, long*) asm ("_Cons");
long* Cons(long value, long* next)
{
    long* newCell = (long*)malloc(24);

    *newCell = 0;
    *(newCell + 1) = value;
    *(newCell + 2) = (long)next;

    _incref(next);

    return newCell;
}

extern long top(long*) asm("_top");
long top(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_TOP_EMPTY);
    }

    return *tree;
}

extern long* left(long*) asm("_left");
long* left(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_LEFT_EMPTY);
    }

    return (long*)*(tree + 2);
}

extern long* right(long*) asm("_right");
long* right(long* tree)
{
    if (tree == NULL)
    {
        _die(ERR_RIGHT_EMPTY);
    }

    return (long*)*(tree + 3);
}

extern long count(long*) asm("_count");
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

extern long* Node(long, long*, long*) asm("_Node");
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

extern long* Empty() asm ("_Empty");
long* Empty()
{
    return NULL;
}

extern void _Tree_decref(long*) asm("__Tree_decref");
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
