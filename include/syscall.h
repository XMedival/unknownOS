#pragma once

// Syscall numbers
#define SYS_exit    1
#define SYS_write   2
#define SYS_shutdown 3

// Syscall dispatcher
void syscall(void);
