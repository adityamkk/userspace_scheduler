#include "pit.h"
#include "../common/common.h"
#include "../common/sbi.h"
#include "kernel.h"
#include "smp.h"
#include "../threads/threads.h"

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
        printf("| Initializing PIT on core %d\n", smp::me());
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

    void pit_handler() {
        return;
    }

    bool disable_interrupts() {
        uint32_t sie = READ_CSR(sie);
        bool prev = (sie & (1 << 5) == 0) ? false : true;
        sie &= ~(1 << 5);       // Clear STIE
        WRITE_CSR(sie, sie);
        return prev;
    }

    void restore_interrupts(bool was) {
        uint32_t sie = READ_CSR(sie);
        if (was) {
            sie |= (1 << 5); // Set STIE
        } else {
            sie &= ~(1 << 5); // Clear STIE
        }
        WRITE_CSR(sie, sie);
    }
};