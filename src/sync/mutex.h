#pragma once

#include "semaphore.h"

class Mutex {
    Semaphore sem;
public:
    Mutex();
    void lock();
    void unlock();
};