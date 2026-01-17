#include <elf.h>

// Validate ELF64 header
// Returns 0 on success, -1 on failure
int elf_check(struct elfhdr *elf) {
    if (!elf)
        return -1;
    if (elf->magic != ELF_MAGIC)
        return -1;
    // Check for 64-bit ELF (elf[0] is class)
    if (elf->elf[0] != ELFCLASS64)
        return -1;
    if (elf->type != ET_EXEC)
        return -1;
    if (elf->machine != EM_X86_64)
        return -1;
    if (elf->phentsize != sizeof(struct proghdr))
        return -1;
    return 0;
}
