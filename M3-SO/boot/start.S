.section .text.boot
.global _start
.type _start, @function

_start:

    # GP
    la gp, __global_pointer$

    # Zerar BSS
    la t0, __bss_start
    la t1, __bss_end
1:
    bge t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b
2:

    # Stack
    la sp, stack_top

    call kernel_main

3:  j 3b


.section .bss
.align 12
stack:
    .space 8192
stack_top: