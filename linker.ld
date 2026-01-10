ENTRY(_start)

SECTIONS {
    . = 1M;

    .multiboot2_header : ALIGN(8) {
        KEEP(*(.multiboot2_header))
    }

    .text : ALIGN(16) {
        *(.text)
    }

    .rodata : ALIGN(16) { *(.rodata*) }
    .data   : ALIGN(16) { *(.data*) }
    .bss    : ALIGN(16) { *(.bss*) *(COMMON) }
}
