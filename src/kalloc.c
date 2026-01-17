#include <kalloc.h>
#include <stdio.h>
#include <types.h>
#include <mmu.h>

extern char end[], start[];
extern char *mbi;
extern uint mbi_size;
extern uint init_module_start;
extern uint init_module_end;

struct run {
    struct run *next;
};

struct {
    struct run *freelist;
	int nfree;
} kmem ;

static inline int in_kernel(void* v) {
  return (v >= (void*)start) && (v < (void*)end);
}

static inline int in_mbi(void* v) {
  uint a = (uint)v;
  uint m0 = (uint)mbi;
  uint m1 = m0 + mbi_size;
  return (a >= m0) && (a < m1);
}

static inline int in_module(void* v) {
  if (init_module_start == 0) return 0;
  uint a = (uint)v;
  // Protect the entire module region (page-aligned)
  uint m0 = init_module_start & ~(PGSIZE - 1);
  uint m1 = (init_module_end + PGSIZE - 1) & ~(PGSIZE - 1);
  return (a >= m0) && (a < m1);
}

void kfree(char *v) {
	struct run *r;

	if((uint)v % PGSIZE != 0) return;
  if(in_kernel(v)) return;
  if(in_mbi(v)) return;
  if(in_module(v)) return;

	uint eflags;
	asm volatile("pushfl; popl %0" : "=r"(eflags));
	asm volatile("cli");
	r = (struct run*)v;
	r->next = kmem.freelist;
	kmem.freelist = r;
	kmem.nfree++;
	if(eflags & 0x200)
		asm volatile("sti");
}

void freerange(void *start, void *end) {
     char *p, *e;
	 p = (char*)PGROUNDUP((uint)start);
   e = (char*)PGROUNDDOWN((uint)end);
	 for(; p + PGSIZE <= e; p += PGSIZE)
		kfree(p);
}

char *kalloc(void) {
  struct run *r;

  uint eflags;
  asm volatile("pushfl; popl %0" : "=r"(eflags));
  asm volatile("cli");
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    kmem.nfree--;
  }
  if(eflags & 0x200)
    asm volatile("sti");

  return (char*)r;
}

int freemem(void) {
  return kmem.nfree;
}
