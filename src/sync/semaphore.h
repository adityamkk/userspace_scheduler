#pragma once

#include "spinlock.h"
#include "../threads/threads.h"
#include "sync_queue.h"

class Semaphore {
    volatile int n;
public:
    SpinlockNoInterrupts lock;
    SyncQueue<threads::TCB> blocked_threads;
    Semaphore(int n);
    ~Semaphore();
    void down();
    void up();
};