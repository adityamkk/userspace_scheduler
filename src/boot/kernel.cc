#include "kernel.h"
#include "../common/sbi.h"
#include "../common/common.h"
#include "smp.h"
#include "../threads/threads.h"
#include "../kernel_main.h"
#include "pit.h"
#include "../drivers/virtio-blk/virtio-blk.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];

const uint32_t HEAP_SIZE = 2 * 1024 * 1024;

volatile uint32_t hart_boot_count = 0;

/* boot_lock states:
 * 0 = not taken (no init in progress)
 * 1 = init in progress (this hart is initializing)
 * 2 = init complete
 */
volatile int boot_lock = 0;

struct sbiret sbi_hart_start(uint32_t hartid, uint64_t start_addr, uint64_t opaque) {
    return sbi_call((long)hartid, (long)start_addr, (long)opaque, 0, 0, 0, 0, 0x48534D);
}

struct sbiret sbi_ipi(uint32_t hartid) {
    uint32_t mask = 1 << hartid;
    return sbi_call((long)&mask, 0, 0, 0, 0, 0, 0, 0x04); // legacy: eid = 0x735049
}

/**
 * All cores end up here
 */
void kernel_init_2(void) {
    uint32_t hartid = smp::me();
    /* Secondary harts should only run after primary completes initialization. */

    /* PER-CORE INIT */
    WRITE_CSR(stvec, (uint32_t) kernel_entry); // Register the trap handler for all harts

    // Enable software interrupts
    uint32_t sie = READ_CSR(sie);
    sie |= (1 << 1);
    WRITE_CSR(sie, sie);
    uint32_t sstatus = READ_CSR(sstatus);
    sstatus |= (1 << 1);
    WRITE_CSR(sstatus, sstatus);

    // Enable timer interrupts
    pit::init();

    /* Wait until boot_lock == 2 (init complete) */
    while (boot_lock != 2) {
        // This is a tight while loop, but its okay since this is temporarily for initialization
        // printf("boot lock = %d\n", boot_lock);
        __asm__ __volatile__("" ::: "memory");
    }

    printf("| HART %d successfully booted!\n", hartid);

    /*
    for (uint32_t id = 0; id < smp::MAX_HARTS; id++) {
        if (id != hartid) {
            printf("Sending IPI to hart %d\n", id);
            sbi_ipi(id); // Wake up sleeping harts
        }
    }
    */

    if (hartid == 0) {
        threads::kthread([] {
            kernel_main();
        });
    }
    
    threads::stop();
    for (;;) {
        __asm__ __volatile__ ("wfi");
    }
}

/**
 * Starts up all secondary HARTs
 * Each secondary hart starts at secondary_boot and jumps to kernel_init_2
 */
void start_secondary_harts(void) {
    // Try to start harts 1 through MAX_HARTS-1 (skip hart 0, which is already running)
    for (uint32_t i = 0; i < smp::MAX_HARTS; i++) {
        extern void secondary_boot(void);
        struct sbiret ret = sbi_hart_start(i, (uint64_t)secondary_boot, 0);
        if (ret.error == 0) {
            printf("| Started HART %d\n", i);
            hart_boot_count += 1;
        } else {
            if ((uint32_t)ret.error == 0xfffffffa) {
                printf("| Cannot boot HART %d: HART %d is already running\n", i, i);
            } else if ((uint32_t)ret.error == 0xfffffffd) {
                printf("| Cannot boot HART %d: No HART %d exists on the machine\n", i, i);
            } else {
                printf("| An error occurred in starting HART %d: ret.error = %x\n", i, ret.error);
            }
        }
    }
}

/**
 * The trap entry point for all synchronous and asynchronous interrupt handlers
 */
__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrw sscratch, sp\n"
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        //"sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        //"lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

/**
 * Handles all traps after saving kernel state
 */
void handle_trap(struct trap_frame *f) {
    int enter_smp = smp::me();
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    ASSERT(pit::are_interrupts_disabled());

    if (scause & 0x80000000) {
        // Async interrupt
        uint32_t code = scause & 0xFF;
        if (code == 1) {
            // Supervisor Software Interrupt (IPI)
            // Used for waking up cores
            return;
        } else if (code == 5) {
            // Timer Interrupt
            uint64_t now = pit::get_time();
            pit::set_timer(now + 1024 * 1024);
            int exit_smp = smp::me();
            ASSERT(enter_smp == exit_smp);
            pit::pit_handler();
            return;
        }
    }

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

/**
 * Entry point of core 0 into the kernel proper
 * Core 0 initializes kernel structures and wakes up the other cores
 */
void kernel_init(void) {
    uint32_t hartid = smp::me();
    printf("| HART ID = %d\n", hartid);

    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss); // Set globals (bss section) to 0
    printf("| It's alive!\n");

    WRITE_CSR(stvec, (uint32_t) kernel_entry); // Register the trap handler
    printf("| Exceptions can now be handled!\n");

    heap::init((paddr_t)(__free_ram), (size_t)HEAP_SIZE); // Initialize the heap
    printf("| Initialized the heap\n");
    pallocator::init((paddr_t)(__free_ram + HEAP_SIZE), (size_t)(__free_ram_end - __free_ram - HEAP_SIZE)); // Initialize the physical memory allocator
    printf("| Initialized the physical memory allocator\n");

    /* Start secondary harts */
    printf("| Starting secondary harts...\n");
    start_secondary_harts();

    /* GLOBAL INIT */
    threads::init(); // Sets up idle threads, etc.

    virtio_blk_init();

    /* Mark initialization complete with memory barrier to ensure secondary harts see it */
    __asm__ volatile("" : : : "memory");
    boot_lock = 2;
    __asm__ volatile("" : : : "memory");
    printf("| Initializing HART %d begin main\n", hartid);
    kernel_init_2();

    for (;;) {
        __asm__ __volatile__("wfi");
    }
}