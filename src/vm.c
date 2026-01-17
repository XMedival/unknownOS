#include <kalloc.h>
#include <x86.h>
#include <vm.h>
#include <string.h>
#include <types.h>
#include <mmu.h>
#include <proc.h>
#include <log.h>

pde *kpgdir;

extern struct cpu cpu;

// Forward declarations
static pte_t* walkpgdir(pde *pgdir, const void *va, int alloc);
static int mappages(pde *pgdir, void *va, uint size, uint pa, int perm);
int deallocuvm(pde *pgdir, uint oldsz, uint newsz);

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

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va. If alloc!=0,
// create any required page table pages.
static pte_t* walkpgdir(pde *pgdir, const void *va, int alloc) {
    pde *pde_entry;
    pte_t *pgtab;

    pde_entry = &pgdir[PDX(va)];

    if (*pde_entry & PTE_P) {
        // Page directory entry exists
        if (*pde_entry & PTE_PS) {
            // It's a 4MB page - can't use for 4KB user mapping
            return 0;
        }
        pgtab = (pte_t*)PTE_ADDR(*pde_entry);
    } else {
        // Need to allocate a new page table
        if (!alloc || (pgtab = (pte_t*)kalloc()) == 0)
            return 0;
        memset(pgtab, 0, PGSIZE);
        // User page table: set U bit so user can access
        *pde_entry = (uint)pgtab | PTE_P | PTE_W | PTE_U;
    }

    return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not be page-aligned.
static int mappages(pde *pgdir, void *va, uint size, uint pa, int perm) {
    char *a, *last;
    pte_t *pte;

    a = (char*)PGROUNDDOWN((uint)va);
    last = (char*)PGROUNDDOWN((uint)va + size - 1);

    for (;;) {
        if ((pte = walkpgdir(pgdir, a, 1)) == 0)
            return -1;
        // Allow overwriting existing PTEs (needed for user pages in first 4MB)
        // The old mapping was kernel-only; we're replacing with user-accessible
        *pte = pa | perm | PTE_P;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

// Create a user page directory with kernel mappings copied
pde* setupuvm(void) {
    pde *pgdir;
    pte_t *pgtab;

    pgdir = (pde*)kalloc();
    if (pgdir == 0)
        return 0;
    memset(pgdir, 0, PGSIZE);

    // For PDE 0 (first 4MB), use 4KB pages so user space can be allocated
    // Map ALL of first 4MB as kernel-only (for scheduler stack, kernel code, etc.)
    // User pages in this region will be created by allocuvm with PTE_U flag
    pgtab = (pte_t*)kalloc();
    if (pgtab == 0) {
        kfree((char*)pgdir);
        return 0;
    }
    memset(pgtab, 0, PGSIZE);

    // Identity map the first 4MB with 4KB pages (kernel-only)
    // This includes: low memory, kernel code (0x100000+), etc.
    // User pages will overwrite these PTEs with PTE_U flag via allocuvm
    for (uint pa = 0; pa < 0x400000; pa += PGSIZE) {
        pgtab[PTX(pa)] = pa | PTE_P | PTE_W;
    }
    pgdir[0] = (uint)pgtab | PTE_P | PTE_W | PTE_U;  // PTE_U on PDE to allow page table walk

    // Copy remaining kernel mappings (4MB pages for PDEs 1+)
    for (uint i = 1; i < NPDENTRIES; i++) {
        if (kpgdir[i] & PTE_P) {
            // Copy kernel mapping without PTE_U
            pgdir[i] = kpgdir[i] & ~PTE_U;
        }
    }

    return pgdir;
}

// Allocate page tables and physical memory to grow process from oldsz to newsz.
// Does not zero the new memory.
int allocuvm(pde *pgdir, uint oldsz, uint newsz) {
    char *mem;
    uint a;

    if (newsz >= KERNBASE)
        return 0;
    if (newsz < oldsz)
        return oldsz;

    a = PGROUNDUP(oldsz);
    for (; a < newsz; a += PGSIZE) {
        mem = kalloc();
        if (mem == 0) {
            LOG_WARN("allocuvm: out of memory");
            deallocuvm(pgdir, newsz, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages(pgdir, (char*)a, PGSIZE, (uint)mem, PTE_W | PTE_U) < 0) {
            LOG_WARN("allocuvm: mappages failed");
            kfree(mem);
            deallocuvm(pgdir, newsz, oldsz);
            return 0;
        }
    }
    return newsz;
}

// Deallocate user pages to bring the process size from oldsz to newsz.
int deallocuvm(pde *pgdir, uint oldsz, uint newsz) {
    pte_t *pte;
    uint a, pa;

    if (newsz >= oldsz)
        return oldsz;

    a = PGROUNDUP(newsz);
    for (; a < oldsz; a += PGSIZE) {
        pte = walkpgdir(pgdir, (char*)a, 0);
        if (!pte) {
            // No page table for this range - skip to next PDE boundary
            a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
        } else if (*pte & PTE_P) {
            pa = PTE_ADDR(*pte);
            if (pa == 0) {
                LOG_WARN("deallocuvm: null physical address");
            }
            kfree((char*)pa);
            *pte = 0;
        }
    }
    return newsz;
}

// Free a page table and all the physical memory pages in the user part.
void freevm(pde *pgdir) {
    uint i;

    if (pgdir == 0)
        return;

    // Deallocate all user memory (below KERNBASE)
    deallocuvm(pgdir, KERNBASE, 0);

    // Free page tables (not 4MB pages)
    for (i = 0; i < NPDENTRIES; i++) {
        if ((pgdir[i] & PTE_P) && !(pgdir[i] & PTE_PS)) {
            // This is a page table, not a 4MB page
            char *v = (char*)PTE_ADDR(pgdir[i]);
            kfree(v);
        }
    }
    kfree((char*)pgdir);
}

// Load data from src to virtual address va in a given page directory.
// va must be page-aligned. Returns 0 on success, -1 on failure.
int loaduvm(pde *pgdir, char *addr, char *src, uint sz) {
    uint i, n;
    pte_t *pte;
    char *pa;

    if ((uint)addr % PGSIZE != 0)
        return -1;

    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walkpgdir(pgdir, addr + i, 0)) == 0)
            return -1;
        if (!(*pte & PTE_P))
            return -1;
        pa = (char*)PTE_ADDR(*pte);
        n = (sz - i < PGSIZE) ? sz - i : PGSIZE;
        memmove(pa, src + i, n);
    }
    return 0;
}

// Switch TSS and page directory to correspond to process p.
void switchuvm(struct proc *p) {
    if (p == 0)
        return;
    if (p->kstack == 0)
        return;
    if (p->pgdir == 0)
        return;

    // Update TSS with this process's kernel stack
    cpu.ts.esp0 = (uint)p->kstack + KSTACKSIZE;

    // Switch to process's page directory
    lcr3((uint)p->pgdir);
}
