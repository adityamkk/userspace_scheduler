#include "spinlock.h"

Spinlock::Spinlock() : locked(0) {}

void Spinlock::lock() {
    int tmp;
    int lockval;
    do {
        // Step 1: Load-reserved
        asm volatile (
            "1: lr.w %0, (%2)\n"        // load-reserved lockval = *locked
            "   bnez %0, 1b\n"           // if nonzero, someone holds it → retry
            // Step 2: Try to store-conditional (set to 1)
            "   li %1, 1\n"
            "   sc.w %0, %1, (%2)\n"     // attempt store
            "   bnez %0, 1b\n"           // if failed (someone else wrote), retry
            : "=&r"(tmp), "=&r"(lockval)
            : "r"(&locked)
            : "memory");
    } while (0);
}

void Spinlock::unlock() {
    asm volatile (
        "amoswap.w zero, zero, (%0)\n"   // atomic store 0 (release)
        :
        : "r"(&locked)
        : "memory");
}
