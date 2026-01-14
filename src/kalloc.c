#include <kalloc.h>
#include <stdio.h>
#include <types.h>
#include <mmu.h>

extern char end[], start[];
extern char *mbi;
extern uint mbi_size;

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

void kfree(char *v) {
	struct run *r;

	if((uint)v % PGSIZE != 0) return;
  if(in_kernel(v)) return;
  if(in_mbi(v)) return;

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
