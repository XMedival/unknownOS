#include <multiboot2.h>
#include <x86.h>
#include <types.h>
#include <serial.h>
#include <kalloc.h>
#include <EGA.h>
#include <idt.h>
#include <gdt.h>
#include <vm.h>
#include <log.h>
#include <assert.h>

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
extern char start[];
extern char end[];
struct multiboot_info *mbi;
uint mbi_size;

static void init_memory(void);
static const char *get_bootloader_name(void);

// ------------------------------------------------------------

void _start() {
    unsigned long magic;
    unsigned long addr;
    asm("mov %%eax,%0" : "=r"(magic));
    asm("mov %%ebx,%0" : "=r"(addr));

    mbi = (struct multiboot_info *) addr;
    ASSERT(mbi);
    mbi_size = mbi->total_size;

    // Early init (no logging yet)
    serial_init();
    cls();

    // Banner
    printf("\n");
    printf("  _   _       _                              ___  ____  \n");
    printf(" | | | |_ __ | | ___ __   _____      ___ __ / _ \\/ ___| \n");
    printf(" | | | | '_ \\| |/ / '_ \\ / _ \\ \\ /\\ / / '_  | | | \\___ \\ \n");
    printf(" | |_| | | | |   <| | | | (_) \\ V  V /| | | | |_| |___) |\n");
    printf("  \\___/|_| |_|_|\\_\\_| |_|\\___/ \\_/\\_/ |_| |_|\\___/|____/ \n");
    printf("\n");

    // Validate multiboot
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        LOG_FAIL("Invalid multiboot2 magic: 0x%x", (unsigned)magic);
    }

    // System initialization
    printf("--- System Initialization ---\n\n");

    LOG_OK("serial (COM1 @ 0x3F8)");

    gdt_init();
    LOG_OK("gdt set up");

    idt_init();
    LOG_OK("idt (48 vectors + syscall)");

    init_memory();

    kvmalloc();
    LOG_OK("paging (4MB pages, identity mapped)");

    const char *bootloader = get_bootloader_name();
    if (bootloader) {
        LOG_INFO("booted via %s", bootloader);
    }

    printf("\n--- System Halted ---\n");
    hlt();
}

static void init_memory(void) {
    for (int i = 0;;) {
        uint type = mbi->tags[i].type;
        uint size = mbi->tags[i].size;

        if (type == 0) {
            break;
        } else if (type == 6) {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap*)&mbi->tags[i];
            uchar *p   = (uchar*)mmap_tag->entries;
            uchar *endp = (uchar*)mmap_tag + mmap_tag->size;

            while (p < endp) {
                struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
                if (e->type == 1){
                    freerange((void*)(uint)e->addr, (void*)(uint)(e->addr + e->len));
                }
                p += mmap_tag->entry_size;
            }
        } else if (type == 21) {
            struct multiboot_tag_load_base_addr *load_addr = (struct multiboot_tag_load_base_addr*)&mbi->tags[i];
            if (start != (char*)load_addr->load_base_addr) {
                LOG_WARN("Grub address not matching the linker start address");
            } else {
                LOG_INFO("Kernel Start label: 0x%x", start);
            }
        }

        i++;
        i += size / sizeof(struct multiboot_tag);
    }

    uint total_kb = (freemem() * 4096) / 1024;
    uint total_mb = total_kb / 1024;

    if (total_mb > 0) {
        LOG_OK("memory (%d MB available)", total_mb);
    } else {
        LOG_OK("memory (%d KB available)", total_kb);
    }
}

static const char *get_bootloader_name(void) {
    for (int i = 0;;) {
        uint type = mbi->tags[i].type;
        uint size = mbi->tags[i].size;

        if (type == 0) {
            break;
        } else if (type == 2) {
            struct multiboot_tag_string *boot_name = (struct multiboot_tag_string*)&mbi->tags[i];
            return boot_name->string;
        }

        i++;
        i += size / sizeof(struct multiboot_tag);
    }
    return NULL;
}
