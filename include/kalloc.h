#pragma once
#include <types.h>

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
#define PGSIZE 4096

void kfree(char *v);
void freerange(void *start, void *end);
char *kalloc();
int freemem();
