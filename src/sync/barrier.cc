#include "barrier.h"

Barrier::Barrier(int n) : count(0), count_sync(1), turnstile(0), turnstile_sync(1) {
    ASSERT(n > 0);
    this->n = n;
}

void Barrier::sync() {
    count_sync.down();
    count = count + 1;
    if (count == n) {
        turnstile_sync.down(); // Trap the threads until all threads make it through the turnstile
        turnstile.up();
        count_sync.up();
    } else {
        count_sync.up();
    }

    turnstile.down();
    turnstile.up();

    count_sync.down();
    count = count - 1;
    if (count == 0) {
        turnstile.down(); // Lock the first turnstile
        turnstile_sync.up();
        count_sync.up();
    } else {
        count_sync.up();
    }

    turnstile_sync.down();
    turnstile_sync.up();
}