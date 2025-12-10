#pragma once

class Spinlock {
public:
    Spinlock();
    void lock();
    void unlock();

//private:
    volatile int locked;
};