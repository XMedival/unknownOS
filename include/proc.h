#pragma once
#include <types.h>
#include <mmu.h>
#include <param.h>

// 64-bit CPU structure
struct cpu {
    uchar apicid;
    struct context *scheduler;
    struct taskstate ts;
    // GDT: 8 entries, but TSS takes 2 slots (16 bytes)
    // So we allocate as bytes to avoid alignment issues
    uint8_t gdt[NSEGS * 8];
    volatile uint started;
    int ncli;
    int intena;
    struct proc *proc;
};

// 64-bit context for context switching
// Callee-saved registers in System V AMD64 ABI: rbx, rbp, r12-r15
struct context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct proc {
    uint64_t sz;              // Process size (64-bit)
    pte_t *pgdir;             // Page table (PML4 in 64-bit)
    char *kstack;
    enum procstate state;
    int pid;
    struct proc *parent;
    struct trapframe *tf;
    struct context *context;
    void *chan;
    int killed;
    struct file *ofile[NOFILE];
    struct inode *cwd;
    char name[16];
    int vt;                   // Which VT this process is attached to
};

// Process management functions
struct proc* allocproc(void);
int exec(char *binary, uint size);
void exit(int status);
void scheduler(void);
