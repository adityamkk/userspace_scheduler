#include "pit.h"
#include "../common/common.h"
#include "../common/sbi.h"
#include "kernel.h"
#include "smp.h"
#include "../threads/threads.h"
#include "../threads/scheduler.h"

namespace pit {

    void set_timer(uint64_t next_cycle) {
        uint32_t lo = (uint32_t)next_cycle;
        uint32_t hi = (uint32_t)(next_cycle >> 32);

        // FID=0, EID=0x54494D45 (TIME extension)
        sbi_call(lo, hi, 0, 0, 0, 0,
                0 /* fid */,
                0x54494D45 /* eid */);
    }

    void init() {
        // Enable STIE
        uint32_t sie = READ_CSR(sie);
        sie |= (1 << 5);
        WRITE_CSR(sie, sie);

        // Enable global interrupts in sstatus
        uint32_t sstatus = READ_CSR(sstatus);
        sstatus |= 0x2; // SSTATUS.SIE = bit 1
        WRITE_CSR(sstatus, sstatus);

        uint64_t now = get_time();
        set_timer(now + 1024 * 1024);
    }

    uint64_t get_time() {
        uint32_t lo = READ_CSR(time);
        uint32_t hi = READ_CSR(timeh);
        return ((uint64_t)hi << 32) | lo;
    }

    /**
     * The timer interrupt handler tries to preempt the currently running thread and context switch to another one
     * This handler operates in O(1) to be efficient
     */
    void pit_handler() {
        // If the currently running thread has preemption disabled or is an idle thread, don't preempt
        threads::TCB* my_thread = threads::hartstates.mine().current_thread;
        ASSERT(my_thread != nullptr);
        if (!my_thread->preemptable) {
            // Short circuit
            return;
        }

        threads::TCB* idle_thread = threads::hartstates.mine().idle_thread;
        ASSERT(my_thread != idle_thread);
        my_thread->setPreemption(false);
        // Preempt!
        ASSERT(pit::are_interrupts_disabled());
        threads::block(my_thread, idle_thread, [] {
            ASSERT(threads::hartstates.mine().prev_thread != nullptr);
            scheduler::schedule(threads::hartstates.mine().prev_thread);
        });
        ASSERT(pit::are_interrupts_disabled());
        ASSERT(my_thread == threads::hartstates.mine().current_thread);
        ASSERT(my_thread != idle_thread);
        //printf("Core %d exiting from pit handler, about to run useful work\n", smp::me());
        my_thread->setPreemption(true);
        return;
    }

    bool disable_interrupts() {
        /*
        uint32_t sie = READ_CSR(sie);
        bool prev = ((sie & (1 << 5)) == 0) ? false : true;
        sie &= ~(1 << 5);       // Clear STIE
        WRITE_CSR(sie, sie);
        return prev;
        */
        uint32_t status = READ_CSR(sstatus);
        bool prev = ((status & (1 << 1)) == 0) ? false : true;
        status &= ~(1 << 1);
        WRITE_CSR(sstatus, status);
        return prev;
    }

    void restore_interrupts(bool was) {
        /*
        uint32_t sie = READ_CSR(sie);
        if (was) {
            sie |= (1 << 5); // Set STIE
        } else {
            sie &= ~(1 << 5); // Clear STIE
        }
        WRITE_CSR(sie, sie);
        */
        uint32_t status = READ_CSR(sstatus);
        if (was) {
            status |= (1 << 1);  
        } else {
            status &= ~(1 << 1);
        }
        WRITE_CSR(sstatus, status);
    }

    bool are_interrupts_disabled() {
        /*
        uint32_t sie = READ_CSR(sie);
        return ((sie & (1 << 5)) == 0) ? true : false;
        */
        uint32_t status = READ_CSR(sstatus);
        return ((status & (1 << 1)) == 0) ? true : false;
    }
};