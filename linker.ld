/* 64-bit kernel linker script */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start32)

SECTIONS {
    . = 1M;
    PROVIDE(start = .);

    /* Multiboot2 header must be in first 32KB */
    .multiboot2_header : ALIGN(8) {
        KEEP(*(.multiboot2_header))
    }

    .text : ALIGN(4K) {
        *(.text)
    }

    .rodata : ALIGN(4K) { *(.rodata*) }
    .data   : ALIGN(4K) { *(.data*) }

    .bss : ALIGN(4K) {
        *(.bss*)
        *(COMMON)
    }

    PROVIDE(end = .);
}
