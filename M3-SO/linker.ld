OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
  . = 0x80200000;

  .text : {
    KEEP(*(.text.boot))
    *(.text*)
  }

  .rodata : { *(.rodata*) }

  .data : { *(.data*) }

  __global_pointer$ = . + 0x800;

  .bss : {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    __bss_end = .;
  }
}