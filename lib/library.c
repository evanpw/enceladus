// For non-standard getline
#define _GNU_SOURCE

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "library.h"

#ifdef __APPLE__

extern void Z4main(void);

int main(void)
{
    Z4main();
    return 0;
}

#else

extern void _Z4main(void);

int main(void)
{
    _Z4main();
    return 0;
}

#endif

void fail(const char* str)
{
    puts(str);
    exit(1);
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

#ifdef __APPLE__
extern void* gcAllocate(size_t) asm("gcAllocate");
#else
extern void* gcAllocate(size_t);
#endif

char* strContent(String* s)
{
    return (char*)(s + 1);
}

String* makeStr(const char* data)
{
    String* result = gcAllocate(sizeof(SplObject) + strlen(data) + 1);
    result->constructorTag = STRING_TAG;
    result->pointerFields = 0;
    result->markBit = 0;
    strcpy(strContent(result), data);

    return result;
}

int64_t strLength(String* s)
{
    return toInt(strlen(strContent(s)));
}

String* strSlice(String* s, int64_t tPos, int64_t tLength)
{
    int64_t pos = fromInt(tPos);
    int64_t length = fromInt(tLength);

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
    result->pointerFields = 0;
    result->markBit = 0;

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
    result->pointerFields = 0;
    result->markBit = 0;

    char* dest = strContent(result);
    strncpy(dest, strContent(lhs), n1);
    strncpy(dest + n1, strContent(rhs), n2);
    *(dest + n1 + n2) = '\0';

    return result;
}

int64_t strAt(String* s, int64_t n)
{
    int64_t idx = fromInt(n);

    if (idx < 0 || idx >= strLength(s))
    {
        fail("*** Exception: String index out of range");
    }

    return toInt(strContent(s)[idx]);
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
    result->pointerFields = 0;
    result->markBit = 0;

    char* out = strContent(result);
    while (!IS_EMPTY(list))
    {
        int64_t c = fromInt((int64_t)list->value);
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
    int64_t value = fromInt(x);

    String* result = gcAllocate(sizeof(SplObject) + 20 + 1);
    result->constructorTag = STRING_TAG;
    result->pointerFields = 0;
    result->markBit = 0;

    sprintf(strContent(result), "%" PRId64, value);
    return result;
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
        return makeStr("");
    }
    else
    {
        String* result = makeStr(line);
        free(line);

        return result;
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

void markFromRoot(SplObject* p)
{
    if (p->markBit) return;

    p->gcNext = NULL;
    SplObject* markStack = p;

    while (markStack)
    {
        // Pop the top of the stack, mark it, and add its children to the stack
        SplObject* object = markStack;
        markStack = object->gcNext;

        object->markBit = 1;

        SplObject** p = (SplObject**)(object + 1);
        uint64_t fields = object->pointerFields;
        while (fields)
        {
            if ((fields & 1) && !IS_TAGGED(*p) && !(*p)->markBit)
            {
                (*p)->gcNext = markStack;
                markStack = *p;
            }

            fields >>= 1;
            ++p;
        }
    }
}

void gcMark(uint64_t* stackTop, uint64_t* stackBottom, uint64_t* framePointer, uint64_t* globalVarTable)
{
    uint64_t* top = stackTop;
    uint64_t* bottom = framePointer;

    while (1)
    {
        //printf("Stack frame: %p %p\n", top, bottom);
        for (uint64_t* p = top; p < bottom; ++p)
        {
            //printf("%p: %p\n", p, (void*)*p);

            SplObject* object = (SplObject*)*p;
            if (object && !IS_TAGGED(object))
            {
                markFromRoot(object);
            }
        }
        //printf("\n");

        if (bottom == stackBottom)
        {
            break;
        }

        top = bottom + 2;
        bottom = (uint64_t*)*bottom;
    }

    //printf("Globals:\n");
    uint64_t numGlobals = *globalVarTable;
    uint64_t** p = (uint64_t**)(globalVarTable + 1);
    for (size_t i = 0; i < numGlobals; ++i)
    {
        //printf("%p: %p\n", p, (void*)**p);

        SplObject* object = (SplObject*)**p;
        if (object && !IS_TAGGED(object))
        {
            markFromRoot(object);
        }

        ++p;
    }
}

uint8_t* firstChunk = NULL;
uint8_t* currentChunk = NULL;
size_t chunkSize = 0;
void* freeList = NULL;

const size_t MIN_BLOCK_SIZE = 0x20;
const size_t MIN_SPLIT_SIZE = 0x50;

static void createFreeBlock(void* p, size_t size, void* prev, void* next)
{
    assert(size >= MIN_BLOCK_SIZE);

    uint64_t* header = p;
    *header++ = size | 1;
    *header++ = (uint64_t)next;

    // Footer
    uint64_t* footer = (uint64_t*)((uint8_t*)p + size) - 2;
    *footer++ = (uint64_t)prev;
    *footer++ = size | 1;
}

uint64_t freeGetSize(void* p)
{
    uint64_t* header = p;
    return *header & ~1ULL;
}

void* freeGetNext(void* p)
{
    uint64_t* header = p;
    return (void*)*(header + 1);
}

void freeSetNext(void* p, void* next)
{
    void** header = p;
    *(header + 1) = next;
}

void* freeGetPrev(void* p)
{
    size_t size = freeGetSize(p);
    uint64_t* footer = (uint64_t*)((uint8_t*)p + size) - 2;
    return (void*)*footer;
}

void freeSetPrev(void* p, void* prev)
{
    size_t size = freeGetSize(p);
    uint64_t* footer = (uint64_t*)((uint8_t*)p + size) - 2;
    *footer = (uint64_t)prev;
}

int createChunk(size_t minSize)
{
    size_t newSize = chunkSize == 0 ? 4 << 20 : chunkSize * 2;
    while (newSize < minSize) newSize *= 2;

    void* newChunk = mmap(0, newSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (newChunk == MAP_FAILED)
    {
        return 0;
    }

    //printf("mymalloc: new chunk @ %p, size = %zx\n", newChunk, newSize);

    if (currentChunk)
    {
        *(void**)currentChunk = newChunk;
    }
    else
    {
        firstChunk = newChunk;
    }

    currentChunk = newChunk;
    chunkSize = newSize;

    // Pointer to next chunk
    *(void**)currentChunk = NULL;                   // Pointer to next chunk
    *((uint64_t*)currentChunk + 1) = chunkSize;     // Size of this chunk

    // Fake sentinel footer and header
    uint64_t* fakeHeader = (uint64_t*)currentChunk + 2;
    uint64_t* fakeFooter = (uint64_t*)(currentChunk + newSize) - 1;
    *fakeHeader = 0;
    *fakeFooter = 0;
    //printf("fakeHeader = %p, fakeFooter = %p\n", fakeHeader, fakeFooter);

    void* newBlock = currentChunk + 3 * 8;
    createFreeBlock(newBlock, newSize - 4 * 8, NULL, freeList);
    if (freeList)
    {
        freeSetPrev(freeList, newBlock);
    }
    freeList = newBlock;

    return 1;
}

void makeAllocatedBlock(void* p, size_t size)
{
    uint64_t* header = p;
    *header = size;

    uint64_t* footer = (uint64_t*)((uint8_t*)p + size) - 1;
    *footer = size;
}

size_t roundUp8(size_t size)
{
    if (size & 7)
    {
        return (size & ~7UL) + 8;
    }
    else
    {
        return size;
    }
}

void checkAllocatedBlock(void* p, size_t useableSize)
{
    uint8_t* bytes = p;
    uint64_t* header = (uint64_t*)bytes;

    size_t actualSize = *header;
    assert(actualSize >= useableSize + 16);

    uint64_t* footer = (uint64_t*)(bytes + actualSize) - 1;
    assert(*footer == actualSize);

    printf("Good block @ %p, size = %zx\n", p, actualSize);
}

void gcSweep()
{
    //printf("\nHeap:\n");

    size_t freeSize = 0;
    size_t totalSize = 0;

    uint8_t* chunk = firstChunk;
    while (chunk)
    {
        void* nextChunk = *((void**)chunk);
        uint64_t chunkSize = *((uint64_t*)chunk + 1);
        uint8_t* chunkEnd = chunk + chunkSize - 8;

        totalSize += chunkSize;

        //printf("Chunk @ %p, next = %p, size = %llx\n", p, nextChunk, chunkSize);

        uint8_t* block = chunk + 3 * 8;
        while (block < chunkEnd)
        {
            uint64_t sizeAndFlag = *(uint64_t*)block;
            uint64_t size = sizeAndFlag & ~1ULL;
            uint64_t freeFlag = sizeAndFlag & 1;

            if (freeFlag)
            {
                //printf("Free block @ %p, size = %llx\n", block, size);
                freeSize += size;
            }
            else
            {
                //printf("Allocated block @ %p, size = %llx\n", block, size);

                SplObject* object = (SplObject*)((uint64_t*)block + 1);
                //printf("\tconstructorTag = %zx\n", object->constructorTag);
                //printf("\tmarkBit = %lld\n", object->markBit);
                //printf("\tpointerFields = %llx\n", object->pointerFields);

                if (!object->markBit)
                {
                    //printf("\tFreeing object\n");
                    myfree(object);
                }
                else
                {
                    object->markBit = 0;
                }
            }

            block += sizeAndFlag & ~1ULL;
        }

        //printf("End Chunk\n");

        chunk = *(uint8_t**)chunk;
    }

    //printf("freeSize = %zx, totalSize = %zx, pct free = %f\n", freeSize, totalSize, freeSize / (double)totalSize);

    // If heap is mostly full even after collecting, double the size of the heap
    if (freeSize <= 0.10 * totalSize)
    {
        createChunk(totalSize);
    }
}

#ifdef __APPLE__
extern void gcCollect(uint64_t*, uint64_t*, uint64_t*, uint64_t*) asm("gcCollect");
#endif

void gcCollect(uint64_t* stackTop, uint64_t* stackBottom, uint64_t* framePointer, uint64_t* globalVarTable)
{
    gcMark(stackTop, stackBottom, framePointer, globalVarTable);
    gcSweep();
}

void walkFreeList()
{
    printf("freeList = %p\n", freeList);

    void* p = freeList;
    while (p)
    {
        uint64_t sizeAndFlag = *(uint64_t*)p;
        printf("Free block @ %p, size = %llx, free = %" PRIu64 "\n", p, sizeAndFlag & ~1ULL, sizeAndFlag & 1);

        void* next = freeGetNext(p);
        void* prev = freeGetPrev(p);

        if (prev) assert(freeGetNext(prev) == p);
        if (next) assert(freeGetPrev(next) == p);

        p = next;
    }
}

#ifdef __APPLE__
void* try_mymalloc(size_t) asm("try_mymalloc");
#endif

// Try to allocate memory from the free list, but don't request more memory from
// the operating system (except on the first call)
void* try_mymalloc(size_t size)
{
    //printf("try_mymalloc: %zx\n", size);

    size = roundUp8(size);

    if (size < MIN_BLOCK_SIZE)
        size = MIN_BLOCK_SIZE;

    // First allocation: need to create a heap
    if (!firstChunk)
    {
        if (!createChunk(0)) return NULL;
    }

    // Search for a large-enough free block
    void* p = freeList;
    while (p)
    {
        //printf("Examining block @ %p, size = %llx\n", p, freeGetSize(p));

        // This block is large enough.
        if (freeGetSize(p) >= size + 16)
        {
            uint64_t* result = p;

            // If this is large enough to accomodate the current request and
            // possibly another one, then split it.
            if (freeGetSize(p) - size - 16 >= MIN_SPLIT_SIZE)
            {
                size_t fullSize = freeGetSize(p);
                void* prev = freeGetPrev(p);
                void* next = freeGetNext(p);

                void* newBlock = (uint8_t*)p + fullSize - size - 16;
                makeAllocatedBlock(newBlock, size + 16);
                createFreeBlock(p, fullSize - size - 16, prev, next);

                //printf("Splitting block @ %p\n", newBlock);

                result = newBlock;
            }
            else // Otherwise, use the whole thing
            {
                void* prev = freeGetPrev(p);
                void* next = freeGetNext(p);

                //printf("Using full block\n");

                makeAllocatedBlock(p, freeGetSize(p));

                if (prev)
                {
                    freeSetNext(prev, next);
                }
                else
                {
                    freeList = next;
                }

                if (next) freeSetPrev(next, prev);
            }

            return (void*)(result + 1);
        }

        p = freeGetNext(p);
    }

    // Cannot allocate from the current free list
    return NULL;
}

#ifdef __APPLE__
void* mymalloc(size_t) asm("mymalloc");
#endif

void* mymalloc(size_t size)
{
    void* result = try_mymalloc(size);
    if (result) return result;

    // Current heap is full: need to allocate another chunk
    createChunk(size + 16);
    return try_mymalloc(size);
}

void myfree(void* p)
{
    void* block = (void*)((uint8_t*)p - 8);
    size_t size = *(uint64_t*)block;

    //printf("myfree @ %p, size = 0x%zx\n", block, size);
    //printf("Start:\n");
    //walkFreeList();

    assert(size >= MIN_BLOCK_SIZE);

    uint64_t* prevFooter = (uint64_t*)block - 1;
    uint64_t* nextHeader = (uint64_t*)((uint8_t*)block + size);

    //printf("prevFooter = %p, nextHeader = %p\n", prevFooter, nextHeader);

    // If previous contiguous block is free, then coalesce
    uint64_t prevSizeAndFlag = *prevFooter;
    if (prevSizeAndFlag & 1)
    {
        size_t prevSize = prevSizeAndFlag & ~1ULL;
        uint8_t* prevBlock = (uint8_t*)block - prevSize;
        void* prev = freeGetPrev(prevBlock);
        void* next = freeGetNext(prevBlock);

        //printf("Coalesce with previous: %p %p %p\n", prevBlock, prev, next);

        createFreeBlock(prevBlock, size + prevSize, prev, next);

        //printf("End:\n");
        //walkFreeList();
        return;
    }

    // If next contiguous block is free, then coalesce
    uint64_t nextSizeAndFlag = *nextHeader;
    if (nextSizeAndFlag & 1)
    {
        size_t nextSize = nextSizeAndFlag & ~1ULL;
        uint8_t* nextBlock = (uint8_t*)nextHeader;
        void* prev = freeGetPrev(nextBlock);
        void* next = freeGetNext(nextBlock);

        //printf("Coalesce with next: %p %p %p\n", nextBlock, prev, next);

        createFreeBlock(block, size + nextSize, prev, next);

        if (prev)
        {
            freeSetNext(prev, block);
        }
        else
        {
            freeList = block;
        }

        if (next)
        {
            freeSetPrev(next, block);
        }

        //printf("End:\n");
        //walkFreeList();
        return;
    }

    // Otherwise, add to the beginning of the free list
    createFreeBlock(block, size, NULL, freeList);
    if (freeList) freeSetPrev(freeList, block);
    freeList = block;

    //printf("End:\n");
    //walkFreeList();
}
