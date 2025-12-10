#pragma once

#include "common.h"
#include "heap.h"
#include "smp.h"

namespace threads {

    constexpr size_t THREAD_STACK_SIZE = 8 * 1024;
    constexpr size_t IDLE_STACK_SIZE = 1 * 1024;

    extern void thread_entry();
    extern __attribute__((naked)) void context_switch(uint32_t *prev_sp, uint32_t *next_sp);

    // Base class for TCBs
    class TCB {
    public:
        uint32_t sp; // current stack pointer value for this thread
        bool preemptable; // whether this TCB can be preempted or not
        bool setPreemption(bool preemption) {
            bool oldFlag = preemptable;
            preemptable = preemption;
            return oldFlag;
        }
        virtual void run() = 0;
        virtual ~TCB() {}
    };

    // State of a single hardware thread
    template <typename BlockRequest>
    struct HARTState {
        TCB* current_thread; // The current thread being run on a HART (nullptr if no current thread)
        TCB* idle_thread; // The idle thread associated with a HART
        BlockRequest req; // Request lambda to the idle thread to do work on the current thread's behalf
        TCB* prev_thread; // Normally nullptr, block will set this to the old thread
        TCB* reap_thread; // Normally nullptr, a BlockRequest will set this whenever it wants a thread to be reaped in the idle thread
    };

    extern smp::PerCPU<HARTState<void(*)()>> hartstates;
    extern void init();
    
    // Assumes preemption is disabled for the current thread
    template <typename BlockRequest>
    void block(TCB* my_thread, TCB* next, BlockRequest req) {
        ASSERT(my_thread != nullptr);
        ASSERT(next != nullptr);
        if (next == hartstates.mine().idle_thread) {
            ASSERT(!next->preemptable);
        } else {
            ASSERT(!next->preemptable);
            //next->setPreemption(false); // Set the next thread's preemption to false
        }
        hartstates.mine().req = req; // Set the request, tells the idle thread what to do
        hartstates.mine().current_thread = next; // IMPORTANT: Assumes req will put the old TCB where it needs to go
        hartstates.mine().prev_thread = my_thread; // Sets this so that BlockRequests can find it
        ASSERT(hartstates.mine().idle_thread != nullptr);
        context_switch(&(my_thread->sp), &(next->sp));
    }

    extern void yield();
    extern void stop();

    class TCBNoWork : public TCB {
    public:
        TCBNoWork() {}
        ~TCBNoWork() {}
        void run() override {
            PANIC("Cannot run a TCBNoWork\n");
        }
    };

    template <typename Work>
    class TCBWithWork : public TCB {
        Work work; // Callable object type
        void* stack_mem; // Localtion of the stack (nullptr if no stack)

    public:
        TCBWithWork(Work work): work(work), stack_mem(nullptr) {
            preemptable = false;
            // Allocate a stack from the heap
            stack_mem = heap::malloc(THREAD_STACK_SIZE);
            if (!stack_mem) PANIC("Kernel heap out of memory: Could not allocate new thread stack\n");

            // Initialize stack so that context_switch can restore registers and
            // return into the trampoline. context_switch expects the stack to
            // contain (at sp): ra, s0..s11 (13 words).
            uint32_t *stack_top = (uint32_t *)((uintptr_t)stack_mem + THREAD_STACK_SIZE);

            // Ensure 4-byte alignment
            stack_top = (uint32_t *)((uintptr_t)stack_top & ~0x3);

            // Reserve space for 13 registers
            stack_top -= 13;

            // Layout expected by context_switch: [0]=ra, [1]=s0, [2]=s1 ... [12]=s11
            // ra points to the thread entry which calls run
            stack_top[0] = (uint32_t)((uintptr_t)thread_entry);

            // zero the remaining callee-saved registers
            for (int i = 1; i < 13; ++i) stack_top[i] = 0;

            // Initialize the saved stack pointer value in the base class field
            this->sp = (uint32_t)((uintptr_t)stack_top);
        }

        ~TCBWithWork() {
            if (stack_mem) heap::free(stack_mem);
        }

        void run() override {
            this->preemptable = true;
            this->work();
        }
    };

    template <typename Work>
    class TCBWithIdle : public TCB {
        Work work; // Callable object type
        void* stack_mem; // Localtion of the stack (nullptr if no stack)

    public:
        TCBWithIdle(Work work): work(work), stack_mem(nullptr) {
            preemptable = false;
            // Allocate a stack from the heap
            stack_mem = heap::malloc(IDLE_STACK_SIZE);
            if (!stack_mem) PANIC("Kernel heap out of memory: Could not allocate new thread stack\n");

            // Initialize stack so that context_switch can restore registers and
            // return into the trampoline. context_switch expects the stack to
            // contain (at sp): ra, s0..s11 (13 words).
            uint32_t *stack_top = (uint32_t *)((uintptr_t)stack_mem + IDLE_STACK_SIZE);

            // Ensure 4-byte alignment
            stack_top = (uint32_t *)((uintptr_t)stack_top & ~0x3);

            // Reserve space for 13 registers
            stack_top -= 13;

            // Layout expected by context_switch: [0]=ra, [1]=s0, [2]=s1 ... [12]=s11
            // ra points to the thread entry which calls run
            stack_top[0] = (uint32_t)((uintptr_t)thread_entry);

            // zero the remaining callee-saved registers
            for (int i = 1; i < 13; ++i) stack_top[i] = 0;

            // Initialize the saved stack pointer value in the base class field
            this->sp = (uint32_t)((uintptr_t)stack_top);
        }

        ~TCBWithIdle() {
            if (stack_mem) heap::free(stack_mem);
        }

        void run() override {
            work();
        }
    };

    extern void kthread_schedule(TCB* kthread);
    // Creates a new kernel thread
    template <typename Task>
    void kthread(Task task) {
        TCB* k_thread = new TCBWithWork<Task>(task);
        kthread_schedule(k_thread);
    }
}