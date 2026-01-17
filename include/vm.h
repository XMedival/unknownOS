#pragma once
#include <types.h>

#define KSTACKSIZE 4096  // Size of per-process kernel stack

// In 64-bit mode, the page table is a PML4 (array of pte_t)
extern pte_t *kpgdir;

struct proc;  // Forward declaration

void kvmalloc(void);
void switchkvm(void);

// User page table functions (64-bit 4-level paging)
pte_t* setupuvm(void);
uint64_t allocuvm(pte_t *pml4, uint64_t oldsz, uint64_t newsz);
uint64_t deallocuvm(pte_t *pml4, uint64_t oldsz, uint64_t newsz);
void freevm(pte_t *pml4);
int loaduvm(pte_t *pml4, char *addr, char *src, uint64_t sz);
void switchuvm(struct proc *p);
