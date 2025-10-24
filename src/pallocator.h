#pragma once
#include "common.h"
#include "spinlock.h"

namespace pallocator {
    extern const uint32_t PAGE_SIZE;
    paddr_t alloc_page();
    void dealloc_pages(paddr_t p);
    void init(paddr_t start, size_t size);
};