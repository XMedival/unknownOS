#include "../multiboot2.h"
#include <stdint.h>

#define CHECK_FLAG(flags, bit)) ((flags) & (1 << (bit)))

#define ATTRIBUTE 7

#define COLUMNS 80
#define LINES 24
#define FRAMEBUFFER_ADDR 0xB8000

typedef struct __attribute__((packed)) {
  uint32_t total_size;
  uint32_t reserved;
  struct multiboot_tag tags[0];
} multiboot_info;

typedef struct __attribute__((packed, aligned(MULTIBOOT_HEADER_ALIGN))) {
  struct multiboot_header header;
  struct multiboot_header_tag end_tag;
} header_t;

#define MULTIBOOT_HEADER_LENGTH (uint32_t)sizeof(header_t)

__attribute__((section(".multiboot2_header"), used, aligned(MULTIBOOT_HEADER_ALIGN)))
const header_t header = {
  .header = {
    .magic = MULTIBOOT2_HEADER_MAGIC,
    .architecture = MULTIBOOT_ARCHITECTURE_I386,
    .header_length = MULTIBOOT_HEADER_LENGTH,
    .checksum = -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + MULTIBOOT_HEADER_LENGTH),
  },
  .end_tag = {
    .type = MULTIBOOT_TAG_TYPE_END,
    .flags = 0,
    .size = sizeof(struct multiboot_header_tag),
  }
}; 

static int xpos;
static int ypos;
static volatile unsigned char *video;
multiboot_info *mbi;

static void hlt();
static void cls();
static void out(unsigned char val, int port);
static char in(int port);
static void itoa(char *buf, int base, int d);
static void putchar(int c);
void printf(const char *format, ...);

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
    mbi = (multiboot_info *) addr;
    cls();
    printf("\nHSH --- The HardWare Shell\n");
    printf("================================\n");
    for (int i = 0;;) {
      uint32_t type = mbi->tags[i].type;
      uint32_t size = mbi->tags[i].size;
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
        uintptr_t p   = (uintptr_t)mmap_tag->entries;
        uintptr_t end = (uintptr_t)mmap_tag + mmap_tag->size;

        int available_mem = 0;
        while (p < end) {
            struct multiboot_mmap_entry *e = (struct multiboot_mmap_entry *)p;
            if (e->type == 1) {
              available_mem+=e->len;
            }
            p += mmap_tag->entry_size; // advance by entry_size, not sizeof(struct)
        }
        printf("Available Memory: %d MB\n", (available_mem / 1024) / 1024);
      }
      i++;
      i+=size/sizeof(struct multiboot_tag);
    }
    hlt();
}

// ------------------------------------------------------------

static void hlt() {
    asm volatile("cli\n" "hlt\n");
}

static void cls (void) {
  int i;

  // video = (unsigned char *) mbi->framebuffer_addr;
  video = (unsigned char *) FRAMEBUFFER_ADDR;
  
  for (i = 0; i < COLUMNS * LINES * 2; i++)
    *(video + i) = 0;

  xpos = 0;
  ypos = 0;
}

static void itoa (char *buf, int base, int d) {
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
  
  /* If %d is specified and D is minus, put ‘-’ in the head. */
  if (base == 'd' && d < 0)
    {
      *p++ = '-';
      buf++;
      ud = -d;
    }
  else if (base == 'x')
    divisor = 16;

  /* Divide UD by DIVISOR until UD == 0. */
  do
    {
      int remainder = ud % divisor;
      
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
  while (ud /= divisor);

  /* Terminate BUF. */
  *p = 0;
  
  /* Reverse BUF. */
  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
    {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
    }
}

static void out(unsigned char val, int port) {
  asm volatile ("out %0, %w1" : : "a"(val), "Nd"(port));
}

static char in(int port) {
  char val;
  asm volatile ("in %w1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

static void putchar (int c) {
  if (c == '\n')
    {
    newline:
      out('\r', 0x3F8);
      out(c, 0x3F8);
      xpos = 0;
      ypos++;
      if (ypos >= LINES)
        ypos = 0;
      return;
    }
  if (c == '\r')
    {
    carriage_ret:
      out(c, 0x3F8);
      xpos = 0;
      return;
    }

  *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
  *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;
  out(c, 0x3F8);

  xpos++;
  if (xpos >= COLUMNS)
    goto newline;
}

void printf (const char *format, ...) {
  char **arg = (char **) &format;
  int c;
  char buf[20];

  arg++;
  
  while ((c = *format++) != 0)
    {
      if (c != '%')
        putchar (c);
      else
        {
          char *p, *p2;
          int pad0 = 0, pad = 0;
          
          c = *format++;
          if (c == '0')
            {
              pad0 = 1;
              c = *format++;
            }

          if (c >= '0' && c <= '9')
            {
              pad = c - '0';
              c = *format++;
            }

          switch (c)
            {
            case 'd':
            case 'u':
            case 'x':
              itoa (buf, c, *((int *) arg++));
              p = buf;
              goto string;
              break;

            case 's':
              p = *arg++;
              if (! p)
                p = "(null)";

            string:
              for (p2 = p; *p2; p2++);
              for (; p2 < p + pad; p2++)
                putchar (pad0 ? '0' : ' ');
              while (*p)
                putchar (*p++);
              break;

            default:
              putchar (*((int *) arg++));
              break;
            }
        }
    }
}
