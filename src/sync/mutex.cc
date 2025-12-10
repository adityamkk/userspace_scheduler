#include "mutex.h"

Mutex::Mutex() : sem(1) {}

void Mutex::lock() {
    sem.down();
}

void Mutex::unlock() {
    sem.up();
}