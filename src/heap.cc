#include "heap.h"
#include "sync/spinlock.h"

// ChatGPT wrote this

namespace heap {

Spinlock heapLock;

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
    heapLock = {};
    heap_start = (BlockHeader*) start;
    heap_total_size = size;

    // create a single big free block
    free_list = heap_start;
    free_list->size = size - sizeof(BlockHeader);
    free_list->free = 1;
    free_list->next = 0;
}

void dump_free_list() {
    ASSERT(free_list != nullptr);
    heapLock.lock();
    BlockHeader* current = free_list;
    printf("-----------------------------------------\n");
    printf("DUMPING FREE LIST starting at %x:\n", free_list);
    while (current) {
        if (current->free) {
            printf("FREE BLOCK: start = %x, size = %d\n", ((uint8_t*)current) + 8, current->size);
        } else {
            printf("USED BLOCK: start = %x, size = %d\n", ((uint8_t*)current) + 8, current->size);
        }
        current = current->next;
        if (current == free_list) {
            printf("Somehow the list looped back around\n");
            break;
        }
    }
    printf("-----------------------------------------\n");
    heapLock.unlock();
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

    heapLock.lock();
    //BlockHeader* prev = 0;
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

            heapLock.unlock();
            return (void*)(current + 1);
        }

        //prev = current;
        current = current->next;
        if (current == free_list) {
            PANIC("out of kernel heap space!\n");
        }
    }

    // no free block found
    heapLock.unlock();
    return 0;
}

// ===========================================================
// Free memory
// ===========================================================

void free(void* ptr) {
    if (!ptr)
        return;

    heapLock.lock();
    BlockHeader* block = ((BlockHeader*)ptr) - 1;
    block->free = 1;

    // coalesce adjacent free blocks
    BlockHeader* current = free_list;
    int iterations = 0;
    int max_iterations = 10000; // prevent infinite loops from circular references
    while (current && iterations < max_iterations) {
        iterations++;
        BlockHeader* next = current->next;
        if (next && current->free && next->free) {
            char* curr_end = (char*)(current + 1) + current->size;
            if (curr_end == (char*)next) {
                // merge adjacent blocks
                current->size += sizeof(BlockHeader) + next->size;
                current->next = next->next;
                // Don't advance current; check if the merged block can merge with the next one
                continue;
            }
        }
        // Only advance current if no merge happened
        current = current->next;
    }
    heapLock.unlock();
}

} // namespace heap

/*****************/
/* C++ operators */
/*****************/

void* operator new(size_t size) {
    void* p = heap::malloc(size);
    if (p == 0) PANIC("out of memory");
    return p;
}

void* operator new(size_t size, std::align_val_t align) {
    // For simplicity, allocate extra space and align manually
    size_t alignment = static_cast<size_t>(align);
    void* p = heap::malloc(size + alignment);
    if (p == 0) PANIC("out of memory");
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned_addr);
}

void operator delete(void* p) noexcept {
    return heap::free(p);
}

void operator delete(void* p, size_t sz) {
    return heap::free(p);
}

void operator delete(void* p, std::align_val_t align) noexcept {
    return heap::free(p);
}

void operator delete(void* p, size_t sz, std::align_val_t align) {
    return heap::free(p);
}

void* operator new[](size_t size) {
    void* p = heap::malloc(size);
    if (p == 0) PANIC("out of memory");
    return p;
}

void* operator new[](size_t size, std::align_val_t align) {
    size_t alignment = static_cast<size_t>(align);
    void* p = heap::malloc(size + alignment);
    if (p == 0) PANIC("out of memory");
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned_addr);
}

void operator delete[](void* p) noexcept {
    return heap::free(p);
}

void operator delete[](void* p, size_t sz) {
    return heap::free(p);
}

void operator delete[](void* p, std::align_val_t align) noexcept {
    return heap::free(p);
}

void operator delete[](void* p, size_t sz, std::align_val_t align) {
    return heap::free(p);
}
