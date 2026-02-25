#include "plic.h"
#include "../common/common.h"
#include "kernel.h"
#include "../drivers/virtio-blk/virtio.h"

namespace plic {

void init() {
    plic_set_priority(VIRTIO_IRQ, 1);
    plic_set_threshold(0, 0);
    plic_enable(VIRTIO_IRQ, 0); // Allow hart 0 to receive plic interrupts
    WRITE_CSR(sie, READ_CSR(sie) | (1 << 9));   // SEIE
    WRITE_CSR(sstatus, READ_CSR(sstatus) | 2); // SIE
}

void plic_enable(uint32_t irq, uint32_t hartid) {
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;

    uint32_t context = 2 * hartid + 1; // S-mode

    volatile uint32_t* enable =
        (uint32_t*)(PLIC_ENABLE_BASE +
                    context * PLIC_ENABLE_STRIDE +
                    reg * 4);

    *enable |= (1u << bit);
}

void plic_disable(uint32_t irq, uint32_t hartid) {
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;

    uint32_t context = 2 * hartid + 1; // S-mode

    volatile uint32_t* enable =
        (uint32_t*)(PLIC_ENABLE_BASE +
                    context * PLIC_ENABLE_STRIDE +
                    reg * 4);

    *enable &= ~(1u << bit);
}

void plic_set_priority(uint32_t irq, uint32_t prio) {
    volatile uint32_t* priority =
        (uint32_t*)(PLIC_PRIORITY_BASE + irq * 4);
    *priority = prio;
}

void plic_set_threshold(uint32_t hartid, uint32_t threshold) {
    uint32_t context = 2 * hartid + 1; // S-mode
    volatile uint32_t* th =
        (uint32_t*)(PLIC_THRESHOLD_BASE + context * 0x1000);
    *th = threshold;
}

uint32_t plic_claim(uint32_t hartid) {
    uint32_t context = 2 * hartid + 1; // S-mode
    volatile uint32_t* claim =
        (uint32_t*)(PLIC_CLAIM_BASE + context * 0x1000);
    return *claim;
}

void plic_complete(uint32_t hartid, uint32_t irq) {
    uint32_t context = 2 * hartid + 1; // S-mode
    volatile uint32_t* claim =
        (uint32_t*)(PLIC_CLAIM_BASE + context * 0x1000);
    *claim = irq;
}
};