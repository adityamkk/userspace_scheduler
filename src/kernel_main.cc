#include "kernel_main.h"
#include "threads/threads.h"
#include "sync/semaphore.h"
#include "sync/barrier.h"
#include "boot/smp.h"

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
    threads::kthread([b] {
        printf("Thread 1 Start\n");
        busy_work(500000000ULL);
        b->sync();
        printf("Thread 1 End\n");
    });
    threads::kthread([b] {
        printf("Thread 2 Start\n");
        busy_work(500000000ULL);
        b->sync();
        printf("Thread 2 End\n");
    });
    threads::kthread([b] {
        printf("Thread 3 Start\n");
        busy_work(500000000ULL);
        printf("Thread 3 End\n");
        b->sync();
    });
    threads::kthread([b] {
        printf("Thread 4 Start\n");
        busy_work(50000000ULL);
        printf("Thread 4 End\n");
        b->sync();
    });
    busy_work(500000000ULL);
    b->sync();
    printf("Goodbye!\n");
}