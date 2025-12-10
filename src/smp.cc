#include "smp.h"

namespace smp {
    uint32_t me(void) {
        uint32_t hartid;
        __asm__ volatile("mv %0, tp" : "=r"(hartid));
        return hartid;
    }
}