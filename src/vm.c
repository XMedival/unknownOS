#include <kalloc.h>
#include <x86.h>
#include <vm.h>
#include <string.h>
#include <types.h>
#include <mmu.h>

pde *kpgdir;

// Set up kernel page directory with identity mapping using 4MB pages
static pde *setupkvm(void) {
    pde *pgdir;

    pgdir = (pde*)kalloc();
    if (pgdir == 0)
        return 0;
    memset(pgdir, 0, PGSIZE);

    // Identity map the first 4GB using 4MB pages (PSE)
    for (uint i = 0; i < NPDENTRIES; i++) {
        pgdir[i] = (i << 22) | PTE_P | PTE_W | PTE_PS;
    }

    return pgdir;
}

void kvmalloc(void) {
    // Enable PSE (4MB pages)
    lcr4(rcr4() | CR4_PSE);

    // Set up page directory
    kpgdir = setupkvm();
    if (!kpgdir)
        return;

    // Load page directory
    lcr3((uint)kpgdir);

    // Enable paging
    lcr0(rcr0() | CR0_PG | CR0_WP);
}

void switchkvm(void) {
    if (kpgdir)
        lcr3((uint)kpgdir);
}
