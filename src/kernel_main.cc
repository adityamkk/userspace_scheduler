#include "kernel_main.h"
#include "threads/threads.h"

void kernel_main() {
    printf("Hello, World!\n");
    threads::kthread([] {
        printf("Lion Messi\n");
    });
    threads::kthread([] {
        printf("Rhino Ronaldo\n");
    });
    threads::yield();
    printf("Hello, Again!\n");
}