#include <types.h>
#include <x86.h>
#include <serial.h>
#include <string.h>

extern uint xpos;
extern uint ypos;
extern volatile uchar *video;

void cls (void) {
  int i;

  video = (unsigned char *) FRAMEBUFFER_ADDR;
  
  for (i = 0; i < COLUMNS * LINES * 2; i++)
    *(video + i) = 0;

  xpos = 0;
  ypos = 0;
}

void itoa (char *buf, int base, int d) {
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

void putchar (int c) {
  if (c == '\n')
    {
    newline:
      serial_putc('\r');
      serial_putc('\n');
      xpos = 0;
      ypos++;
      if (ypos >= LINES)
        ypos = 0;
      return;
    }
  if (c == '\r')
    {
    carriage_ret:
      serial_putc(c);
      xpos = 0;
      return;
    }

  *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
  *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;
  serial_putc(c);

  xpos++;
  if (xpos >= COLUMNS)
    goto newline;
}

static void ftoa(char *out, double x, int prec) {
  if (x < 0) { *out++ = '-'; x = -x; }

  int ip = (int)x;
  double frac = x - (double)ip;

  char tmp[20];
  itoa(tmp, 'd', ip);

  char *t = tmp;
  while (*t) *out++ = *t++;

  *out++ = '.';

  for (int i = 0; i < prec; i++) {
    frac *= 10.0;
    int d = (int)frac;
    *out++ = (char)('0' + d);
    frac -= (double)d;
  }

  *out = 0;
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

            case 'f':
              ftoa(buf, *((double *)arg), 6);
              arg += 2;
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
