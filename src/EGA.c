#include <x86.h>
#include <types.h>
#include <EGA.h>
#include <fb.h>
#include <serial.h>
#include <stdarg.h>

extern uint xpos;
extern uint ypos;
extern volatile uchar *video;

// cls now uses framebuffer driver
void cls(void) {
    fb_clear();
}

// Convert integer to string (supports both 32-bit and 64-bit values)
static void itoa64(char *buf, int base, uint64_t ud, int is_signed) {
    char *p = buf;
    char *p1, *p2;
    int divisor = 10;

    // Handle signed negative numbers
    if (is_signed && (int64_t)ud < 0) {
        *p++ = '-';
        buf++;
        ud = (uint64_t)(-(int64_t)ud);
    }

    if (base == 16)
        divisor = 16;

    // Divide UD by DIVISOR until UD == 0
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);

    // Terminate BUF
    *p = 0;

    // Reverse BUF
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

// Legacy 32-bit version for compatibility
void itoa(char *buf, int base, int d) {
    itoa64(buf, base == 'x' ? 16 : 10, (uint64_t)(unsigned int)d, base == 'd');
}

// putchar now uses framebuffer driver
void putchar(int c) {
    fb_putchar(c);
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

void printf(const char *format, ...) {
    va_list ap;
    int c;
    char buf[32];

    va_start(ap, format);

    while ((c = *format++) != 0) {
        if (c != '%') {
            putchar(c);
        } else {
            char *p, *p2;
            int pad0 = 0, pad = 0;
            int is_long = 0;

            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }

            while (c >= '0' && c <= '9') {
                pad = pad * 10 + (c - '0');
                c = *format++;
            }

            // Check for 'l' modifier (long)
            if (c == 'l') {
                is_long = 1;
                c = *format++;
                // Check for 'll' (long long) - treat same as 'l' in 64-bit
                if (c == 'l') {
                    c = *format++;
                }
            }

            switch (c) {
            case 'd':
            case 'i':
                if (is_long) {
                    int64_t val = va_arg(ap, int64_t);
                    itoa64(buf, 10, (uint64_t)val, 1);
                } else {
                    int val = va_arg(ap, int);
                    itoa64(buf, 10, (uint64_t)(unsigned int)val, 1);
                }
                p = buf;
                goto string;

            case 'u':
                if (is_long) {
                    uint64_t val = va_arg(ap, uint64_t);
                    itoa64(buf, 10, val, 0);
                } else {
                    unsigned int val = va_arg(ap, unsigned int);
                    itoa64(buf, 10, val, 0);
                }
                p = buf;
                goto string;

            case 'x':
            case 'X':
                if (is_long) {
                    uint64_t val = va_arg(ap, uint64_t);
                    itoa64(buf, 16, val, 0);
                } else {
                    unsigned int val = va_arg(ap, unsigned int);
                    itoa64(buf, 16, val, 0);
                }
                p = buf;
                goto string;

            case 'p':
                // Pointer - always 64-bit
                {
                    uint64_t val = (uint64_t)va_arg(ap, void*);
                    itoa64(buf, 16, val, 0);
                }
                p = buf;
                goto string;

            case 'f':
                ftoa(buf, va_arg(ap, double), 6);
                p = buf;
                goto string;

            case 's':
                p = va_arg(ap, char*);
                if (!p)
                    p = "(null)";

            string:
                for (p2 = p; *p2; p2++);
                for (; p2 < p + pad; p2++)
                    putchar(pad0 ? '0' : ' ');
                while (*p)
                    putchar(*p++);
                break;

            case 'c':
                putchar(va_arg(ap, int));
                break;

            case '%':
                putchar('%');
                break;

            default:
                putchar('%');
                putchar(c);
                break;
            }
        }
    }

    va_end(ap);
}
