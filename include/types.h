#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t, ptrdiff_t, NULL

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

// 64-bit page table entry types
typedef uint64_t pte_t;
typedef uint64_t pde_t;

// Use stdint types - don't redefine size_t, ssize_t, etc.
// They're provided by stddef.h/stdint.h
typedef int64_t ssize_t;
typedef uintptr_t vaddr_t;  // Virtual address
typedef uintptr_t paddr_t;  // Physical address
