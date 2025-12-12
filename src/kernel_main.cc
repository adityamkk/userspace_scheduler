#include "kernel_main.h"
#include "threads/threads.h"
#include "sync/semaphore.h"
#include "sync/barrier.h"
#include "sync/promise.h"
#include "sync/shared.h"

void busy_work(uint64_t iterations)
{
    volatile uint64_t x = 0;
    for (uint64_t i = 0; i < iterations; i++) {
        x += i;
    }
}

struct A {
    int val;
    A(int val) : val(val) {}

    ~A() {
        printf("Destructor: %d\n", val);
    }
};

void kernel_main() {
    SharedPtr<A> shared = SharedPtr<A>(new A(5));
    Barrier *b = new Barrier(6);
    for (int i = 0; i < 5; i++) {
        threads::kthread([shared, b] {
            printf("Thread %d sees *shared = %x\n", threads::getktid(), shared->val);
            b->sync();
            b->sync();
            printf("Thread %d sees *shared = %x\n", threads::getktid(), shared->val);
        });
    }
    printf("Main sees *shared = %x\n", shared->val);
    b->sync();
    shared->val = 6;
    b->sync();
    printf("Main sees *shared = %x\n", shared->val);
    printf("Kernel main finished!\n");
}