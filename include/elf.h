#pragma once
#include <types.h>

// ELF Magic
#define ELF_MAGIC 0x464C457F  // "\x7FELF" in little endian

// ELF64 Header (64 bytes)
struct elfhdr {
    uint32_t magic;         // Must equal ELF_MAGIC
    uint8_t  elf[12];       // e_ident[4..15]: class, data, version, osabi, abiversion, pad
    uint16_t type;          // ET_EXEC=2 (executable)
    uint16_t machine;       // EM_X86_64=62
    uint32_t version;
    uint64_t entry;         // Entry point virtual address
    uint64_t phoff;         // Program header table offset
    uint64_t shoff;         // Section header table offset
    uint32_t flags;
    uint16_t ehsize;        // ELF header size
    uint16_t phentsize;     // Program header entry size
    uint16_t phnum;         // Number of program headers
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

// ELF64 Program Header (56 bytes)
struct proghdr {
    uint32_t type;          // PT_LOAD=1, PT_NULL=0
    uint32_t flags;         // PF_X=1, PF_W=2, PF_R=4
    uint64_t off;           // Offset in file
    uint64_t vaddr;         // Virtual address
    uint64_t paddr;         // Physical address (usually same as vaddr)
    uint64_t filesz;        // Size in file
    uint64_t memsz;         // Size in memory (>= filesz, difference is BSS)
    uint64_t align;         // Alignment
};

// ELF types
#define ET_NONE  0
#define ET_REL   1
#define ET_EXEC  2
#define ET_DYN   3

// Machine types
#define EM_386      3
#define EM_X86_64   62

// ELF class (32 or 64-bit)
#define ELFCLASS32  1
#define ELFCLASS64  2

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
