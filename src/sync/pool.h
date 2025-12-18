#pragma once

#include "../common/common.h"
#include "sync_queue.h"
#include "mutex.h"

// Resource pool, basically a wrapper for a non-blocking queue
template <typename T>
class Pool {
    SyncQueue<T> resources;
public:
    Pool() : resources() {}

    T* allocate() {
        return resources.pop();
    }

    void free(T* val) {
        if (val != nullptr) {
            resources.push(val);
        }
    }
};