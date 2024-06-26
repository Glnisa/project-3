#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define ALIGNMENT 8 // Alignment for memory blocks
#define SIZEOFBLOCK sizeof(MemoryBlock) // Size of the memory block structure
#define SIZEOFPOOL (6 * 1024) // Size of the initial memory pool


// Structure defining a memory block
typedef struct MemoryBlock
{
    // Size of the memory block
    size_t size; 
    // Pointer to the next memory block
    struct MemoryBlock *next;
    // Pointer to the previous memory block
    struct MemoryBlock *prev;

    // Flag indicating whether  block is free (1) or in use (0)
    unsigned char isFree;
} MemoryBlock;

// Head of the memory block list
MemoryBlock *head = NULL;

// Align size to meet alignment requirements
size_t alignSize(size_t size)
{
    return (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1));
}


// Split a memory block into two, marking the second part as free
void split_Block(MemoryBlock *block, size_t size)
{
    size_t remSize = block->size - size - SIZEOFBLOCK;
    if (remSize > SIZEOFBLOCK + ALIGNMENT)
    {
        MemoryBlock *newBlock = (MemoryBlock *)((char *)block + SIZEOFBLOCK + size);
        newBlock->size = remSize;
        newBlock->isFree = 1;
        newBlock->next = block->next;
        newBlock->prev = block;
        if (newBlock->next)
        {
            newBlock->next->prev = newBlock;
        }
        block->size = size;
        block->next = newBlock;
    }
}

MemoryBlock *searchAndAllocateBestFitBlock(MemoryBlock **last, size_t size)
{
    MemoryBlock *current = head;
    MemoryBlock *fit = NULL;

    while (current)
    {
        if (current->isFree && current->size >= size)
        {
            if (!fit || current->size < fit->size)
            {
                fit = current;
            }
        }

        *last = current;
        current = current->next;
    }

    if (fit)
    {
        if (fit->size >= size + SIZEOFBLOCK + ALIGNMENT)
        {
            split_Block(fit, size);
        }
        fit->isFree = 0;
    }

    return fit;
}

// Extend heap by allocating more memory from system
MemoryBlock *extendHeap(MemoryBlock *last, size_t size)
{
    size_t totalSize = SIZEOFPOOL;
    if (size + SIZEOFBLOCK > totalSize)
    {
        totalSize = alignSize(size + SIZEOFBLOCK);
    }

    MemoryBlock *block = sbrk(totalSize);
    if (block == (void *)-1)
    {
        return NULL;
    }
    block->size = size;
    block->isFree = 0;
    block->next = NULL;
    block->prev = last;

    if (last)
    {
        last->next = block;
    }

    size_t remSize = totalSize - size - SIZEOFBLOCK;
    if (remSize > SIZEOFBLOCK)
    {
        MemoryBlock *freeBlock = (MemoryBlock *)((char *)block + SIZEOFBLOCK + size);
        freeBlock->size = remSize - SIZEOFBLOCK;
        freeBlock->isFree = 1;
        freeBlock->next = NULL;
        freeBlock->prev = block;
        block->next = freeBlock;
    }

    return block;
}
MemoryBlock *merge_free_blocks(MemoryBlock *block)
{
    if (block->next && block->next->isFree)
    {
        block->size += SIZEOFBLOCK + block->next->size;
        block->next = block->next->next;
        if (block->next)
        {
            block->next->prev = block;
        }
    }
    if (block->prev && block->prev->isFree)
    {
        block->prev->size += SIZEOFBLOCK + block->size;
        block->prev->next = block->next;
        if (block->next)
        {
            block->next->prev = block->prev;
        }
        block = block->prev;
    }
    return block;
}

// Securely zero out  memory content
void secureZeroMemory(void *inputblock, size_t inputSize)
{
    memset(inputblock, 0, inputSize);
}

// Get address of memory block from user pointer
MemoryBlock *getBlockAddress(void *ptr)
{
    return (MemoryBlock *)((char *)ptr - SIZEOFBLOCK);
}

void *kumalloc(size_t size)
{
    if (size <= 0)
    {
        return NULL;
    }

    size_t correctedSize = alignSize(size);
    MemoryBlock *block;

    if (!head)
    {

        block = extendHeap(NULL, correctedSize);
        if (!block)
        {
            return NULL;
        }
        head = block;
    }
    else
    {

        MemoryBlock *last = head;
        block = searchAndAllocateBestFitBlock(&last, correctedSize);

        if (!block)
        {

            block = extendHeap(last, correctedSize);
            if (!block)
            {
                return NULL;
            }
        }
        else
        {

            block->isFree = 0;

            if (block->size > correctedSize + SIZEOFBLOCK)
            {
                split_Block(block, correctedSize);
            }
        }
    }
    return (void *)(block + 1);
}

void *kucalloc(size_t numElements, size_t elementSize)
{
    if (numElements == 0 || elementSize == 0)
    {
        return NULL;
    }

    size_t totalSize = numElements * elementSize;
    size_t correctedSize = alignSize(totalSize);
    void *newBlock = kumalloc(correctedSize);

    if (newBlock)
    {
        secureZeroMemory(newBlock, correctedSize);
    }

    return newBlock;
}
void kufree(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    MemoryBlock *block = getBlockAddress(ptr);
    block->isFree = 1;
    block = merge_free_blocks(block);

    if (block->next == NULL && block->isFree)
    {

        if (block->prev)
        {
            block->prev->next = NULL;
        }
        else
        {
            head = NULL;
        }

        if (brk(block) == -1)
        {

            fprintf(stderr, "Error!!!\n");
        }
    }
}

void *kurealloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        kufree(ptr);
        return NULL;
    }

    if (!ptr)
    {
        return kumalloc(size);
    }

    MemoryBlock *block = (MemoryBlock *)ptr - 1;
    if (block->size >= size)
    {
        return ptr;
    }

    void *newPtr = kumalloc(size);
    if (newPtr)
    {
        memcpy(newPtr, ptr, block->size);
        kufree(ptr);
    }

    return newPtr;
}

/*
 * Enable the code below to enable system allocator support for your allocator.
 * Doing so will make debugging much harder (e.g., using printf may result in
 * infinite loops).
 */
// #if 1
void *malloc(size_t size) { return kumalloc(size); }
void *calloc(size_t nmemb, size_t size) { return kucalloc(nmemb, size); }
void *realloc(void *ptr, size_t size) { return kurealloc(ptr, size); }
void free(void *ptr) { kufree(ptr); }
// #endif