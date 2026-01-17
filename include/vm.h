#pragma once
#include <types.h>

#define KSTACKSIZE 4096  // Size of per-process kernel stack

extern pde *kpgdir;

struct proc;  // Forward declaration

void kvmalloc(void);
void switchkvm(void);

// User page table functions
pde* setupuvm(void);
int allocuvm(pde *pgdir, uint oldsz, uint newsz);
int deallocuvm(pde *pgdir, uint oldsz, uint newsz);
void freevm(pde *pgdir);
int loaduvm(pde *pgdir, char *addr, char *src, uint sz);
void switchuvm(struct proc *p);
