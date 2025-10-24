#pragma once
#include "common.h"
#include "spinlock.h"

namespace pallocator {
    static const uint32_t PAGE_SIZE = 4096;
    paddr_t alloc_page();
    void dealloc_pages(paddr_t p);
    void init(paddr_t start, size_t size);
};