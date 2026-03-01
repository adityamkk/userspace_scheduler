#pragma once

#include "spinlock.h"
#include "../threads/threads.h"
#include "sync_queue.h"

class Semaphore {
    volatile int n;
public:
    Spinlock lock; // NoInterrupts (SpinlockNoInterrupts may cause issues with the virtio-blk device isr)
    SyncQueue<threads::TCB> blocked_threads;
    Semaphore(int n);
    void down();
    void up();
};