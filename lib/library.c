// For non-standard getline
#define _GNU_SOURCE

#include "library.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

void fail(const char* str)
{
    puts(str);
    exit(1);
}

//// Arrays ////////////////////////////////////////////////////////////////////

uint64_t* arrayContent(Array* s)
{
    return (uint64_t*)(s + 1);
}

Array* makeArray(int64_t n, uint64_t value)
{
    int64_t size = FROM_INT(n);

    if (size < 0) fail("*** Exception: Cannot create array of negative size");

    Array* result = gcAllocate(sizeof(SplObject) + n * 8);
    result->constructorTag = ARRAY_TAG;
    result->sizeInWords = size;

    uint64_t* p = arrayContent(result);
    for (size_t i = 0; i < (size_t)size; ++i)
    {
        *p++ = value;
    }

    return result;
}

uint64_t arrayAt(Array* arr, int64_t n)
{
    int64_t index = FROM_INT(n);

    if (index < 0 || index > arr->sizeInWords)
    {
        fail("*** Exception: Out-of-bounds array access");
    }

    uint64_t* p = arrayContent(arr);
    return p[index];
}

void arraySet(Array* arr, int64_t n, uint64_t value)
{
    int64_t index = FROM_INT(n);

    if (index < 0 || index > arr->sizeInWords)
    {
        fail("*** Exception: Out-of-bounds array access");
    }

    uint64_t* p = arrayContent(arr);
    p[index] = value;
}

//// Strings ///////////////////////////////////////////////////////////////////

char* strContent(String* s)
{
    return (char*)(s + 1);
}

String* makeStr(const char* data)
{
    String* result = gcAllocate(sizeof(SplObject) + strlen(data) + 1);
    result->constructorTag = STRING_TAG;
    result->sizeInWords = 0; // Unstructured data
    strcpy(strContent(result), data);

    return result;
}

int64_t strLength(String* s)
{
    return TO_INT(strlen(strContent(s)));
}

String* strSlice(String* s, int64_t tPos, int64_t tLength)
{
    int64_t pos = FROM_INT(tPos);
    int64_t length = FROM_INT(tLength);

    if (pos < 0 || pos >= strLength(s))
    {
        fail("*** Exception: String slice position out of range");
    }
    else if (length < 0 || pos + length >= strLength(s))
    {
        fail("*** Exception: String slice length out of range");
    }

    String* result = gcAllocate(sizeof(SplObject) + length + 1);
    result->constructorTag = STRING_TAG;
    result->sizeInWords = 0;

    strncpy(strContent(result), strContent(s) + pos, length);
    strContent(result)[length] = '\0';

    return result;
}

String* strCat(String* lhs, String* rhs)
{
    size_t n1 = strlen(strContent(lhs));
    size_t n2 = strlen(strContent(rhs));

    String* result = gcAllocate(sizeof(SplObject) + n1 + n2 + 1);
    result->constructorTag = STRING_TAG;
    result->sizeInWords = 0;

    char* dest = strContent(result);
    strncpy(dest, strContent(lhs), n1);
    strncpy(dest + n1, strContent(rhs), n2);
    *(dest + n1 + n2) = '\0';

    return result;
}

int64_t strAt(String* s, int64_t n)
{
    int64_t idx = FROM_INT(n);

    if (idx < 0 || idx >= strLength(s))
    {
        fail("*** Exception: String index out of range");
    }

    return TO_INT(strContent(s)[idx]);
}

#define IS_EMPTY(xs) ((xs)->constructorTag == 1)

String* strFromList(List* list)
{
    // Get length
    List* p = list;
    size_t length = 0;
    while (!IS_EMPTY(list))
    {
        list = list->next;
        ++length;
    }

    String* result = gcAllocate(sizeof(SplObject) + length + 1);
    result->constructorTag = STRING_TAG;
    result->sizeInWords = 0;

    char* out = strContent(result);
    while (!IS_EMPTY(list))
    {
        int64_t c = FROM_INT((int64_t)list->value);
        if (c < 0 || c > 255)
        {
            fail("*** Exception: Char value out of range");
        }

        *out++ = c;
        list = list->next;
    }

    *out++ = '\0';

    return result;
}

