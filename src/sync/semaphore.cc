#include "semaphore.h"
#include "../threads/threads.h"
#include "../threads/scheduler.h"

Semaphore::Semaphore(int n) : n(n), lock() {}

// Decrement the integer, if it goes below 0 then block
void Semaphore::down() {
    // TODO: Disable interrupts
    threads::TCB* my_thread = threads::hartstates.mine().current_thread;
    my_thread->setPreemption(false);
    // TODO: Enable interrupts
    lock.lock();
    n = n - 1;
    if (n < 0) {
        // Block
        threads::hartstates.mine().prev_sem = this; // Set the semaphore
        threads::block(my_thread, threads::hartstates.mine().idle_thread, [] {
            ASSERT(threads::hartstates.mine().prev_thread != nullptr);
            ASSERT(threads::hartstates.mine().prev_sem != nullptr);
            // Add the thread to the semaphore's blocking queue
            threads::hartstates.mine().prev_sem->blocked_threads.push(threads::hartstates.mine().prev_thread);
            // Unlock the semaphore spinlock
            threads::hartstates.mine().prev_sem->lock.unlock();
            threads::hartstates.mine().prev_sem = nullptr;
        });
    } else {
        lock.unlock();
    }
    my_thread->setPreemption(true);
}

void Semaphore::up() {
    // TODO: Disable interrupts
    threads::TCB* my_thread = threads::hartstates.mine().current_thread;
    my_thread->setPreemption(false);
    // TODO: Enable interrupts
    lock.lock();
    n = n + 1;
    if (n <= 0) {
        // Pull TCB off the blocking queue and add to the scheduler
        threads::TCB* blocked_thread = this->blocked_threads.pop();
        scheduler::schedule(blocked_thread);
    }
    lock.unlock();
    my_thread->setPreemption(true);
}