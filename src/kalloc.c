#include <kalloc.h>
#include <stdio.h>
#include <types.h>
#include <mmu.h>
#include <x86.h>

extern char end[], start[];
extern char *mbi;
extern uint mbi_size;
extern uintptr_t init_module_start;
extern uintptr_t init_module_end;

struct run {
    struct run *next;
};

struct {
    struct run *freelist;
    int nfree;
} kmem;

static inline int in_kernel(void* v) {
    return (v >= (void*)start) && (v < (void*)end);
}

static inline int in_mbi(void* v) {
    uintptr_t a = (uintptr_t)v;
    uintptr_t m0 = (uintptr_t)mbi;
    uintptr_t m1 = m0 + mbi_size;
    return (a >= m0) && (a < m1);
}

static inline int in_module(void* v) {
    if (init_module_start == 0) return 0;
    uintptr_t a = (uintptr_t)v;
    // Protect the entire module region (page-aligned)
    uintptr_t m0 = init_module_start & ~(PGSIZE - 1);
    uintptr_t m1 = (init_module_end + PGSIZE - 1) & ~(PGSIZE - 1);
    return (a >= m0) && (a < m1);
}

void kfree(char *v) {
    struct run *r;

    if ((uintptr_t)v % PGSIZE != 0) return;
    if (in_kernel(v)) return;
    if (in_mbi(v)) return;
    if (in_module(v)) return;

    uint64_t rflags;
    asm volatile("pushfq; popq %0" : "=r"(rflags));
    asm volatile("cli");
    r = (struct run*)v;
    r->next = kmem.freelist;
    kmem.freelist = r;
    kmem.nfree++;
    if (rflags & 0x200)
        asm volatile("sti");
}

void freerange(void *vstart, void *vend) {
    char *p, *e;
    p = (char*)PGROUNDUP((uintptr_t)vstart);
    e = (char*)PGROUNDDOWN((uintptr_t)vend);
    for (; p + PGSIZE <= e; p += PGSIZE)
        kfree(p);
}

char *kalloc(void) {
    struct run *r;

    uint64_t rflags;
    asm volatile("pushfq; popq %0" : "=r"(rflags));
    asm volatile("cli");
    r = kmem.freelist;
    if (r) {
        kmem.freelist = r->next;
        kmem.nfree--;
    }
    if (rflags & 0x200)
        asm volatile("sti");

    return (char*)r;
}

int freemem(void) {
    return kmem.nfree;
}
