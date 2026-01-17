#pragma once
#include <types.h>

// Port I/O
static inline uchar inb(ushort port) {
    uchar data;
    asm volatile("inb %1,%0" : "=a" (data) : "d" (port));
    return data;
}

static inline ushort inw(ushort port) {
    ushort data;
    asm volatile("inw %1,%0" : "=a" (data) : "d" (port));
    return data;
}

static inline uint inl(ushort port) {
    uint data;
    asm volatile("inl %1,%0" : "=a" (data) : "dN" (port));
    return data;
}

static inline void insl(int port, void *addr, int cnt) {
    asm volatile("cld; rep insl" :
                 "=D" (addr), "=c" (cnt) :
                 "d" (port), "0" (addr), "1" (cnt) :
                 "memory", "cc");
}

static inline void outb(ushort port, uchar data) {
    asm volatile("outb %0,%1" : : "a" (data), "d" (port));
}

static inline void outw(ushort port, ushort data) {
    asm volatile("outw %0,%1" : : "a" (data), "d" (port));
}

static inline void outl(ushort port, uint data) {
    asm volatile("outl %0,%1" : : "a" (data), "dN" (port));
}

static inline void outsl(int port, const void *addr, int cnt) {
    asm volatile("cld; rep outsl" :
                 "=S" (addr), "=c" (cnt) :
                 "d" (port), "0" (addr), "1" (cnt) :
                 "cc");
}

static inline void stosb(void *addr, int data, int cnt) {
    asm volatile("cld; rep stosb" :
                 "=D" (addr), "=c" (cnt) :
                 "0" (addr), "1" (cnt), "a" (data) :
                 "memory", "cc");
}

static inline void stosl(void *addr, int data, int cnt) {
    asm volatile("cld; rep stosl" :
                 "=D" (addr), "=c" (cnt) :
                 "0" (addr), "1" (cnt), "a" (data) :
                 "memory", "cc");
}

// 64-bit GDT/IDT loading
// GDTR/IDTR format: 16-bit limit, 64-bit base

static inline void lgdt(void *p, int size) {
    volatile struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) pd;

    pd.limit = size - 1;
    pd.base = (uint64_t)p;

    asm volatile("lgdt %0" : : "m" (pd));
}

static inline void lidt(void *p, int size) {
    volatile struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) pd;

    pd.limit = size - 1;
    pd.base = (uint64_t)p;

    asm volatile("lidt %0" : : "m" (pd));
}

static inline void ltr(ushort sel) {
    asm volatile("ltr %0" : : "r" (sel));
}

static inline uint64_t readrflags(void) {
    uint64_t rflags;
    asm volatile("pushfq; popq %0" : "=r" (rflags));
    return rflags;
}

// Legacy name
static inline uint readeflags(void) {
    return (uint)readrflags();
}

static inline void loadgs(ushort v) {
    asm volatile("movw %0, %%gs" : : "r" (v));
}

static inline void cli(void) {
    asm volatile("cli");
}

static inline void sti(void) {
    asm volatile("sti");
}

static inline void hlt(void) {
    asm volatile("hlt");
}

static inline uint64_t xchg64(volatile uint64_t *addr, uint64_t newval) {
    uint64_t result;
    asm volatile("lock; xchgq %1, %0" :
                 "+m" (*addr), "=r" (result) :
                 "1" (newval) :
                 "cc");
    return result;
}

// 32-bit version for compatibility
static inline uint xchg(volatile uint *addr, uint newval) {
    uint result;
    asm volatile("lock; xchgl %1, %0" :
                 "+m" (*addr), "=r" (result) :
                 "1" (newval) :
                 "cc");
    return result;
}

// Control registers - 64-bit versions
static inline uint64_t rcr2(void) {
    uint64_t val;
    asm volatile("movq %%cr2, %0" : "=r" (val));
    return val;
}

static inline void lcr3(uint64_t val) {
    asm volatile("movq %0, %%cr3" : : "r" (val) : "memory");
}

static inline uint64_t rcr3(void) {
    uint64_t val;
    asm volatile("movq %%cr3, %0" : "=r" (val));
    return val;
}

static inline uint64_t rcr0(void) {
    uint64_t val;
    asm volatile("movq %%cr0, %0" : "=r" (val));
    return val;
}

static inline void lcr0(uint64_t val) {
    asm volatile("movq %0, %%cr0" : : "r" (val) : "memory");
}

static inline uint64_t rcr4(void) {
    uint64_t val;
    asm volatile("movq %%cr4, %0" : "=r" (val));
    return val;
}

static inline void lcr4(uint64_t val) {
    asm volatile("movq %0, %%cr4" : : "r" (val) : "memory");
}

// MSR access
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = val & 0xFFFFFFFF;
    uint32_t high = val >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// Initialize x87 FPU
static inline void fpu_init(void) {
    uint64_t cr0 = rcr0();
    // Clear EM (emulation), set MP (monitor coprocessor)
    cr0 &= ~(1ULL << 2);  // Clear EM bit
    cr0 |= (1ULL << 1);   // Set MP bit
    lcr0(cr0);

    // Initialize FPU
    asm volatile("fninit");
}

// ============================================================================
// 64-bit Trap Frame
// ============================================================================
// Layout built by trap.asm and passed to trap()
// Must match the push order in trap.asm!

struct trapframe {
    // General purpose registers saved by software
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    // Trap number and error code
    uint64_t trapno;
    uint64_t err;

    // Below here pushed by CPU on interrupt/exception
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;     // Always pushed in 64-bit mode
    uint64_t ss;      // Always pushed in 64-bit mode
};
