#pragma once

#include "semaphore.h"

template <typename T>
class Promise {
    T value;
    Semaphore sem;
public:
    Promise() : sem(0) {}

    /**
     * Gets the value from a promise
     * Blocks until the value is ready
     */
    T get() {
        sem.down();
        sem.up();
        return value;
    }

    /**
     * Sets the value of a promise
     * Undefined if called more than once
     */
    void set(T val) {
        value = val;
        sem.up(); // Signals consumers that the value is ready
    }
};