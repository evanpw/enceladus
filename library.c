#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


#define ERR_HEAD_EMPTY      0
#define ERR_TAIL_EMPTY      1
#define ERR_REF_NEG         2
#define ERR_TOP_EMPTY       3
#define ERR_LEFT_EMPTY      4
#define ERR_RIGHT_EMPTY     5
#define ERR_OUT_OF_BOUNDS   6

void _die(int64_t errorCode)
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

        case ERR_OUT_OF_BOUNDS:
            puts("*** Exception: Index passed to charAt is out of range");
            break;

        default:
            puts("*** Exception: Unknown error");
            break;
    }

    exit(1);
}

void _dieWithMessage(const char* str)
{
    puts(str);
    exit(1);
}

//// Various structs ///////////////////////////////////////////////////////////
#define SplObject_HEAD \
    int64_t refCount; \
    uint32_t numScalars; \
    uint32_t numPointers; \

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

//// Reference counting ////////////////////////////////////////////////////////

#define Spl_INCREF(p) _incref((SplObject*)(p))

void _incref(SplObject* object)
{
    if (object == NULL || IS_TAGGED(object)) return;

    ++(object->refCount);
}

int64_t _decrefNoFree(SplObject* object)
{
    if (object == NULL || IS_TAGGED(object)) return 1;

    --object->refCount;

    if (object->refCount < 0)
    {
        _dieWithMessage("*** Exception: Reference count is negative");
    }

    return object->refCount;
}

void _destroy(SplObject* object);

void _decref(SplObject* object)
{
    if (object == NULL || IS_TAGGED(object)) return;

    if (_decrefNoFree(object) == 0)
    {
        _destroy(object);
    }
}

void _destroy(SplObject* object)
{
    SplObject** child = (SplObject**)(object + 1);
    for (int i = 0; i < object->numPointers; ++i)
    {
        _decref(*child);
        ++child;
    }

    free(object);
}

//// Ints //////////////////////////////////////////////////////////////////////

int64_t toInt(int64_t n)
{
    return (n << 1) + 1;
}

int64_t fromInt(int64_t n)
{
    return n >> 1;
}

//// Strings ///////////////////////////////////////////////////////////////////

List* Cons(void* value, List* next);

String* makeString(const char* s)
{
    List* str = NULL;

    for (int i = strlen(s) - 1; i >= 0; --i)
    {
        int64_t c = toInt(s[i]);
        str = Cons((void*)c, str);
    }

    return str;
}

size_t get_length(String* s)
{
    size_t length = 0;

    while (s != NULL)
    {
        ++length;
        s = s->next;
    }

    return length;
}

char* content(String* s)
{
    char* str = (char*)malloc(get_length(s) + 1);

    char* out = str;
    while (s != NULL)
    {
        *out = fromInt((int64_t)s->value);

        ++out;
        s = s->next;
    }

    *out = '\0';

    return str;
}

//// I/O ///////////////////////////////////////////////////////////////////////

int64_t read()
{
    int64_t result;
    scanf("%lld", &result);

    return toInt(result);
}

String* readLine()
{
    char* line = NULL;
    size_t len = 0;

    ssize_t read = getline(&line, &len, stdin);

    if (read == -1)
    {
        return NULL;
    }
    else
    {
        String* result = makeString(line);
        free(line);

        return result;
    }
}

void print(String* s)
{
    char* str = content(s);

    printf("%s\n", str);

    free(str);
}

void dieWithMessage(String* s)
{
    print(s);
    exit(1);
}

//// Lists /////////////////////////////////////////////////////////////////////

List* Cons(void* value, List* next)
{
    List* newCell = (List*)malloc(sizeof(List));

    newCell->refCount = 0;
    newCell->numScalars = 0;
    newCell->numPointers = 2;
    newCell->next = next;
    newCell->value = value;

    Spl_INCREF(next);
    Spl_INCREF(value);

    return newCell;
}

//// Trees /////////////////////////////////////////////////////////////////////

Tree* Empty()
{
    return NULL;
}
