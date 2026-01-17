#include <kalloc.h>
#include <x86.h>
#include <vm.h>
#include <string.h>
#include <types.h>
#include <mmu.h>
#include <proc.h>
#include <log.h>

// Kernel page table (PML4) - in 64-bit mode, this is already set up by boot64.asm
// We keep a reference to it for switching back after user processes
pte_t *kpgdir;

extern struct cpu cpu;

// Forward declarations
static pte_t* walkpgdir(pte_t *pml4, const void *va, int alloc);
static int mappages(pte_t *pml4, void *va, uint64_t size, uint64_t pa, int perm);
uint64_t deallocuvm(pte_t *pml4, uint64_t oldsz, uint64_t newsz);

// In 64-bit mode, paging is already enabled by boot64.asm
// This function just saves the current PML4 reference
void kvmalloc(void) {
    // Get the current PML4 from CR3 (set up by boot64.asm)
    kpgdir = (pte_t*)rcr3();

    // Note: boot64.asm already set up identity mapping for first 4GB
    // using 2MB pages. We don't need to set up paging again.
}

void switchkvm(void) {
    if (kpgdir)
        lcr3((uint64_t)kpgdir);
}

// Walk 4-level page table to find PTE for virtual address
// In 64-bit mode: PML4 -> PDPT -> PD -> PT -> Page
// For simplicity, we'll use 4KB pages for user space allocation
static pte_t* walkpgdir(pte_t *pml4, const void *va, int alloc) {
    pte_t *pdpt, *pd, *pt;
    pte_t *entry;

    // Level 4: PML4
    entry = &pml4[PML4X(va)];
    if (*entry & PTE_P) {
        pdpt = (pte_t*)PTE_ADDR(*entry);
    } else {
        if (!alloc || (pdpt = (pte_t*)kalloc()) == 0)
            return 0;
        memset(pdpt, 0, PGSIZE);
        *entry = (uint64_t)pdpt | PTE_P | PTE_W | PTE_U;
    }

    // Level 3: PDPT
    entry = &pdpt[PDPTX(va)];
    if (*entry & PTE_P) {
        if (*entry & PTE_PS) {
            // 1GB page - can't walk further
            return 0;
        }
        pd = (pte_t*)PTE_ADDR(*entry);
    } else {
        if (!alloc || (pd = (pte_t*)kalloc()) == 0)
            return 0;
        memset(pd, 0, PGSIZE);
        *entry = (uint64_t)pd | PTE_P | PTE_W | PTE_U;
    }

    // Level 2: PD
    entry = &pd[PDX(va)];
    if (*entry & PTE_P) {
        if (*entry & PTE_PS) {
            // 2MB page - can't walk further for 4KB allocation
            return 0;
        }
        pt = (pte_t*)PTE_ADDR(*entry);
    } else {
        if (!alloc || (pt = (pte_t*)kalloc()) == 0)
            return 0;
        memset(pt, 0, PGSIZE);
        *entry = (uint64_t)pt | PTE_P | PTE_W | PTE_U;
    }

    // Level 1: PT
    return &pt[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
static int mappages(pte_t *pml4, void *va, uint64_t size, uint64_t pa, int perm) {
    char *a, *last;
    pte_t *pte;

    a = (char*)PGROUNDDOWN((uint64_t)va);
    last = (char*)PGROUNDDOWN((uint64_t)va + size - 1);

    for (;;) {
        if ((pte = walkpgdir(pml4, a, 1)) == 0)
            return -1;
        *pte = pa | perm | PTE_P;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

// Create a user page table (PML4)
// We create a fresh PML4 for user space but preserve kernel mappings
// from boot64.asm (identity mapping of first 4GB with 2MB pages).
// For the first 2MB, we use 4KB pages to allow fine-grained user space control.
pte_t* setupuvm(void) {
    pte_t *pml4;

    pml4 = (pte_t*)kalloc();
    if (pml4 == 0)
        return 0;
    memset(pml4, 0, PGSIZE);

    // Copy PML4 entries 1-511 from kernel (addresses above 512GB)
    if (kpgdir) {
        for (int i = 1; i < NPML4ENTRIES; i++) {
            if (kpgdir[i] & PTE_P) {
                pml4[i] = kpgdir[i] & ~PTE_U;
            }
        }
    }

    // Get kernel's PDPT from PML4[0]
    pte_t *kpdpt = NULL;
    if (kpgdir && (kpgdir[0] & PTE_P)) {
        kpdpt = (pte_t*)PTE_ADDR(kpgdir[0]);
    }

    // Create a fresh PDPT for PML4[0] (first 512GB)
    pte_t *pdpt = (pte_t*)kalloc();
    if (pdpt == 0) {
        kfree((char*)pml4);
        return 0;
    }
    memset(pdpt, 0, PGSIZE);

    // Copy PDPT entries 1-511 from kernel (1GB-512GB, kernel uses 1-3 for 1-4GB)
    if (kpdpt) {
        for (int i = 1; i < NPDPTENTRIES; i++) {
            if (kpdpt[i] & PTE_P) {
                pdpt[i] = kpdpt[i] & ~PTE_U;  // Remove user bit for kernel pages
            }
        }
    }

    // Get kernel's PD for first 1GB
    pte_t *kpd = NULL;
    if (kpdpt && (kpdpt[0] & PTE_P)) {
        kpd = (pte_t*)PTE_ADDR(kpdpt[0]);
    }

    // Create a fresh PD for PDPT[0] (first 1GB)
    pte_t *pd = (pte_t*)kalloc();
    if (pd == 0) {
        kfree((char*)pdpt);
        kfree((char*)pml4);
        return 0;
    }
    memset(pd, 0, PGSIZE);

    // Copy PD entries 1-511 from kernel (2MB-1GB range, using 2MB pages)
    if (kpd) {
        for (int i = 1; i < NPDENTRIES; i++) {
            if (kpd[i] & PTE_P) {
                pd[i] = kpd[i] & ~PTE_U;  // Remove user bit for kernel pages
            }
        }
    }

    // For PD[0] (first 2MB), use 4KB pages for fine-grained user/kernel separation
    pte_t *pt = (pte_t*)kalloc();
    if (pt == 0) {
        kfree((char*)pd);
        kfree((char*)pdpt);
        kfree((char*)pml4);
        return 0;
    }

    // Identity map first 2MB with 4KB pages (kernel only, no PTE_U)
    for (int i = 0; i < 512; i++) {
        pt[i] = (i * PGSIZE) | PTE_P | PTE_W;  // No PTE_U - kernel only
    }

    // Link page tables
    pd[0] = (uint64_t)pt | PTE_P | PTE_W | PTE_U;  // PTE_U on directory to allow walk
    pdpt[0] = (uint64_t)pd | PTE_P | PTE_W | PTE_U;
    pml4[0] = (uint64_t)pdpt | PTE_P | PTE_W | PTE_U;

    return pml4;
}

// Allocate page tables and physical memory to grow process from oldsz to newsz.
uint64_t allocuvm(pte_t *pml4, uint64_t oldsz, uint64_t newsz) {
    char *mem;
    uint64_t a;

    if (newsz >= 0x800000000000ULL)  // User space limit (128TB)
        return 0;
    if (newsz < oldsz)
        return oldsz;

    a = PGROUNDUP(oldsz);
    for (; a < newsz; a += PGSIZE) {
        mem = kalloc();
        if (mem == 0) {
            LOG_WARN("allocuvm: out of memory");
            deallocuvm(pml4, newsz, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages(pml4, (char*)a, PGSIZE, (uint64_t)mem, PTE_W | PTE_U) < 0) {
            LOG_WARN("allocuvm: mappages failed");
            kfree(mem);
            deallocuvm(pml4, newsz, oldsz);
            return 0;
        }
    }
    return newsz;
}

// Deallocate user pages to bring the process size from oldsz to newsz.
uint64_t deallocuvm(pte_t *pml4, uint64_t oldsz, uint64_t newsz) {
    pte_t *pte;
    uint64_t a, pa;

    if (newsz >= oldsz)
        return oldsz;

    a = PGROUNDUP(newsz);
    for (; a < oldsz; a += PGSIZE) {
        pte = walkpgdir(pml4, (char*)a, 0);
        if (!pte) {
            // No page table - skip ahead
            a = PGROUNDDOWN(a + PGSIZE_2MB) - PGSIZE;
            continue;
        }
        if (*pte & PTE_P) {
            pa = PTE_ADDR(*pte);
            if (pa == 0) {
                LOG_WARN("deallocuvm: null physical address");
                continue;
            }
            kfree((char*)pa);
            *pte = 0;
        }
    }
    return newsz;
}

// Free a page table and all the physical memory pages in the user part.
void freevm(pte_t *pml4) {
    if (pml4 == 0)
        return;

    // Deallocate all user memory
    deallocuvm(pml4, 0x800000000000ULL, 0);

    // Free page tables (walk and free non-huge page tables)
    // This is simplified - in production you'd recursively free all levels
    for (int i = 0; i < NPML4ENTRIES / 2; i++) {  // Only lower half is user space
        if (!(pml4[i] & PTE_P))
            continue;
        if (pml4[i] & PTE_PS)  // Huge page, don't free
            continue;

        pte_t *pdpt = (pte_t*)PTE_ADDR(pml4[i]);
        for (int j = 0; j < NPDPTENTRIES; j++) {
            if (!(pdpt[j] & PTE_P))
                continue;
            if (pdpt[j] & PTE_PS)
                continue;

            pte_t *pd = (pte_t*)PTE_ADDR(pdpt[j]);
            for (int k = 0; k < NPDENTRIES; k++) {
                if (!(pd[k] & PTE_P))
                    continue;
                if (pd[k] & PTE_PS)
                    continue;

                kfree((char*)PTE_ADDR(pd[k]));
            }
            kfree((char*)pd);
        }
        kfree((char*)pdpt);
    }
    kfree((char*)pml4);
}

// Load data from src to virtual address va in a given page directory.
int loaduvm(pte_t *pml4, char *addr, char *src, uint64_t sz) {
    uint64_t i, n;
    pte_t *pte;
    char *pa;

    if ((uint64_t)addr % PGSIZE != 0)
        return -1;

    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walkpgdir(pml4, addr + i, 0)) == 0)
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
    cpu.ts.rsp0 = (uint64_t)p->kstack + KSTACKSIZE;

    // Switch to process's page directory
    lcr3((uint64_t)p->pgdir);
}
