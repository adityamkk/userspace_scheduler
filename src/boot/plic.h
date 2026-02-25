#pragma once

#include "../common/common.h"

#define PLIC_BASE 0x0C000000UL
#define PLIC_ENABLE_BASE (PLIC_BASE + 0x2000)
#define PLIC_ENABLE_STRIDE 0x80
#define PLIC_PRIORITY_BASE (PLIC_BASE + 0x0000)
#define PLIC_THRESHOLD_BASE (PLIC_BASE + 0x200000)
#define PLIC_CLAIM_BASE (PLIC_BASE + 0x200004)

namespace plic {
void init();
void plic_enable(uint32_t irq, uint32_t hartid);
void plic_disable(uint32_t irq, uint32_t hartid);
void plic_set_priority(uint32_t irq, uint32_t prio);
void plic_set_threshold(uint32_t hartid, uint32_t threshold);
uint32_t plic_claim(uint32_t hartid);
void plic_complete(uint32_t hartid, uint32_t irq);
};