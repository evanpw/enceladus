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

        default:
            puts("*** Exception: Unknown error");
            break;
    }

    exit(1);
}

/*
long _read()
{
    long result;
    scanf("%ld", &result);

    return result;
}
*/

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

void _decref(long* p)
{
    if (p == NULL) return;

    long* refCount = p - 1;
    --(*refCount);

    if (*refCount == 0)
    {
        void* next = p + 1;
        free(refCount);

        _decref(next);
    }
    else if (*refCount < 0)
    {
        _die(ERR_REF_NEG);
    }
}

void _decrefNoFree(long* p)
{
    if (p == NULL) return;

    long* refCount = p - 1;
    --(*refCount);

    if (*refCount < 0)
    {
        _die(ERR_REF_NEG);
    }
}

long* cons(long value, long* next)
{
    long* newCell = (long*)malloc(24);

    *newCell = 0;
    *(newCell + 1) = value;
    *(newCell + 2) = (long)next;

    _incref(newCell + 1);

    return newCell + 1;
}

