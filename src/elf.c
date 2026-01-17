#include <elf.h>

// Validate ELF header
// Returns 0 on success, -1 on failure
int elf_check(struct elfhdr *elf) {
    if (!elf)
        return -1;
    if (elf->magic != ELF_MAGIC)
        return -1;
    if (elf->type != ET_EXEC)
        return -1;
    if (elf->machine != EM_386)
        return -1;
    if (elf->phentsize != sizeof(struct proghdr))
        return -1;
    return 0;
}
