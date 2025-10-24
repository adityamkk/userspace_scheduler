#include "heap.h"

// ChatGPT wrote this

namespace heap {

// ===========================================================
// Basic block header
// ===========================================================

struct BlockHeader {
    size_t size;          // size of usable area (in bytes)
    size_t free;          // 1 = free, 0 = used
    BlockHeader* next;          // next block in list
};

// ===========================================================
// Global allocator state
// ===========================================================

static BlockHeader* heap_start = 0;
static size_t heap_total_size = 0;
static BlockHeader* free_list = 0;

// ===========================================================
// Initialize the heap
// ===========================================================

void init(paddr_t start, size_t size) {
    heap_start = (BlockHeader*) start;
    heap_total_size = size;

    // create a single big free block
    free_list = heap_start;
    free_list->size = size - sizeof(BlockHeader);
    free_list->free = 1;
    free_list->next = 0;
}

// ===========================================================
// Allocate memory
// ===========================================================

void* malloc(size_t size) {
    if (size == 0)
        return 0;

    // align size to 4 bytes
    if (size & 3)
        size = (size + 3) & ~3;

    BlockHeader* prev = 0;
    BlockHeader* current = free_list;

    while (current) {
        if (current->free && current->size >= size) {
            // found a suitable block
            size_t remaining = current->size - size;

            // If we can split the block into two
            if (remaining > sizeof(BlockHeader) + 4) {
                // The new block begins right after the allocated area
                char* next_addr = (char*)(current + 1) + size;
                BlockHeader* next = (BlockHeader*) next_addr;

                next->size = remaining - sizeof(BlockHeader);
                next->free = 1;
                next->next = current->next;

                current->size = size;
                current->next = next;
            }

            current->free = 0;

            // update free list head if necessary
            if (prev == 0 && current->free == 0) {
                free_list = current->next;
            }

            return (void*)(current + 1);
        }

        prev = current;
        current = current->next;
    }

    // no free block found
    return 0;
}

// ===========================================================
// Free memory
// ===========================================================

void free(void* ptr) {
    if (!ptr)
        return;

    BlockHeader* block = ((BlockHeader*)ptr) - 1;
    block->free = 1;

    // insert at front of free list
    block->next = free_list;
    free_list = block;

    // coalesce adjacent free blocks
    BlockHeader* current = free_list;
    while (current) {
        BlockHeader* next = current->next;
        if (next && current->free && next->free) {
            char* curr_end = (char*)(current + 1) + current->size;
            if (curr_end == (char*)next) {
                // merge adjacent blocks
                current->size += sizeof(BlockHeader) + next->size;
                current->next = next->next;
                continue;
            }
        }
        current = current->next;
    }
}

} // namespace heap
