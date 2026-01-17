#include <syscall.h>
#include <types.h>
#include <proc.h>
#include <x86.h>
#include <EGA.h>
#include <acpi.h>
#include <log.h>

extern struct cpu cpu;

// Fetch syscall argument from trapframe registers
// System V AMD64 convention: syscall number in rax, args in rdi, rsi, rdx, r10, r8, r9
static int argint(int n, int64_t *ip) {
    struct trapframe *tf = cpu.proc->tf;
    switch (n) {
    case 0: *ip = tf->rdi; break;
    case 1: *ip = tf->rsi; break;
    case 2: *ip = tf->rdx; break;
    case 3: *ip = tf->r10; break;
    case 4: *ip = tf->r8; break;
    case 5: *ip = tf->r9; break;
    default: return -1;
    }
    return 0;
}

// sys_exit - terminate the current process
static void sys_exit(void) {
    int64_t status;
    argint(0, &status);
    exit((int)status);
    // exit() never returns
}

static void sys_shutdown(void) {
    acpi_shutdown();
}

// sys_write - write to console (minimal implementation for testing)
static int64_t sys_write(void) {
    int64_t fd, n;
    int64_t buf;

    argint(0, &fd);
    argint(1, &buf);
    argint(2, &n);

    if (fd != 1 && fd != 2) {
        // Only support stdout/stderr for now
        return -1;
    }

    // Simple character-by-character output
    // NOTE: This is unsafe - should validate user pointer
    char *p = (char*)buf;
    for (int64_t i = 0; i < n; i++) {
        putchar(p[i]);
    }

    return n;
}

// Syscall dispatcher - called from trap handler
void syscall(void) {
    int64_t num;

    if (cpu.proc == 0) {
        LOG_WARN("syscall: no process");
        return;
    }

    num = cpu.proc->tf->rax;

    switch (num) {
    case SYS_exit:
        sys_exit();
        break;
    case SYS_write:
        cpu.proc->tf->rax = sys_write();
        break;
    case SYS_shutdown:
        sys_shutdown();
        break;
    default:
        LOG_WARN("Unknown syscall %ld from pid %d", num, cpu.proc->pid);
        cpu.proc->tf->rax = -1;
    }
}
