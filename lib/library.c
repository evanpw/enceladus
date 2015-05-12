// For non-standard getline
#define _GNU_SOURCE

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library.h"

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

void fail(const char* str)
{
    puts(str);
    exit(1);
}

//// Reference counting ////////////////////////////////////////////////////////

#define Spl_INCREF(p) _incref((SplObject*)(p))
#define Spl_DECREF(p) _decref((SplObject*)(p))
#define Spl_DECREF_NO_FREE(p) _decrefNoFree((SplObject*)(p))

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
        fail("*** Exception: Reference count is negative");
    }

    return object->refCount;
}

static void destroy(SplObject* object);

void _decref(SplObject* object)
{
    if (object == NULL || IS_TAGGED(object)) return;

    --object->refCount;
    if (object->refCount < 0)
    {
        fail("*** Exception: Reference count is negative");
    }
    else if (object->refCount > 0)
    {
        return;
    }

    if (object->constructorTag <= MAX_STRUCTURED_TAG)
    {
        destroy(object);
    }
    else
    {
        free(object);
    }
}

// Recursively destroy object and decrement the reference count of its children.
// Does a depth-first traversal of the object graph in constant stack space by
// storing back-tracking pointers in the child pointers themselves.
// See "Deutsch-Schorr-Waite pointer reversal"
static void destroy(SplObject* object)
{
    if (object->refCount != 0)
    {
        fail("*** Exception: Destroying object with positive reference count");
    }

    SplObject* back = NULL;
    SplObject* next = object;

    while (1)
    {
continue_main_loop:
        if (next->pointerFields)
        {
            uint64_t mask = 1;
            SplObject** p = (SplObject**)(next + 1);

            while (next->pointerFields)
            {
                if (next->pointerFields & mask)
                {
                    // Decrement child. If also at refcount 0, then recurse
                    if (_decrefNoFree(*p) == 0)
                    {
                        // Rotate back, next, *p cyclically to the left
                        SplObject* tmp = back;
                        back = next;
                        next = *p;
                        *p = tmp;
                        goto continue_main_loop;
                    }
                    else
                    {
                        next->pointerFields &= ~mask;
                    }
                }

                mask <<= 1;
                ++p;
            }
        }

        free(next);

        if (back)
        {
            // Backtrack
            next = back;

            uint64_t mask = 1;
            SplObject** p = (SplObject**)(next + 1);
            while (!(next->pointerFields & mask))
            {
                mask <<= 1;
                ++p;
            }

            next->pointerFields &= ~mask;
            back = *p;
        }
        else
        {
            break;
        }
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

List* Cons(void* value, List* next);
List* Nil();
#define IS_EMPTY(xs) ((xs)->constructorTag == 1)

String* makeString(const char* s)
{
    List* str = Nil();

    for (int i = strlen(s) - 1; i >= 0; --i)
    {
        int64_t c = toInt(s[i]);
        str = Cons((void*)c, str);
    }

    return str;
}

size_t get_length(String* s)
{
    String* original = s;
    Spl_INCREF(original);

    size_t length = 0;

    while (!IS_EMPTY(s))
    {
        ++length;
        s = s->next;
    }

    Spl_DECREF(original);
    return length;
}

// Should only ever be called by print function
char* content(String* s)
{
    String* original = s;

    char* str = (char*)malloc(get_length(s) + 1);

    char* out = str;
    while (!IS_EMPTY(s))
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
    scanf("%" PRId64, &result);

    return toInt(result);
}

String* readLine()
{
    char* line = NULL;
    size_t len = 0;

    ssize_t read = getline(&line, &len, stdin);

    if (read == -1)
    {
        return Nil();
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
    Spl_INCREF(s);
    char* str = content(s);

    printf("%s\n", str);

    free(str);
    Spl_DECREF(s);
}

void die(String* s)
{
    print(s);
    exit(1);
}

//// Trees /////////////////////////////////////////////////////////////////////

Tree* Empty()
{
    return NULL;
}
