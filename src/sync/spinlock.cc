#include "spinlock.h"
#include "../boot/pit.h"

Spinlock::Spinlock() : locked(0) {}

void Spinlock::lock() {
    bool was = pit::disable_interrupts();
    while (!locked.compare_and_swap(0, 1)) {
        pit::restore_interrupts(was);
        was = pit::disable_interrupts();
        // TODO: Do something
    }
}

void Spinlock::unlock() {
    locked.set(0);
}

SpinlockNoInterrupts::SpinlockNoInterrupts() : locked(0) {}

void SpinlockNoInterrupts::lock() {
    while (!locked.compare_and_swap(0, 1)) {
        // TODO: Do something
    }
}

void SpinlockNoInterrupts::unlock() {
    locked.set(0);
}
