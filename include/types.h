#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t, ptrdiff_t, NULL

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

// Short fixed-width type aliases
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

// 64-bit page table entry types
typedef uint64_t pte_t;
typedef uint64_t pde_t;

// Use stdint types - don't redefine size_t, ssize_t, etc.
// They're provided by stddef.h/stdint.h
typedef int64_t ssize_t;
typedef uintptr_t vaddr_t;  // Virtual address
typedef uintptr_t paddr_t;  // Physical address
