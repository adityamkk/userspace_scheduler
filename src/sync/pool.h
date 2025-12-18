#pragma once

#include "../common/common.h"
#include "sync_queue.h"
#include "semaphore.h"

// Resource pool, basically a wrapper for a non-blocking queue
template <typename T>
class Pool {
    Semaphore sem;
    SyncQueue<T> resources;
public:
    Pool() : sem(0), resources() {}

    T* allocate() {
        sem.down();
        T* val = resources.pop();
        return val;
    }

    void free(T* val) {
        if (val != nullptr) {
            resources.push(val);
            printf("Pushing val\n");
            sem.up();
        }
    }
};