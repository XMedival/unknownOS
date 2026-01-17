#include <types.h>
#include <mmu.h>
#include <x86.h>
#include <proc.h>
#include <string.h>

// Global CPU structure (single CPU for now)
struct cpu cpu;

// Defined in gdt_flush.asm
extern void gdt_flush(uint64_t gdtr_ptr, uint64_t code_sel, uint64_t data_sel);

void gdt_init(void) {
    // Clear CPU structure
    memset(&cpu, 0, sizeof(cpu));

    // Cast GDT as array of segdesc for easier access
    struct segdesc *gdt = (struct segdesc *)cpu.gdt;

    // Set up 64-bit GDT entries
    // In 64-bit mode, base and limit are ignored for code/data segments
    gdt[SEG_NULL]  = SEGNULL;                    // 0: Null
    gdt[SEG_KCODE] = SEG64_CODE(DPL_KERN);       // 1: Kernel code
    gdt[SEG_KDATA] = SEG64_DATA(DPL_KERN);       // 2: Kernel data
    gdt[SEG_UCODE] = SEG64_CODE(DPL_USER);       // 3: User code (32-bit compat)
    gdt[SEG_UDATA] = SEG64_DATA(DPL_USER);       // 4: User data
    gdt[SEG_UCODE64] = SEG64_CODE(DPL_USER);     // 5: User code 64-bit

    // Initialize 64-bit TSS
    cpu.ts.rsp0 = 0;  // Will be set when switching to user process
    cpu.ts.iomb = sizeof(struct taskstate);

    // TSS descriptor (16 bytes, spans slots 6 and 7)
    // Note: TSS descriptor in 64-bit mode is 16 bytes
    struct tssdesc *tss_desc = (struct tssdesc *)&gdt[SEG_TSS];
    set_tss_desc(tss_desc, (uint64_t)&cpu.ts, sizeof(cpu.ts) - 1);

    // Load GDT
    lgdt(cpu.gdt, sizeof(cpu.gdt));

    // Reload segments (code segment via far return, data segments directly)
    gdt_flush(0, SEG_KCODE << 3, SEG_KDATA << 3);

    // Load Task Register
    ltr(SEG_TSS << 3);
}
