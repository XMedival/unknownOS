#include <types.h>
#include <mmu.h>
#include <x86.h>
#include <proc.h>
#include <string.h>

// Global CPU structure (single CPU for now)
struct cpu cpu;

// Defined in gdt_flush.asm
extern void gdt_flush(uint gdtr_ptr, uint code_sel, uint data_sel);

void gdt_init(void) {
	// Clear CPU structure
	memset(&cpu, 0, sizeof(cpu));

	// Flat model: base=0, limit=4GiB
	cpu.gdt[0] = SEGNULL;
	cpu.gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFF, DPL_KERN);
	cpu.gdt[SEG_KDATA] = SEG(STA_W,         0x0, 0xFFFFF, DPL_KERN);
	cpu.gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFF, DPL_USER);
	cpu.gdt[SEG_UDATA] = SEG(STA_W,         0x0, 0xFFFFF, DPL_USER);

	// Initialize TSS
	cpu.ts.ss0 = SEG_KDATA << 3;   // Kernel stack segment
	cpu.ts.esp0 = 0;               // Will be set per-process
	cpu.ts.iomb = sizeof(struct taskstate);  // Disable I/O bitmap

	// TSS descriptor in GDT
	cpu.gdt[SEG_TSS] = SEGTSS(STS_T32A, (uint)&cpu.ts,
	                          sizeof(cpu.ts) - 1, DPL_KERN);

	lgdt(cpu.gdt, sizeof(cpu.gdt));

	// Reload segments
	gdt_flush(0, SEG_KCODE << 3, SEG_KDATA << 3);

	// Load Task Register
	ltr(SEG_TSS << 3);
}