String* show(int64_t x)
{
    int64_t value = FROM_INT(x);

    String* result = gcAllocate(sizeof(SplObject) + 20 + 1);
    result->constructorTag = STRING_TAG;
    result->sizeInWords = 0;

    sprintf(strContent(result), "%" PRId64, value);
    return result;
}


//// I/O ///////////////////////////////////////////////////////////////////////

int64_t read()
{
    int64_t result;
    int n = scanf("%" PRId64, &result);
    if (n != 1)
    {
        fail("*** Exception: Invalid input");
    }

    return TO_INT(result);
}

void* readLine()
{
    char* line = NULL;
    size_t len = 0;

    ssize_t read = getline(&line, &len, stdin);

    if (read == -1)
    {
        return splcall0(None);
    }
    else
    {
        String* result = makeStr(line);
        free(line);

        return splcall1(Some, result);
    }
}

void print(String* s)
{
    printf("%s\n", strContent(s));
}

void die(String* s)
{
    fprintf(stderr, "%s\n", strContent(s));
    exit(1);
}

//// Garbage collector /////////////////////////////////////////////////////////

// Cheney-style copying collector
uint64_t* heapStart;
uint64_t* heapPointer;
uint64_t* heapEnd;

uint64_t* otherStart;
uint64_t* otherEnd;

void initializeHeap() asm("initializeHeap");

void initializeHeap()
{
    size_t size = 4 << 20;

    heapStart = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (heapStart == MAP_FAILED)
    {
        fail("*** Exception: Cannot initialize heap");
    }
    heapPointer = heapStart;
    heapEnd = heapStart + size / sizeof(uint64_t);

    otherStart = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (otherStart == MAP_FAILED)
    {
        fail("*** Exception: Cannot initialize heap");
    }
    otherEnd = otherStart + size / sizeof(uint64_t);
}

// increment should be a power of 2
size_t roundUp(size_t size, size_t increment)
{
    if (size % increment)
    {
        return (size - size % increment) + increment;
    }
    else
    {
        return size;
    }
}

