#include <proc.h>
#include <types.h>
#include <kalloc.h>
#include <mmu.h>
#include <vm.h>
#include <elf.h>
#include <string.h>
#include <x86.h>
#include <log.h>

#define NPROC 64

// Process table
static struct proc ptable[NPROC];
static int nextpid = 1;

// External CPU structure from gdt.c
extern struct cpu cpu;

// External assembly functions
extern void forkret(void);
extern void trapret(void);
extern void swtch(struct context **old, struct context *new);

// Allocate a new process structure and initialize it
struct proc* allocproc(void) {
    struct proc *p;
    char *sp;

    // Find an unused slot in the process table
    for (p = ptable; p < &ptable[NPROC]; p++) {
        if (p->state == UNUSED)
            goto found;
    }
    return 0;

found:
    p->state = EMBRYO;
    p->pid = nextpid++;

    // Allocate kernel stack
    if ((p->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame at top of kernel stack
    sp -= sizeof(struct trapframe);
    p->tf = (struct trapframe*)sp;

    // Set up new context to start at forkret, which returns to trapret
    sp -= sizeof(struct context);
    p->context = (struct context*)sp;
    memset(p->context, 0, sizeof(struct context));
    p->context->rip = (uint64_t)forkret;

    return p;
}

// Load an ELF64 binary and create a new process
int exec(char *binary, uint size) {
    struct elfhdr *elf;
    struct proghdr *ph, *eph;
    pte_t *pgdir = 0;
    struct proc *p;
    uint64_t sz = 0;
    uint64_t ustack;

    // Validate ELF header
    elf = (struct elfhdr*)binary;
    if (elf_check(elf) < 0) {
        LOG_WARN("exec: invalid ELF (not 64-bit or wrong format)");
        return -1;
    }

    // Allocate process structure
    if ((p = allocproc()) == 0) {
        LOG_WARN("exec: no free proc");
        return -1;
    }

    // Create page directory (uses identity mapping for now)
    if ((pgdir = setupuvm()) == 0) {
        LOG_WARN("exec: setupuvm failed");
        goto bad;
    }

    // Load program segments
    ph = (struct proghdr*)((char*)binary + elf->phoff);
    eph = ph + elf->phnum;

    for (; ph < eph; ph++) {
        if (ph->type != PT_LOAD)
            continue;
        if (ph->memsz < ph->filesz) {
            LOG_WARN("exec: memsz < filesz");
            goto bad;
        }
        if (ph->vaddr + ph->memsz < ph->vaddr) {
            LOG_WARN("exec: vaddr overflow");
            goto bad;
        }

        // Allocate memory for this segment
        if ((sz = allocuvm(pgdir, sz, ph->vaddr + ph->memsz)) == 0) {
            LOG_WARN("exec: allocuvm failed");
            goto bad;
        }

        // Ensure vaddr is page-aligned for loading
        if (ph->vaddr % PGSIZE != 0) {
            LOG_WARN("exec: vaddr not aligned");
            goto bad;
        }

        // Load segment data
        if (loaduvm(pgdir, (char*)ph->vaddr, binary + ph->off, ph->filesz) < 0) {
            LOG_WARN("exec: loaduvm failed");
            goto bad;
        }
    }

    // Allocate user stack (2 pages: one guard, one usable)
    sz = PGROUNDUP(sz);
    if ((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0) {
        LOG_WARN("exec: stack allocuvm failed");
        goto bad;
    }

    ustack = sz;  // Stack grows down from here

    // Set up trapframe for return to user mode (64-bit)
    memset(p->tf, 0, sizeof(*p->tf));
    p->tf->cs = (SEG_UCODE64 << 3) | DPL_USER;  // 64-bit user code segment
    p->tf->ss = (SEG_UDATA << 3) | DPL_USER;
    p->tf->rflags = FL_IF;  // Enable interrupts in user mode
    p->tf->rsp = ustack;
    p->tf->rip = elf->entry;

    // Commit to the process
    p->pgdir = pgdir;
    p->sz = sz;
    p->state = RUNNABLE;
    safestrcpy(p->name, "init", sizeof(p->name));

    LOG_INFO("exec: loaded ELF64, entry=0x%lx, size=%ld, pid=%d",
             elf->entry, sz, p->pid);

    return p->pid;

bad:
    if (pgdir)
        freevm(pgdir);
    if (p->kstack)
        kfree(p->kstack);
    p->kstack = 0;
    p->state = UNUSED;
    return -1;
}

// Exit the current process
void exit(int status) {
    struct proc *p = cpu.proc;

    if (p == 0) {
        LOG_WARN("exit: no current process");
        return;
    }

    LOG_INFO("Process %d exited with status %d", p->pid, status);

    // Mark as zombie
    p->state = ZOMBIE;

    // Jump back to scheduler (never returns)
    swtch(&p->context, cpu.scheduler);

    // Should never get here
    LOG_FAIL("zombie exit");
}

// Simple round-robin scheduler
void scheduler(void) {
    struct proc *p;

    LOG_INFO("Scheduler started");

    for (;;) {
        // Enable interrupts
        sti();

        // Loop over process table looking for RUNNABLE
        for (p = ptable; p < &ptable[NPROC]; p++) {
            if (p->state != RUNNABLE)
                continue;

            // Switch to this process
            cpu.proc = p;
            p->state = RUNNING;

            // Switch page table and update TSS
            switchuvm(p);

            // Context switch to process
            swtch(&cpu.scheduler, p->context);

            // Process returned - switch back to kernel page table
            switchkvm();
            cpu.proc = 0;

            // If process is zombie, clean it up
            if (p->state == ZOMBIE) {
                LOG_INFO("Cleaning up zombie pid %d", p->pid);
                kfree(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                p->pgdir = 0;
                p->state = UNUSED;
                p->pid = 0;
                p->name[0] = 0;
            }
        }
    }
}
