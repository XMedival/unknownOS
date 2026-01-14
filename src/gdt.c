#include <types.h>
#include <mmu.h>
#include <x86.h>

static struct segdesc gdt[NSEGS];

// Defined in gdt_flush.asm
extern void gdt_flush(uint gdtr_ptr, uint code_sel, uint data_sel);

void gdt_init(void) {
	// Flat model: base=0, limit=4GiB
	gdt[0] = SEGNULL;
	gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFF, DPL_KERN);
	gdt[SEG_KDATA] = SEG(STA_W,         0x0, 0xFFFFF, DPL_KERN);
	gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0x0, 0xFFFFF, DPL_USER);
	gdt[SEG_UDATA] = SEG(STA_W,         0x0, 0xFFFFF, DPL_USER);

	lgdt(gdt, sizeof(gdt));

	// Reload segments
	gdt_flush(0, SEG_KCODE << 3, SEG_KDATA << 3);
}