void expandHeap(size_t minimumSize)
{
    size_t currentSize = (otherEnd - otherStart) * sizeof(uint64_t);
    munmap(otherStart, currentSize);

    currentSize *= 2;
    if (currentSize < minimumSize)
    {
        currentSize = roundUp(minimumSize, 4096);
    }

    otherStart = mmap(0, currentSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (otherStart == MAP_FAILED)
    {
        fail("*** Exception: Cannot expand heap");
    }

    otherEnd = otherStart + (currentSize / sizeof(uint64_t));
}

// Make sure that the other heap has at least the same capacity as
// the current heap
void equalizeHeaps()
{
    size_t otherSize = (otherEnd - otherStart) * sizeof(uint64_t);
    size_t currentSize = (heapEnd - heapStart) * sizeof(uint64_t);
    if (otherSize >= currentSize) return;

    munmap(otherStart, otherSize);

    otherSize = currentSize;

    otherStart = mmap(otherStart, otherSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (otherStart == MAP_FAILED)
    {
        fail("*** Exception: Cannot expand heap");
    }

    otherEnd = otherStart + (otherSize / sizeof(uint64_t));
}

void* try_mymalloc(size_t) asm("try_mymalloc");

// Try to allocate memory from the current heap
void* try_mymalloc(size_t sizeInBytes)
{
     return malloc(sizeInBytes);

    // Allocate in units of 8 bytes
    size_t sizeInWords = (sizeInBytes + 7) / 8;

    // New allocation is at the current heapPointer
    uint64_t* p = heapPointer;
    heapPointer += (sizeInWords + 1);

    // Bump the pointer and check whether we jumped the end of the heap
    if (heapPointer >= heapEnd)
    {
        return NULL;
    }

    // The first word of the allocated block contains the size in words
    // (tagged so that we can distinguish it from a forwarding pointer)
    *p++ = ((sizeInWords << 1) | 1);

    return p;
}

uint64_t* allocPtr;
uint64_t* scanPtr;

void* gcCopy(void* object)
{
    assert((otherEnd - otherStart) >= (heapEnd - heapStart));

    // Back up one word to the beginning of the allocated block
    uint64_t* block = (uint64_t*)object - 1;
    uint64_t header = *block;
    if (!(header & 1))
    {
        // No tag => this is a forwarding pointer. Don't copy
        return (void*)header;
    }

    size_t sizeInWords = header >> 1;

    // Copy to the "to" space
    memcpy(allocPtr, block, (sizeInWords + 1) * sizeof(uint64_t));

    // Leave a forwarding address (to the object, not the block header)
    void* newLocation = allocPtr + 1;
    *block = (uint64_t)newLocation;

    allocPtr += (sizeInWords + 1);

    return newLocation;
}

void gcCopyRoots(uint64_t* stackTop, uint64_t* stackBottom, uint64_t* additionalRoots)
{
    uint64_t* top = stackTop;
    uint64_t* bottom = stackTop;

    while (1)
    {
        for (uint64_t* p = top; p < bottom; ++p)
        {
            SplObject* object = (SplObject*)*p;
            if (object && IS_REFERENCE(object))
            {
                void* newLocation = gcCopy(object);
                *p = (uint64_t)newLocation;
            }
        }

        if (bottom == stackBottom)
        {
            break;
        }

        top = bottom + 2;
        bottom = (uint64_t*)*bottom;
    }

    while (additionalRoots)
    {
        uint64_t numGlobals = *additionalRoots;
        uint64_t** p = (uint64_t**)(additionalRoots + 1);
        for (size_t i = 0; i < numGlobals; ++i)
        {
            SplObject* object = (SplObject*)**p;
            if (object && IS_REFERENCE(object))
            {
                void* newLocation = gcCopy(object);
                **p = (uint64_t)newLocation;
            }

            ++p;
        }

        additionalRoots = *p;
    }
}

void gcScan()
{
    while (scanPtr < allocPtr)
    {
        // Skip the size word to get to the next object on the scan list
        SplObject* object = (SplObject*)(scanPtr + 1);

        // Iterate over the children, and copy them to the new heap
        SplObject** p = (SplObject**)(object + 1);
        SplObject** pend = p + object->sizeInWords;
        while (p < pend)
        {
            if (IS_REFERENCE(*p))
            {
                void* newLocation = gcCopy(*p);
                *p = (SplObject*)newLocation;
            }

            ++p;
        }

        size_t sizeInWords = *scanPtr >> 1;
        scanPtr += (sizeInWords + 1);
    }
}

void gcCollect(uint64_t* stackTop, uint64_t* stackBottom, uint64_t* additionalRoots)
{
    allocPtr = otherStart;
    scanPtr = otherStart;

    gcCopyRoots(stackTop, stackBottom, additionalRoots);
    gcScan();

    // Swap the heaps
    uint64_t* tmpStart = heapStart;
    uint64_t* tmpEnd = heapEnd;

    heapStart = otherStart;
    heapPointer = allocPtr;
    heapEnd = otherEnd;

    otherStart = tmpStart;
    otherEnd = tmpEnd;
}

extern void* gcCollectAndAllocate(size_t, uint64_t*, uint64_t*, uint64_t*) asm("gcCollectAndAllocate");

void* gcCollectAndAllocate(size_t sizeInBytes, uint64_t* stackTop, uint64_t* stackBottom, uint64_t* additionalRoots)
{
    assert(otherEnd - otherStart >= heapEnd - heapStart);

    // This is minimum free size we need to satisfy this allocation
    size_t bytesNeeded = roundUp(sizeInBytes, 8) + 8;

    gcCollect(stackTop, stackBottom, additionalRoots);

    // If there's not much free space even after collection, increase the size
    // of the heap
    size_t totalSize = heapEnd - heapStart;
    size_t usedSize = heapPointer - heapStart;
    size_t freeSize = totalSize - usedSize;
    if (freeSize < 0.20 * totalSize || freeSize < bytesNeeded / 8)
    {
        // Always allocate enough so that the new heap will be at most half full
        size_t minimumSize = (usedSize * 8 + bytesNeeded) * 2;
        expandHeap(minimumSize);
    }
    else
    {
        equalizeHeaps();
    }

    void* result = try_mymalloc(sizeInBytes);
    if (!result)
    {
        // In the unhappy case where the current heap doesn't have enough space
        // for the current allocation, we have to copy again into the
        // newly-enlarged other heap
        gcCollect(stackTop, stackBottom, additionalRoots);
        result = try_mymalloc(sizeInBytes);

        // The heap-expansion above should have guaranteed that we have enough
        // space to make this allocation
        assert(result);

        equalizeHeaps();
    }

    return result;
}
