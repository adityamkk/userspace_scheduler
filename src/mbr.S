    .section .text.boot
    .globl boot
    .type boot, @function

boot:
    /* OpenSBI passes hartid in a0 when jumping to payload (in S-mode) */
    mv    t0, a0             /* t0 = hartid passed by OpenSBI */

    /* optional: save hartid into tp for quick C access later */
    mv    tp, t0

    /* compute per-hart stack: stacks_top - hartid * STACK_SIZE */
    la    t1, __stack_top    /* t1 = stacks_top (linker symbol) */
    li    t2, 4096           /* KERNEL_STACK_SIZE is 4096 */
    mul   t2, t2, t0         /* t2 = hartid * STACK_SIZE */
    sub   sp, t1, t2         /* sp = stacks_top - hartid*STACK_SIZE */
    
    j kernel_main
