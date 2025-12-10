#include "kernel_main.h"
#include "threads/threads.h"
#include "sync/semaphore.h"
#include "sync/barrier.h"

void kernel_main() {
    printf("Hello, World!\n");
    Barrier* b = new Barrier(3);
    threads::kthread([b] {
        printf("Lion Messi 1\n");
        b->sync();
        printf("Lion Messi 2\n");
        b->sync();
        printf("Lion Messi 3\n");
    });
    threads::kthread([b] {
        printf("Rhino Ronaldo 1\n");
        b->sync();
        printf("Rhino Ronaldo 2\n");
        b->sync();
        printf("Rhino Ronaldo 3\n");
    });
    printf("Hello\n");
    b->sync();
    printf("Hello, Again!\n");
    b->sync();
    printf("Hello, Again, Again\n");
}