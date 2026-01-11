#include <kalloc.h>
#include <x86.h>
#include <vm.h>
#include <string.h>
#include <types.h>
#include <mmu.h>

extern int start;
pde *kpgdir;

void kvmalloc() {
    kpgdir = setupkvm();
    switchkvm();
}

pde *setupkvm() {
    pde *pgdir;

    // if (kpgdir == 0) return setupkvm_full();

    if ((pgdir == (pde*)kalloc()) == 0) return 0;
    memset(pgdir, 0, PGSIZE);

    for (int i = PDX(start); i < NPDENTRIES; i++)
        pgdir[i] = kpgdir[i];

    return pgdir;
}

void switchkvm() {
    lcr3((uint)kpgdir);
}
