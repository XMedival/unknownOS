#include <multiboot2.h>
#include <x86.h>
#include <types.h>
#include <string.h>
#include <serial.h>
#include <kalloc.h>

#define CHECK_FLAG(flags, bit)) ((flags) & (1 << (bit)))

struct multiboot_info {
  uint total_size;
  uint reserved;
  struct multiboot_tag tags[0];
}__attribute__((aligned(MULTIBOOT_INFO_ALIGN)));


#define MULTIBOOT_HEADER_LENGTH (uint)sizeof(header)

__attribute__((section(".multiboot2_header"), used,  aligned(MULTIBOOT_HEADER_ALIGN)))
const struct {
  struct multiboot_header header;
  struct multiboot_header_tag_console_flags cflags __attribute__((aligned(MULTIBOOT_HEADER_ALIGN)));
  struct multiboot_header_tag end_tag __attribute__((aligned(MULTIBOOT_HEADER_ALIGN)));
} header = {
  .header = {
    .magic = MULTIBOOT2_HEADER_MAGIC,
    .architecture = MULTIBOOT_ARCHITECTURE_I386,
    .header_length = MULTIBOOT_HEADER_LENGTH,
    .checksum = 0 - (MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + sizeof(header)),
  },
  .cflags = {
    .type = MULTIBOOT_HEADER_TAG_CONSOLE_FLAGS,
    .flags = 0,
    .size = sizeof(struct multiboot_header_tag_console_flags),
    .console_flags = 3,
  },
  .end_tag = {
    .type = MULTIBOOT_TAG_TYPE_END,
    .flags = 0,
    .size = sizeof(struct multiboot_header_tag),
  },
};

uint xpos;
uint ypos;
volatile uchar *video;
uint start;
extern char end[];
struct multiboot_info *mbi;

// ------------------------------------------------------------

void _start() {
    unsigned long magic;
    unsigned long addr;
    asm("mov %%eax,%0" : "=r"(magic));
    asm("mov %%ebx,%0" : "=r"(addr));
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        printf("Invalid magic number: 0x%x\n", (unsigned) magic);
        hlt();
    }
    mbi = (struct multiboot_info *) addr;
    serial_init();
    cls();
    printf("\nUnknownOS --- The OS nobody knows about\n");
    printf("=======================================\n");
    for (int i = 0;;) {
      uint type = mbi->tags[i].type;
      uint size = mbi->tags[i].size;
      if (type == 0) {
        break;
      } else if (type == 1) {
        struct multiboot_tag_string *cmdline = (struct multiboot_tag_string*)&mbi->tags[i];
        if (!(*cmdline->string == 0)) printf("%s\n", cmdline->string);
      } else if (type == 2) {
        struct multiboot_tag_string *boot_name = (struct multiboot_tag_string*)&mbi->tags[i];
        printf("Bootloader Name: %s\n", boot_name->string);
      } else if (type == 6) {
        struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap*)&mbi->tags[i];
        uchar *p   = (uchar*)mmap_tag->entries;
        uchar *end = (uchar*)mmap_tag + mmap_tag->size;

        while (p < end) {
            struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
            if (e->type == 1) 
              freerange((void*)e->addr, (void*)(e->addr + e->len));
            p += mmap_tag->entry_size;
        }
        ulong available_mem = freemem();
        if (available_mem != 0) printf("Available Memory: %d KB\n", (available_mem * 4096) / 1024);
      } else if (type == 21) {
        struct multiboot_tag_load_base_addr *load_addr = (struct multiboot_tag_load_base_addr*)&mbi->tags[i];
        start = load_addr->load_base_addr;
      }
      i++;
      i+=size/sizeof(struct multiboot_tag);
    }
    hlt();
}
