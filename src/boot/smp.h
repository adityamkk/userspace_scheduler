#pragma once

#include "../common/common.h"

namespace smp {
    const int MAX_HARTS = 16;
    extern uint32_t me();

    template <typename T>
    class PerCPU {
        T data[MAX_HARTS];
    public:
        T& forCPU(uint32_t id) {
            ASSERT(id >= 0 && id < MAX_HARTS);
            return data[id];
        }
        T& mine() {
            int me = smp::me();
            return this->forCPU(me);
        }
    };
};