#pragma once

class Spinlock {
public:
    Spinlock();
    void lock();
    void unlock();

//private:
    volatile int locked;
};

// Does not disable interrupts while holding the lock
class SpinlockNoInterrupts {
public:
    SpinlockNoInterrupts();
    void lock();
    void unlock();

//private:
    volatile int locked;
};