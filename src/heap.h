#pragma once

#include "common/common.h"

// Copilot made the heap implementation
namespace heap {
    extern void init(paddr_t start, size_t size);
    extern void dump_free_list();
    extern "C" void* malloc(size_t size);
    extern "C" void free(void* p);
};