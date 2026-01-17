#include <multiboot2.h>
#include <x86.h>
#include <types.h>
#include <serial.h>
#include <kalloc.h>
#include <EGA.h>
#include <fb.h>
#include <idt.h>
#include <gdt.h>
#include <vm.h>
#include <log.h>
#include <assert.h>
#include <pci.h>
#include <acpi.h>
#include <proc.h>

#define CHECK_FLAG(flags, bit)) ((flags) & (1 << (bit)))

struct multiboot_info {
  uint total_size;
  uint reserved;
  struct multiboot_tag tags[0];
}__attribute__((aligned(MULTIBOOT_INFO_ALIGN)));


#define MULTIBOOT_HEADER_LENGTH (uint)sizeof(header)

// Custom info request struct with actual requests
struct inforeq_with_tags {
  multiboot_uint16_t type;
  multiboot_uint16_t flags;
  multiboot_uint32_t size;
  multiboot_uint32_t requests[7];  // Expanded for framebuffer
} __attribute__((packed));

__attribute__((section(".multiboot2_header"), used,  aligned(MULTIBOOT_HEADER_ALIGN)))
const struct {
  struct multiboot_header header;
  struct multiboot_header_tag_console_flags cflags __attribute__((aligned(MULTIBOOT_HEADER_ALIGN)));
  struct multiboot_header_tag_framebuffer fb_req __attribute__((aligned(MULTIBOOT_HEADER_ALIGN)));
  struct inforeq_with_tags infreq __attribute__((aligned(MULTIBOOT_HEADER_ALIGN)));
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
    .flags = MULTIBOOT_HEADER_TAG_OPTIONAL,
    .size = sizeof(struct multiboot_header_tag_console_flags),
    .console_flags = 0,
  },
  .fb_req = {
    .type = MULTIBOOT_HEADER_TAG_FRAMEBUFFER,
    .flags = MULTIBOOT_HEADER_TAG_OPTIONAL,  // Optional for fallback to text mode
    .size = sizeof(struct multiboot_header_tag_framebuffer),
    .width = 1024,   // Preferred width
    .height = 768,   // Preferred height
    .depth = 32,     // Preferred depth
  },
  .infreq = {
      .type = MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST,
      .flags = 0,
      .size = sizeof(struct inforeq_with_tags),
      .requests = {
          MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME,  // type 2
          MULTIBOOT_TAG_TYPE_MMAP,              // type 6
          MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR,    // type 21
          MULTIBOOT_TAG_TYPE_ACPI_OLD,          // type 14
          MULTIBOOT_TAG_TYPE_ACPI_NEW,          // type 15
          MULTIBOOT_TAG_TYPE_MODULE,            // type 3
          MULTIBOOT_TAG_TYPE_FRAMEBUFFER,       // type 8
      },
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

// Module info (for init binary) - exported for kalloc to protect
uint init_module_start = 0;
uint init_module_end = 0;

static void parse_multiboot2_info(void);
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

    // Early init - GDT/IDT first so we can see exceptions
    serial_init();
    gdt_init();
    idt_init();

    fb_init(mbi);  // Initialize framebuffer (or fall back to VGA text mode)

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
    LOG_OK("gdt set up");
    LOG_OK("idt (48 vectors + syscall)");

    parse_multiboot2_info();

    kvmalloc();
    LOG_OK("paging (4MB pages, identity mapped)");

    const char *bootloader = get_bootloader_name();
    if (bootloader) {
        LOG_INFO("booted via %s", bootloader);
    }

    pci_init();

    acpi_init();

    printf("\n--- Running User Process ---\n");

    // Try to load init from multiboot module
    if (init_module_start != 0) {
        uint module_size = init_module_end - init_module_start;
        int pid = exec((char*)init_module_start, module_size);
        if (pid > 0) {
            LOG_OK("Loaded init process (pid %d)", pid);
            scheduler();  // Never returns
        } else {
            LOG_FAIL("Failed to load init module");
        }
    } else {
        LOG_WARN("No init module provided");
    }

    printf("\n--- System Halted ---\n");
    hlt();
}

static void parse_multiboot2_info(void) {
    // First pass: find modules (so we can protect their memory during freerange)
    for (int i = 0;;) {
        uint type = mbi->tags[i].type;
        uint size = mbi->tags[i].size;

        if (type == 0) {
            break;
        } else if (type == MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR) {
            struct multiboot_tag_load_base_addr *load_addr = (struct multiboot_tag_load_base_addr*)&mbi->tags[i];
            if (start != (char*)load_addr->load_base_addr) {
                LOG_WARN("Grub address not matching the linker start address");
            } else {
                LOG_INFO("Kernel Start label: 0x%x", start);
            }
        } else if (type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *mod = (struct multiboot_tag_module*)&mbi->tags[i];
            LOG_INFO("Module: %s (0x%x - 0x%x)", mod->cmdline, mod->mod_start, mod->mod_end);
            // Store first module as init
            if (init_module_start == 0) {
                init_module_start = mod->mod_start;
                init_module_end = mod->mod_end;
            }
        }

        i++;
        i += size / sizeof(struct multiboot_tag);
    }

    // Second pass: process memory map (now that modules are registered for protection)
    for (int i = 0;;) {
        uint type = mbi->tags[i].type;
        uint size = mbi->tags[i].size;

        if (type == 0) {
            break;
        } else if (type == MULTIBOOT_TAG_TYPE_MMAP) {
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

            uint total_kb = (freemem() * 4096) / 1024;
            uint total_mb = total_kb / 1024;

            if (total_mb > 0) {
                LOG_OK("memory (%d MB available)", total_mb);
            } else {
                LOG_OK("memory (%d KB available)", total_kb);
            }
        }

        i++;
        i += size / sizeof(struct multiboot_tag);
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
