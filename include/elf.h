#pragma once
#include <types.h>

// ELF Magic
#define ELF_MAGIC 0x464C457F  // "\x7FELF" in little endian

// ELF32 Header (52 bytes)
struct elfhdr {
    uint   magic;         // Must equal ELF_MAGIC
    uchar  elf[12];       // e_ident[4..15]
    ushort type;          // ET_EXEC=2 (executable)
    ushort machine;       // EM_386=3 (i386)
    uint   version;
    uint   entry;         // Entry point virtual address
    uint   phoff;         // Program header table offset
    uint   shoff;         // Section header table offset
    uint   flags;
    ushort ehsize;        // ELF header size
    ushort phentsize;     // Program header entry size
    ushort phnum;         // Number of program headers
    ushort shentsize;
    ushort shnum;
    ushort shstrndx;
};

// Program Header (32 bytes)
struct proghdr {
    uint type;            // PT_LOAD=1, PT_NULL=0
    uint off;             // Offset in file
    uint vaddr;           // Virtual address
    uint paddr;           // Physical address (usually same as vaddr)
    uint filesz;          // Size in file
    uint memsz;           // Size in memory (>= filesz, difference is BSS)
    uint flags;           // PF_X=1, PF_W=2, PF_R=4
    uint align;           // Alignment
};

// ELF types
#define ET_NONE  0
#define ET_REL   1
#define ET_EXEC  2
#define ET_DYN   3

// Machine types
#define EM_386   3

// Program header types
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3

// Program header flags
#define PF_X 0x1  // Executable
#define PF_W 0x2  // Writable
#define PF_R 0x4  // Readable

// Validate ELF header, returns 0 on success, -1 on failure
int elf_check(struct elfhdr *elf);
