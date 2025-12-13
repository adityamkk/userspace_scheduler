#pragma once

#include "atomic.h"

class Spinlock {
public:
    Spinlock();
    void lock();
    void unlock();

//private:
    Atomic<int> locked;
    bool prev_interrupt_state;
};

// Does not disable interrupts while holding the lock
class SpinlockNoInterrupts {
public:
    SpinlockNoInterrupts();
    void lock();
    void unlock();

//private:
    Atomic<int> locked;
};