#include "kernel_main.h"
#include "threads/threads.h"
#include "sync/semaphore.h"
#include "sync/barrier.h"
#include "sync/promise.h"

void busy_work(uint64_t iterations)
{
    volatile uint64_t x = 0;
    for (uint64_t i = 0; i < iterations; i++) {
        x += i;
    }
}

void kernel_main() {
    printf("Hello world!\n");
    Barrier *b = new Barrier(5);
    Promise<int> *p = new Promise<int>();
    threads::kthread([b, p] {
        printf("Thread 1 Start\n");
        busy_work(500000000ULL);
        b->sync();
        ASSERT(p->get() == 5);
        printf("Thread 1 End\n");
    });
    threads::kthread([b, p] {
        printf("Thread 2 Start\n");
        busy_work(500000000ULL);
        b->sync();
        ASSERT(p->get() == 5);
        printf("Thread 2 End\n");
    });
    threads::kthread([b, p] {
        printf("Thread 3 Start\n");
        busy_work(500000000ULL);
        b->sync();
        ASSERT(p->get() == 5);
        printf("Thread 3 End\n");
    });
    threads::kthread([b, p] {
        printf("Thread 4 Start\n");
        busy_work(50000000ULL);
        printf("Thread 4 Mid\n");
        b->sync();
        busy_work(50000000ULL);
        printf("Must print first\n");
        p->set(5);
        printf("Thread 4 End\n");
    });
    busy_work(500000000ULL);
    b->sync();
    ASSERT(p->get() == 5);
    printf("Goodbye!\n");
}