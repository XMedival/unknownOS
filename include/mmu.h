#pragma once
#include <types.h>

// This file contains definitions for the x86-64 memory management unit (MMU).

// In 64-bit mode with identity mapping, we don't use a high kernel base
#define KERNBASE 0x0

#define P2V(p) ((void*)(uintptr_t)(p))
#define V2P(v) ((uintptr_t)(v))

// Eflags register
#define FL_IF           0x00000200      // Interrupt Enable

// Control Register flags
#define CR0_PE          0x00000001      // Protection Enable
#define CR0_WP          0x00010000      // Write Protect
#define CR0_PG          0x80000000      // Paging

#define CR4_PAE         0x00000020      // Physical Address Extension
#define CR4_PSE         0x00000010      // Page size extension

// MSR registers
#define MSR_EFER        0xC0000080
#define EFER_LME        0x100           // Long Mode Enable
#define EFER_LMA        0x400           // Long Mode Active

// Various segment selectors (indexes into GDT)
#define SEG_NULL  0   // null
#define SEG_KCODE 1   // kernel code
#define SEG_KDATA 2   // kernel data+stack
#define SEG_UCODE 3   // user code (32-bit compat, not used)
#define SEG_UDATA 4   // user data+stack
#define SEG_UCODE64 5 // user code 64-bit
#define SEG_TSS   6   // TSS (takes 2 GDT slots in 64-bit mode)

// cpu->gdt[NSEGS] holds the above segments.
// TSS takes 2 slots in 64-bit mode (16 bytes)
#define NSEGS     8

#ifndef __ASSEMBLER__

// 64-bit Segment Descriptor
// In long mode, base and limit are ignored for code/data segments
struct segdesc {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;        // type, S, DPL, P
    uint8_t  flags_limit;   // limit_high:4, flags:4 (AVL, L, D/B, G)
    uint8_t  base_high;
} __attribute__((packed));

// 64-bit TSS Descriptor (16 bytes, spans 2 GDT entries)
struct tssdesc {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_mid2;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

// Null descriptor
#define SEGNULL ((struct segdesc){0, 0, 0, 0, 0, 0})

// 64-bit code segment (L=1, D=0)
// access: P=1, DPL, S=1, type (0xA for code execute/read)
// flags: G=1, L=1, D=0
#define SEG64_CODE(dpl) ((struct segdesc){ \
    .limit_low = 0xFFFF, \
    .base_low = 0, \
    .base_mid = 0, \
    .access = 0x9A | ((dpl) << 5), /* P=1, S=1, type=0xA (exec/read) */ \
    .flags_limit = 0xAF, /* G=1, L=1, D=0, limit_high=0xF */ \
    .base_high = 0 \
})

// 64-bit data segment (in long mode, most fields ignored)
#define SEG64_DATA(dpl) ((struct segdesc){ \
    .limit_low = 0xFFFF, \
    .base_low = 0, \
    .base_mid = 0, \
    .access = 0x92 | ((dpl) << 5), /* P=1, S=1, type=0x2 (read/write) */ \
    .flags_limit = 0xCF, /* G=1, D=1, limit_high=0xF */ \
    .base_high = 0 \
})

// Build TSS descriptor (returns first 8 bytes, second 8 bytes set separately)
static inline void set_tss_desc(struct tssdesc *desc, uint64_t base, uint32_t limit) {
    desc->limit_low = limit & 0xFFFF;
    desc->base_low = base & 0xFFFF;
    desc->base_mid = (base >> 16) & 0xFF;
    desc->access = 0x89;  // P=1, type=0x9 (64-bit TSS available)
    desc->flags_limit = ((limit >> 16) & 0xF);  // G=0 for byte granularity
    desc->base_mid2 = (base >> 24) & 0xFF;
    desc->base_high = (base >> 32) & 0xFFFFFFFF;
    desc->reserved = 0;
}

#endif // __ASSEMBLER__

#define DPL_KERN    0x0     // Kernel DPL
#define DPL_USER    0x3     // User DPL

// Application segment type bits
#define STA_X       0x8     // Executable segment
#define STA_W       0x2     // Writeable (non-executable segments)
#define STA_R       0x2     // Readable (executable segments)

// System segment type bits (64-bit mode)
#define STS_T64A    0x9     // Available 64-bit TSS
#define STS_T64B    0xB     // Busy 64-bit TSS
#define STS_IG64    0xE     // 64-bit Interrupt Gate
#define STS_TG64    0xF     // 64-bit Trap Gate

// Legacy 32-bit types (for reference)
#define STS_T32A    0x9     // Available 32-bit TSS
#define STS_IG32    0xE     // 32-bit Interrupt Gate
#define STS_TG32    0xF     // 32-bit Trap Gate

