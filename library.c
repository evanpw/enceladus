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

typedef struct String
{
    SplObject_HEAD
    int64_t length;
} String;

typedef struct Tree
{
    SplObject_HEAD
    struct Tree* left;
    struct Tree* right;
    int64_t value;
    int64_t count;
} Tree;

//// Reference counting ////////////////////////////////////////////////////////

#define Spl_INCREF(p) _incref((SplObject*)(p))
void _incref(SplObject* p)
{
    if (p == NULL || ((int64_t)p & 0x3)) return;

    ++(p->refCount);
}

int64_t _decrefNoFree(int64_t* p)
{
    if (p == NULL || ((int64_t)p & 0x3)) return 1;

    --(*p);

    if (*p < 0)
    {
        _dieWithMessage("*** Exception: Reference count is negative");
    }

    return *p;
}

void _decref(int64_t* object)
{
    if (object == NULL || ((int64_t)object & 0x3)) return;

    if (_decrefNoFree(object) == 0)
    {
        int64_t pointerCount = *(object + 1) >> 32;

        int64_t* child = object + 2;
        for (int i = 0; i < pointerCount; ++i)
        {
            _decref((int64_t*)*child);
            ++child;
        }

        free(object);
    }
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

static inline char* content(String* s)
{
    if (s == NULL)
    {
        _dieWithMessage("*** Exception: Called content on null string");
    }

    return (char*)(s + 1);
}

static String* makeString(const char* s)
{
    size_t length = strlen(s);

    String* result = malloc(sizeof(String) + length + 1);
    result->refCount = 0;
    result->numScalars = 1;
    result->numPointers = 0;
    result->length = toInt(length);

    strcpy(content(result), s);

    return result;
}

void echo(String* s)
{
    printf("%s\n", content(s));
}

int64_t len(String* s)
{
    if (s == NULL)
    {
        _dieWithMessage("*** Exception: Called len on null string");
    }

    return s->length;
}

int64_t charAt(int64_t i, String* s)
{
    int64_t n = fromInt(i);

    int64_t length = fromInt(len(s));
    if (n >= length)
    {
        _dieWithMessage("*** Exception: Index passed to charAt is out of range");
    }

    const char* str = content(s);
    return toInt((int64_t)(str[n]));
}

String* cat(String* p1, String* p2)
{
    int64_t length1 = fromInt(len(p1));
    int64_t length2 = fromInt(len(p2));

    int64_t newLength = length1 + length2;

    String* result = malloc(sizeof(String) + newLength + 1);
    result->refCount = 0;
    result->numScalars = 1;
    result->numPointers = 0;
    result->length = toInt(newLength);

    char* resultString = content(result);
    strncpy(resultString, content(p1), length1);
    strncpy(resultString + length1, content(p2), length2);
    resultString[length1 + length2] = 0;

    return result;
}

String* listToString(List* xs)
{
    size_t length = 0;
    List* p = xs;
    while (p != 0)
    {
        ++length;
        p = p->next;
    }

    String* result = malloc(sizeof(String) + length + 1);
    result->refCount = 0;
    result->numScalars = 0;
    result->numPointers = 1;
    result->length = toInt(length);

    char* resultString = content(result);

    p = xs;
    while (p != 0)
    {
        *resultString = (char)fromInt((int64_t)p->value);
        ++resultString;
        p = p->next;
    }

    return result;
}

void dieWithMessage(String* s)
{
    echo(s);
    exit(1);
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
    char * line = NULL;
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

void print(int64_t value)
{
    printf("%lld\n", fromInt(value));
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

int64_t count(Tree*);

Tree* Node(int64_t value, Tree* left, Tree* right)
{
    Tree* newTree = (Tree*)malloc(sizeof(Node));

    newTree->refCount = 0;
    newTree->numScalars = 2;
    newTree->numPointers = 2;
    newTree->left = left;
    newTree->right = right;
    newTree->value = value;

    int64_t leftCount = fromInt(count(left));
    int64_t rightCount = fromInt(count(right));
    int64_t myCount = toInt(1 + leftCount + rightCount);
    newTree->count = myCount;

    Spl_INCREF(left);
    Spl_INCREF(right);

    return newTree;
}

Tree* Empty()
{
    return NULL;
}

int64_t top(Tree* tree)
{
    if (tree == NULL)
    {
        _dieWithMessage("*** Exception: Called top on empty tree");
    }

    return tree->value;
}

Tree* left(Tree* tree)
{
    if (tree == NULL)
    {
        _dieWithMessage("*** Exception: Called left on empty tree");
    }

    return tree->left;
}

Tree* right(Tree* tree)
{
    if (tree == NULL)
    {
        _dieWithMessage("*** Exception: Called right on empty tree");
    }

    return tree->right;
}

int64_t count(Tree* tree)
{
    if (tree == NULL)
    {
        return 0;
    }
    else
    {
        return tree->count;
    }
}

