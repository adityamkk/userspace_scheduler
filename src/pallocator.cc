#include "pallocator.h"

namespace pallocator {

// ==========================================================
// Internal state
// ==========================================================

static paddr_t region_start = 0;     // physical start address of the managed area
static uint32_t total_pages = 0;     // total number of pages in the region
static uint32_t bitmap_size = 0;     // size of bitmap in bytes
static uint8_t* bitmap = 0;          // pointer to bitmap in memory

// ==========================================================
// Helper functions (no stdlib)
// ==========================================================

static inline void set_bit(uint32_t bit_index) {
    bitmap[bit_index / 8] |= (1 << (bit_index % 8));
}

static inline void clear_bit(uint32_t bit_index) {
    bitmap[bit_index / 8] &= ~(1 << (bit_index % 8));
}

static inline uint32_t test_bit(uint32_t bit_index) {
    return (bitmap[bit_index / 8] >> (bit_index % 8)) & 1;
}

// ==========================================================
// Initialization
// ==========================================================

void init(paddr_t start, size_t size) {
    region_start = start;

    // how many pages we can manage
    total_pages = size / PAGE_SIZE;

    // bitmap size in bytes (round up)
    bitmap_size = (total_pages + 7) / 8;

    // place bitmap at start of the region
    bitmap = (uint8_t*) start;

    // clear bitmap (mark all pages free)
    for (uint32_t i = 0; i < bitmap_size; i++)
        bitmap[i] = 0;

    // skip over the bitmap itself so we don't allocate it
    uint32_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < bitmap_pages; i++)
        set_bit(i);
}

// ==========================================================
// Allocate one 4 KiB page
// ==========================================================

paddr_t alloc_page() {
    for (uint32_t i = 0; i < total_pages; i++) {
        if (!test_bit(i)) {
            set_bit(i);
            return region_start + (i * PAGE_SIZE);
        }
    }

    // no free pages
    return 0;
}

// ==========================================================
// Deallocate one 4 KiB page
// ==========================================================

void dealloc_pages(paddr_t p) {
    if (p < region_start)
        return;

    uint32_t index = (p - region_start) / PAGE_SIZE;
    if (index >= total_pages)
        return;

    clear_bit(index);
}

} // namespace pallocator
