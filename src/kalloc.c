#include <kalloc.h>
#include <types.h>
#include <mmu.h>

extern uint end;
extern uint start;

struct run {
    struct run *next;
};

struct {
    struct run *freelist;
	int nfree;
} kmem ;


void kfree(char *v) {
	struct run *r;
	if((uint)v % PGSIZE || (v < (char*)end && v > (char*)start)) return;

	uint eflags;
	asm volatile("pushfl; popl %0" : "=r"(eflags));
	asm volatile("cli");
	r = kmem.freelist;
	kmem.freelist = r;
	kmem.nfree++;
	if(eflags & 0x200)
		asm volatile("sti");
}

void freerange(void *start, void *end) {
     char *p;
	 p = (char*)PGROUNDUP((uint)start);
	 for(; p + PGSIZE <= (char*)end; p += PGSIZE)
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
