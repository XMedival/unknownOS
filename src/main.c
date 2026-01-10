#include "../multiboot.h"

#define CHECK_FLAG(flags, bit)) ((flags) & (1 << (bit)))

#define COLUMNS 80
#define LINES 24

#define ATTRIBUTE 7

#define MULTIBOOT_HEADER_FLAGS (MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE)

__attribute__((section(".multiboot_header")))
struct multiboot_header header = {
    MULTIBOOT_HEADER_MAGIC,
    MULTIBOOT_HEADER_FLAGS,
    -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS),
    0,
    0,
    0,
    0,
    0,
    1,
    COLUMNS,
    LINES,
    0,
}; 

static int xpos;
static int ypos;
static volatile unsigned char *video;
struct multiboot_info *mbi;

static void hlt();
static void cls();
static void itoa(char *buf, int base, int d);
static void putchar(int c);
void printf(const char *format, ...);

// ------------------------------------------------------------

void _start() {
    unsigned long magic;
    unsigned long addr;
    asm("mov %%eax,%0" : "=r"(magic));
    asm("mov %%ebx,%0" : "=r"(addr));
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("Invalid magic number: 0x%x\n", (unsigned) magic);
        hlt();
    }
    mbi = (struct multiboot_info *) addr;
    cls();
    printf("\nHSH --- The HardWare Shell\n");
    printf("================================\n");
    printf("Available Memory: %d MB\n", ((mbi->mem_upper - mbi->mem_lower) / 1024));
    printf("Boot Loader: %s\n", mbi->boot_loader_name);
    printf("================================\n");
    printf("framebuffer addr: 0x%x\n", mbi->framebuffer_addr);
    printf("framebuffer type: 0x%x\n", mbi->framebuffer_type);
    hlt();
}

// ------------------------------------------------------------

static void hlt() {
    asm volatile("cli\n" "hlt\n");
}

static void cls (void) {
  int i;

  video = (unsigned char *) mbi->framebuffer_addr;
  
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

static void putchar (int c) {
  if (c == '\n' || c == '\r')
    {
    newline:
      __asm__ volatile ("out %0, %w1" : : "a"('\r'), "Nd"(0x3F8));
      __asm__ volatile ("out %0, %w1" : : "a"(c), "Nd"(0x3F8));
      xpos = 0;
      ypos++;
      if (ypos >= LINES)
        ypos = 0;
      return;
    }

  *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
  *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;
  __asm__ volatile ("out %0, %w1" : : "a"(c), "Nd"(0x3F8));

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
