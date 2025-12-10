#pragma once

#include "atomic.h"
#include "semaphore.h"

// Implementation inspired from https://greenteapress.com/semaphores/LittleBookOfSemaphores.pdf
class Barrier {
    int n;
    int count;
    Semaphore count_sync;
    Semaphore turnstile;
    Semaphore turnstile_sync;
public:
    Barrier(int n);
    void sync();
};