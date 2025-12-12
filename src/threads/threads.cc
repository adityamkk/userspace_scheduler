#include "threads.h"
#include "scheduler.h"
#include "../boot/pit.h"
#include "../boot/kernel.h"

namespace threads {
    // We need the attribute because we need the args to be in specific registers
    __attribute__((naked)) void context_switch(uint32_t *prev_sp, uint32_t *next_sp) {
        __asm__ __volatile__(
            // Save callee-saved registers onto the current process's stack
            "addi sp, sp, -16 * 4\n" // Allocate stack space for 13 4-byte words
            "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers
            "sw s0,  1  * 4(sp)\n"
            "sw s1,  2  * 4(sp)\n"
            "sw s2,  3  * 4(sp)\n"
            "sw s3,  4  * 4(sp)\n"
            "sw s4,  5  * 4(sp)\n"
            "sw s5,  6  * 4(sp)\n"
            "sw s6,  7  * 4(sp)\n"
            "sw s7,  8  * 4(sp)\n"
            "sw s8,  9  * 4(sp)\n"
            "sw s9,  10 * 4(sp)\n"
            "sw s10, 11 * 4(sp)\n"
            "sw s11, 12 * 4(sp)\n"
            "csrr t0, sstatus\n"
            "sw t0,  13 * 4(sp)\n"
            "csrr t0, sepc\n"
            "sw t0,  14 * 4(sp)\n"
            "csrr t0, sscratch\n"
            "sw t0,  15 * 4(sp)\n"

            // Switch the stack pointer
            "sw sp, (a0)\n"         // *prev_sp = sp;
            "lw sp, (a1)\n"         // sp = *next_sp;

            // Restore callee-saved registers from the next process's stack.
            "lw ra,  0  * 4(sp)\n"
            "lw s0,  1  * 4(sp)\n"
            "lw s1,  2  * 4(sp)\n"
            "lw s2,  3  * 4(sp)\n"
            "lw s3,  4  * 4(sp)\n"
            "lw s4,  5  * 4(sp)\n"
            "lw s5,  6  * 4(sp)\n"
            "lw s6,  7  * 4(sp)\n"
            "lw s7,  8  * 4(sp)\n"
            "lw s8,  9  * 4(sp)\n"
            "lw s9,  10 * 4(sp)\n"
            "lw s10, 11 * 4(sp)\n"
            "lw s11, 12 * 4(sp)\n"
            "lw t0,  13 * 4(sp)\n"
            "csrw sstatus, t0\n"
            "lw t0,  14 * 4(sp)\n"
            "csrw sepc, t0\n"
            "lw t0,  15 * 4(sp)\n"
            "csrw sscratch, t0\n"
            "addi sp, sp, 16 * 4\n"  // We've popped 13 4-byte words from the stack
            "ret\n"
        );
    }

    smp::PerCPU<HARTState<void(*)()>> hartstates;
    Atomic<uint32_t> tidCounter = Atomic<uint32_t>(0);

    void init() {
        for (uint32_t id = 0; id < smp::MAX_HARTS; id++) {
            hartstates.forCPU(id).current_thread = new TCBNoWork();
            hartstates.forCPU(id).idle_thread = new TCBWithIdle([] {
                // Idle thread logic
                // Handle request from blocking thread
                while (true) {
                    ASSERT(hartstates.mine().req != nullptr);
                    ASSERT(hartstates.mine().current_thread->preemptable == false);
                    ASSERT(hartstates.mine().idle_thread->preemptable == false);
                    //printf("Entered idle thread on core %d\n", smp::me());
                    hartstates.mine().req(); // Run the blocking thread's request
                    //printf("Finished req on core %d\n", smp::me());
                    hartstates.mine().req = nullptr; // Destroy the reference to the request
                    if (hartstates.mine().reap_thread != nullptr) {
                        //printf("Deleting on core %d\n", smp::me());
                        delete hartstates.mine().reap_thread;
                        hartstates.mine().reap_thread = nullptr;
                    }
                    ASSERT(hartstates.mine().current_thread->preemptable == false);
                    ASSERT(hartstates.mine().idle_thread->preemptable == false);
                    // Look for the next kthread to run
                    TCB* me = hartstates.mine().current_thread;
                    ASSERT(me == hartstates.mine().idle_thread);
                    TCB* next = nullptr;
                    //printf("Core %d while looping in idle\n", smp::me());
                    while (next == nullptr) {
                        ASSERT(hartstates.mine().current_thread->preemptable == false);
                        ASSERT(hartstates.mine().idle_thread->preemptable == false);
                        next = scheduler::next();
                    }
                    //printf("Exited idle thread on core %d\n", smp::me());
                    block(me, next, [] {});
                }
            });
            hartstates.forCPU(id).idle_thread->setPreemption(false); // Idle threads should never be preempted
            hartstates.forCPU(id).reap_thread = nullptr;
            hartstates.forCPU(id).req = nullptr;
        }
    }

    // Helper function for scheduling
    void kthread_schedule(TCB* kthread) {
        ASSERT(kthread != nullptr);
        scheduler::schedule(kthread);
    }

    void thread_entry() {
        // Get current thread
        TCB* my_thread = hartstates.mine().current_thread;
        ASSERT(my_thread != nullptr);
        my_thread->setPreemption(my_thread != hartstates.mine().idle_thread);
        my_thread->run();
        stop();
        PANIC("Stop returned in thread_entry, a critical failure occurred.\n");
    }

    // Gets the id of the currently running kthread
    uint32_t getktid() {
        bool was = pit::disable_interrupts();
        TCB* my_thread = hartstates.mine().current_thread;
        pit::restore_interrupts(was);
        return my_thread->tid;
    }

    // Yields the currently running thread and switches to another thread
    void yield() {
        bool was = pit::disable_interrupts();
        ASSERT(pit::are_interrupts_disabled());
        TCB* my_thread = hartstates.mine().current_thread;
        ASSERT(my_thread != nullptr);
        my_thread->setPreemption(false); // If a preempt happens now, ignore it
        pit::restore_interrupts(was);
        block(my_thread, hartstates.mine().idle_thread, [] {
            ASSERT(hartstates.mine().prev_thread != nullptr);
            scheduler::schedule(hartstates.mine().prev_thread);
        });
        my_thread->setPreemption(true);
    }

    // Context switches to a new thread, deletes the old thread
    void stop() {
        bool was = pit::disable_interrupts();
        ASSERT(pit::are_interrupts_disabled());
        TCB* my_thread = hartstates.mine().current_thread;
        //printf("Core %d called stop and disabled interrupts, my_thread = %x, idle = %x\n", smp::me(), my_thread, hartstates.mine().idle_thread);
        ASSERT(my_thread != nullptr);
        ASSERT(my_thread != hartstates.mine().idle_thread);
        my_thread->setPreemption(false); // If an interrupt happens now, ignore it
        pit::restore_interrupts(was);
        //printf("Stop called on core %d, my_thread = %x, idle_thread = %x\n", smp::me(), my_thread, hartstates.mine().idle_thread);
        block(my_thread, hartstates.mine().idle_thread, [] {
            ASSERT(hartstates.mine().prev_thread != nullptr);
            hartstates.mine().reap_thread = hartstates.mine().prev_thread; // Tell the idle thread to reap my_thread (put it in hartstate)
        });
        //printf("Stop returned on core %d\n", smp::me());
        PANIC("Stop somehow returned\n");
    }
};