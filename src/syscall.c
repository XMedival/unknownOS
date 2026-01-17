#include <syscall.h>
#include <types.h>
#include <proc.h>
#include <x86.h>
#include <EGA.h>
#include <acpi.h>
#include <log.h>

extern struct cpu cpu;

// Fetch syscall argument from trapframe registers
// Linux x86 convention: syscall number in eax, args in ebx, ecx, edx, esi, edi
static int argint(int n, int *ip) {
    struct trapframe *tf = cpu.proc->tf;
    switch (n) {
    case 0: *ip = tf->ebx; break;
    case 1: *ip = tf->ecx; break;
    case 2: *ip = tf->edx; break;
    case 3: *ip = tf->esi; break;
    case 4: *ip = tf->edi; break;
    default: return -1;
    }
    return 0;
}

// sys_exit - terminate the current process
static void sys_exit(void) {
    int status;
    argint(0, &status);
    exit(status);
    // exit() never returns
}

static void sys_shutdown(void) {
    acpi_shutdown();
}

// sys_write - write to console (minimal implementation for testing)
static int sys_write(void) {
    int fd, n;
    uint buf;

    argint(0, &fd);
    argint(1, (int*)&buf);
    argint(2, &n);

    if (fd != 1 && fd != 2) {
        // Only support stdout/stderr for now
        return -1;
    }

    // Simple character-by-character output
    // NOTE: This is unsafe - should validate user pointer
    char *p = (char*)buf;
    for (int i = 0; i < n; i++) {
        putchar(p[i]);
    }

    return n;
}

// Syscall dispatcher - called from trap handler
void syscall(void) {
    int num;

    if (cpu.proc == 0) {
        LOG_WARN("syscall: no process");
        return;
    }

    num = cpu.proc->tf->eax;

    switch (num) {
    case SYS_exit:
        sys_exit();
        break;
    case SYS_write:
        cpu.proc->tf->eax = sys_write();
        break;
    case SYS_shutdown:
            sys_shutdown();
    default:
        LOG_WARN("Unknown syscall %d from pid %d", num, cpu.proc->pid);
        cpu.proc->tf->eax = -1;
    }
}
