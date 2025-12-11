#pragma once

#include "../common/common.h"

namespace pit {
    extern void init(); // Per-core init

    extern void set_timer(uint64_t time);
    extern uint64_t get_time();

    extern void pit_handler();

    extern bool disable_interrupts();
    extern void restore_interrupts(bool was);
};