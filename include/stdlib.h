#pragma once
#include <types.h>

void abort(void) __attribute__((noreturn));
void exit(int status) __attribute__((noreturn));

extern int *__errno_location(void);
#define errno (*__errno_location())
