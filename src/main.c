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
#include <kb.h>
#include <input.h>
#include <ata.h>
#include <vfs.h>

// Display mode (set via Makefile)
#ifndef FB_FORCE_MODE
#define FB_FORCE_MODE 0
#endif

#define CHECK_FLAG(flags, bit)) ((flags) & (1 << (bit)))

struct multiboot_info {
  uint total_size;
  uint reserved;
  struct multiboot_tag tags[0];
}__attribute__((aligned(MULTIBOOT_INFO_ALIGN)));


// Multiboot2 header with framebuffer request (0,0,0 = let GRUB choose)
#define MB_HEADER_SIZE 48

__attribute__((section(".multiboot2_header"), used, aligned(MULTIBOOT_HEADER_ALIGN)))
const struct {
  struct multiboot_header header;
  struct multiboot_header_tag_framebuffer fbtag __attribute__((aligned(MULTIBOOT_HEADER_ALIGN)));
  struct multiboot_header_tag end_tag __attribute__((aligned(MULTIBOOT_HEADER_ALIGN)));
} header = {
  .header = {
    .magic = MULTIBOOT2_HEADER_MAGIC,
    .architecture = MULTIBOOT_ARCHITECTURE_I386,
    .header_length = MB_HEADER_SIZE,
    .checksum = (uint32_t)(-(int32_t)(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + MB_HEADER_SIZE)),
  },
  .fbtag = {
      .type = MULTIBOOT_HEADER_TAG_FRAMEBUFFER,
      .flags = 0,
      .size = sizeof(struct multiboot_header_tag_framebuffer),
      .width = 0,
      .height = 0,
      .depth = 0,
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
uintptr_t init_module_start = 0;
uintptr_t init_module_end = 0;

static void parse_multiboot2_info(void);
static const char *get_bootloader_name(void);

// ------------------------------------------------------------

// PC speaker beep as early debug signal
void beep(void) {
    // Set up PIT channel 2 for ~1000 Hz tone
    outb(0x43, 0xB6);           // Channel 2, lobyte/hibyte, square wave
    outb(0x42, 0x97);           // Divisor low byte (1193180 / 1000 ≈ 1193 = 0x04A9)
    outb(0x42, 0x04);           // Divisor high byte

    // Enable speaker
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp | 0x03);     // Enable speaker + PIT gate

    // Delay (beep duration)
    for (volatile int i = 0; i < 10000000; i++);

    // Disable speaker
    outb(0x61, tmp & 0xFC);
}

void _start(unsigned long magic, struct multiboot_info *info) {
    mbi = info;
    ASSERT(mbi);
    mbi_size = mbi->total_size;

    // Verify multiboot2 magic
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        for(;;) asm("hlt");
    }

    serial_init();
    gdt_init();
    idt_init();

    input_init();
    kb_init();

    fb_init(mbi);

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
    LOG_OK("keyboard (PS/2, IRQ1)");

    parse_multiboot2_info();

    kvmalloc();
    LOG_OK("paging (4MB pages, identity mapped)");

    const char *bootloader = get_bootloader_name();
    if (bootloader) {
        LOG_INFO("booted via %s", bootloader);
    }

    pci_init();

    acpi_init();

    ata_init();

    vfs_init();

    printf("\n");
    printf("  _   _       _                              ___  ____  \n");
    printf(" | | | |_ __ | | ___ __   _____      ___ __ / _ \\/ ___| \n");
    printf(" | | | | '_ \\| |/ / '_ \\ / _ \\ \\ /\\ / / '_  | | | \\___ \\ \n");
    printf(" | |_| | | | |   <| | | | (_) \\ V  V /| | | | |_| |___) |\n");
    printf("  \\___/|_| |_|_|\\_\\_| |_|\\___/ \\_/\\_/ |_| |_|\\___/|____/ \n");
    printf("\n");
    printf("\n");
    printf("  _   _       _                              ___  ____  \n");
    printf(" | | | |_ __ | | ___ __   _____      ___ __ / _ \\/ ___| \n");
    printf(" | | | | '_ \\| |/ / '_ \\ / _ \\ \\ /\\ / / '_  | | | \\___ \\ \n");
    printf(" | |_| | | | |   <| | | | (_) \\ V  V /| | | | |_| |___) |\n");
    printf("  \\___/|_| |_|_|\\_\\_| |_|\\___/ \\_/\\_/ |_| |_|\\___/|____/ \n");
    printf("\n");

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
    printf("(Use PageUp/PageDown to scroll)\n");

    // Enable interrupts so keyboard works even without a running process
    sti();

    // Halt loop - wakes on interrupt, handles it, then halts again
    for (;;) {
        hlt();
    }
}

static void parse_multiboot2_info(void) {
    struct multiboot_tag *tag;

    // First pass: find modules (so we can protect their memory during freerange)
    tag = (struct multiboot_tag *)((uchar *)mbi + 8);
    while (tag->type != 0) {
        if (tag->type == MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR) {
            struct multiboot_tag_load_base_addr *load_addr = (struct multiboot_tag_load_base_addr*)tag;
            if (start != (char*)(uintptr_t)load_addr->load_base_addr) {
                LOG_WARN("Bootloader address not matching the linker start address");
            } else {
                LOG_INFO("Kernel Start label: 0x%x", start);
            }
        } else if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module *mod = (struct multiboot_tag_module*)tag;
            LOG_INFO("Module: %s (0x%x - 0x%x)", mod->cmdline, mod->mod_start, mod->mod_end);
            // Store first module as init
            if (init_module_start == 0) {
                init_module_start = mod->mod_start;
                init_module_end = mod->mod_end;
            }
        }
        // Move to next tag (8-byte aligned)
        tag = (struct multiboot_tag *)((uchar *)tag + ((tag->size + 7) & ~7));
    }

    // Second pass: process memory map (now that modules are registered for protection)
    tag = (struct multiboot_tag *)((uchar *)mbi + 8);
    while (tag->type != 0) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap*)tag;
            uchar *p   = (uchar*)mmap_tag->entries;
            uchar *endp = (uchar*)mmap_tag + mmap_tag->size;

            while (p < endp) {
                struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
                if (e->type == 1){
                    freerange((void*)(uintptr_t)e->addr, (void*)(uintptr_t)(e->addr + e->len));
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
        // Move to next tag (8-byte aligned)
        tag = (struct multiboot_tag *)((uchar *)tag + ((tag->size + 7) & ~7));
    }
}

static const char *get_bootloader_name(void) {
    struct multiboot_tag *tag = (struct multiboot_tag *)((uchar *)mbi + 8);
    while (tag->type != 0) {
        if (tag->type == MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME) {
            struct multiboot_tag_string *boot_name = (struct multiboot_tag_string*)tag;
            return boot_name->string;
        }
        tag = (struct multiboot_tag *)((uchar *)tag + ((tag->size + 7) & ~7));
    }
    return NULL;
}