// ============================================================================
// 64-bit (4-level) Paging
// ============================================================================
//
// Virtual address structure (48-bit canonical):
// +-------9------+-------9------+-------9------+-------9------+----12----+
// |    PML4      |    PDPT      |     PD       |     PT       |  Offset  |
// +-------9------+-------9------+-------9------+-------9------+----12----+
//  bits 47:39     bits 38:30     bits 29:21     bits 20:12     bits 11:0

// Page table indices
#define PML4X(va)       (((uint64_t)(va) >> 39) & 0x1FF)
#define PDPTX(va)       (((uint64_t)(va) >> 30) & 0x1FF)
#define PDX(va)         (((uint64_t)(va) >> 21) & 0x1FF)
#define PTX(va)         (((uint64_t)(va) >> 12) & 0x1FF)

// Page sizes
#define PGSIZE          4096            // 4KB page
#define PGSIZE_2MB      (2 * 1024 * 1024)  // 2MB huge page
#define PGSIZE_1GB      (1024 * 1024 * 1024UL)  // 1GB huge page

// Number of entries per table
#define NPTENTRIES      512
#define NPDENTRIES      512
#define NPDPTENTRIES    512
#define NPML4ENTRIES    512

// Shifts for page table levels
#define PTXSHIFT        12
#define PDXSHIFT        21
#define PDPTXSHIFT      30
#define PML4XSHIFT      39

#define PGROUNDUP(sz)   (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a)  ((a) & ~(PGSIZE - 1))

// Page table/directory entry flags
#define PTE_P           0x001   // Present
#define PTE_W           0x002   // Writeable
#define PTE_U           0x004   // User accessible
#define PTE_PWT         0x008   // Write-through
#define PTE_PCD         0x010   // Cache disable
#define PTE_A           0x020   // Accessed
#define PTE_D           0x040   // Dirty
#define PTE_PS          0x080   // Page Size (2MB/1GB page)
#define PTE_G           0x100   // Global
#define PTE_NX          (1ULL << 63)  // No Execute (requires NX bit enabled)

// Address extraction from PTE (mask off flags, get physical address)
#define PTE_ADDR(pte)   ((uint64_t)(pte) & 0x000FFFFFFFFFF000ULL)
#define PTE_FLAGS(pte)  ((uint64_t)(pte) & 0xFFF)

#ifndef __ASSEMBLER__

// 64-bit Task State Segment
// In long mode, TSS is simplified - no saved registers, just stack pointers
struct taskstate64 {
    uint32_t reserved0;
    uint64_t rsp0;          // Stack pointer for ring 0
    uint64_t rsp1;          // Stack pointer for ring 1
    uint64_t rsp2;          // Stack pointer for ring 2
    uint64_t reserved1;
    uint64_t ist1;          // Interrupt Stack Table entries
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomb;          // I/O map base address
} __attribute__((packed));

// 64-bit Gate Descriptor (16 bytes for interrupt/trap gates)
struct gatedesc64 {
    uint16_t off_15_0;      // Low 16 bits of handler offset
    uint16_t cs;            // Code segment selector
    uint8_t  ist;           // IST index (bits 0-2), rest reserved
    uint8_t  flags;         // Type (4 bits), 0, DPL (2 bits), P (1 bit)
    uint16_t off_31_16;     // Bits 16-31 of handler offset
    uint32_t off_63_32;     // Bits 32-63 of handler offset
    uint32_t reserved;      // Reserved, must be 0
} __attribute__((packed));

// GDTR/IDTR structure for 64-bit mode
struct pseudodesc64 {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Set up a 64-bit interrupt/trap gate descriptor
// istrap: 1 for trap gate (keeps interrupts enabled), 0 for interrupt gate
// sel: code segment selector
// off: 64-bit offset to handler
// dpl: descriptor privilege level
// ist: IST index (0 = don't use IST, 1-7 = use IST entry)
static inline void setgate64(struct gatedesc64 *gate, int istrap, uint16_t sel,
                             uint64_t off, uint8_t dpl, uint8_t ist) {
    gate->off_15_0 = off & 0xFFFF;
    gate->cs = sel;
    gate->ist = ist & 0x7;
    gate->flags = (istrap ? STS_TG64 : STS_IG64) | ((dpl & 0x3) << 5) | 0x80;
    gate->off_31_16 = (off >> 16) & 0xFFFF;
    gate->off_63_32 = (off >> 32) & 0xFFFFFFFF;
    gate->reserved = 0;
}

// Macro version for compatibility
#define SETGATE64(gate, istrap, sel, off, d) \
    setgate64(&(gate), (istrap), (sel), (uint64_t)(off), (d), 0)

// Compatibility: keep old SETGATE working but redirect to 64-bit
#define SETGATE(gate, istrap, sel, off, d) \
    SETGATE64(gate, istrap, sel, off, d)

// Legacy taskstate for compatibility (renamed)
struct taskstate {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomb;
} __attribute__((packed));

// Legacy gatedesc - actually use 64-bit version
typedef struct gatedesc64 gatedesc;

#endif // __ASSEMBLER__
