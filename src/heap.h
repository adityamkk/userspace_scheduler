#pragma once

#include "common.h"

namespace heap {
    extern void init(paddr_t start, size_t size);
    extern "C" void* malloc(size_t size);
    extern "C" void free(void* p);
};