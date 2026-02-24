SECTIONS {
    . = 0x00100000;
    .text ALIGN(4K) : {
        *(.text)
        kernel.asm.o
        kernel.c.o
    }
    .data ALIGN(4K) : { *(.data) }
    .bss ALIGN(4K) : { *(.bss) }
}
