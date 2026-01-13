/*
** stdio functions for unknownOS
** snprintf/vsnprintf implementation for Lua number formatting
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Fake FILE streams */
static struct _FILE _stdin, _stdout, _stderr;
FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

/* File operations - stubs that fail */
FILE *fopen(const char *path, const char *mode) {
  (void)path; (void)mode;
  return NULL;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
  (void)path; (void)mode; (void)stream;
  return NULL;
}

int fclose(FILE *stream) {
  (void)stream;
  return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  (void)ptr; (void)size; (void)nmemb; (void)stream;
  return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  (void)ptr; (void)size; (void)nmemb; (void)stream;
  return 0;
}

int feof(FILE *stream) {
  (void)stream;
  return 1;  /* Always at EOF */
}

int ferror(FILE *stream) {
  (void)stream;
  return 0;
}

int fgetc(FILE *stream) {
  (void)stream;
  return EOF;
}

int ungetc(int c, FILE *stream) {
  (void)c; (void)stream;
  return EOF;
}

int fprintf(FILE *stream, const char *format, ...) {
  (void)stream; (void)format;
  return 0;
}

char *strerror(int errnum) {
  (void)errnum;
  return "error";
}

int getc(FILE *stream) {
  (void)stream;
  return EOF;
}

int fflush(FILE *stream) {
  (void)stream;
  return 0;
}

char *fgets(char *s, int size, FILE *stream) {
  (void)s; (void)size; (void)stream;
  return NULL;
}

int fputs(const char *s, FILE *stream) {
  (void)s; (void)stream;
  return EOF;
}

/* Internal: integer to string */
static int int_to_str(char *buf, size_t size, int val, int base, int is_signed, int width, int pad_zero)
{
    char tmp[32];
    char *p = tmp;
    unsigned int uval;
    int neg = 0;
    int len = 0;

    if (is_signed && val < 0) {
        neg = 1;
        uval = -val;
    } else {
        uval = val;
    }

    /* Convert to string (reversed) */
    do {
        int digit = uval % base;
        *p++ = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        uval /= base;
    } while (uval);

    int numlen = p - tmp;
    int total = numlen + neg;

    /* Output padding */
    int i = 0;
    if (width > total && !pad_zero) {
        for (; i < width - total && i < (int)size - 1; i++)
            buf[i] = ' ';
    }
    if (neg && i < (int)size - 1)
        buf[i++] = '-';
    if (width > total && pad_zero) {
        for (; i < width - numlen && i < (int)size - 1; i++)
            buf[i] = '0';
    }

    /* Output digits (reversed) */
    while (p > tmp && i < (int)size - 1)
        buf[i++] = *--p;

    return i;
}

/* Internal: unsigned long to string */
static int ulong_to_str(char *buf, size_t size, unsigned long val, int base, int width, int pad_zero)
{
    char tmp[32];
    char *p = tmp;
    int len = 0;

    do {
        int digit = val % base;
        *p++ = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        val /= base;
    } while (val);

    int numlen = p - tmp;

    int i = 0;
    if (width > numlen && !pad_zero) {
        for (; i < width - numlen && i < (int)size - 1; i++)
            buf[i] = ' ';
    }
    if (width > numlen && pad_zero) {
        for (; i < width - numlen && i < (int)size - 1; i++)
            buf[i] = '0';
    }

    while (p > tmp && i < (int)size - 1)
        buf[i++] = *--p;

    return i;
}

/* Internal: double to string (for %f, %g) */
static int double_to_str(char *buf, size_t size, double val, int prec, int use_g)
{
    int i = 0;

    /* Handle special cases */
    if (val != val) {  /* NaN */
        const char *s = "nan";
        while (*s && i < (int)size - 1)
            buf[i++] = *s++;
        return i;
    }
    if (val == 1.0/0.0) {
        const char *s = "inf";
        while (*s && i < (int)size - 1)
            buf[i++] = *s++;
        return i;
    }
    if (val == -1.0/0.0) {
        const char *s = "-inf";
        while (*s && i < (int)size - 1)
            buf[i++] = *s++;
        return i;
    }

    if (val < 0) {
        if (i < (int)size - 1) buf[i++] = '-';
        val = -val;
    }

    /* Default precision */
    if (prec < 0) prec = 6;

    /* For %g, use exponential if very large or very small */
    if (use_g) {
        if (val != 0.0 && (val >= 1e14 || val < 1e-4)) {
            /* Use exponential notation */
            int exp = 0;
            while (val >= 10.0) { val /= 10.0; exp++; }
            while (val < 1.0 && val != 0.0) { val *= 10.0; exp--; }

            /* Significand */
            int ip = (int)val;
            if (i < (int)size - 1) buf[i++] = '0' + ip;
            if (i < (int)size - 1) buf[i++] = '.';

            double frac = val - ip;
            for (int j = 0; j < prec - 1 && i < (int)size - 1; j++) {
                frac *= 10.0;
                int d = (int)frac;
                buf[i++] = '0' + d;
                frac -= d;
            }

            /* Exponent */
            if (i < (int)size - 1) buf[i++] = 'e';
            if (exp < 0) {
                if (i < (int)size - 1) buf[i++] = '-';
                exp = -exp;
            } else {
                if (i < (int)size - 1) buf[i++] = '+';
            }
            if (exp >= 10) {
                if (i < (int)size - 1) buf[i++] = '0' + (exp / 10);
            }
            if (i < (int)size - 1) buf[i++] = '0' + (exp % 10);
            return i;
        }
        /* Trim trailing zeros for %g */
    }

    /* Integer part */
    unsigned long ip = (unsigned long)val;
    char tmp[32];
    char *p = tmp;
    if (ip == 0) {
        *p++ = '0';
    } else {
        while (ip) {
            *p++ = '0' + (ip % 10);
            ip /= 10;
        }
    }
    while (p > tmp && i < (int)size - 1)
        buf[i++] = *--p;

    /* Fractional part */
    if (prec > 0) {
        if (i < (int)size - 1) buf[i++] = '.';
        double frac = val - (unsigned long)val;
        for (int j = 0; j < prec && i < (int)size - 1; j++) {
            frac *= 10.0;
            int d = (int)frac;
            buf[i++] = '0' + d;
            frac -= d;
        }

        /* For %g, trim trailing zeros */
        if (use_g) {
            while (i > 0 && buf[i-1] == '0') i--;
            if (i > 0 && buf[i-1] == '.') i--;
        }
    }

    return i;
}

int vsnprintf(char *buf, size_t size, const char *format, va_list ap)
{
    size_t pos = 0;

    if (size == 0) return 0;

    while (*format && pos < size - 1) {
        if (*format != '%') {
            buf[pos++] = *format++;
            continue;
        }

        format++;  /* skip % */

        /* Flags */
        int pad_zero = 0;
        int left_align = 0;
        while (*format == '0' || *format == '-') {
            if (*format == '0') pad_zero = 1;
            if (*format == '-') left_align = 1;
            format++;
        }

        /* Width */
        int width = 0;
        while (isdigit(*format)) {
            width = width * 10 + (*format - '0');
            format++;
        }

        /* Precision */
        int prec = -1;
        if (*format == '.') {
            format++;
            prec = 0;
            while (isdigit(*format)) {
                prec = prec * 10 + (*format - '0');
                format++;
            }
        }

        /* Length modifier */
        int is_long = 0;
        if (*format == 'l') {
            is_long = 1;
            format++;
            if (*format == 'l') {  /* ll */
                format++;
            }
        }

        /* Conversion */
        switch (*format) {
        case 'd':
        case 'i': {
            int val = va_arg(ap, int);
            pos += int_to_str(buf + pos, size - pos, val, 10, 1, width, pad_zero);
            break;
        }
        case 'u': {
            unsigned int val = va_arg(ap, unsigned int);
            pos += ulong_to_str(buf + pos, size - pos, val, 10, width, pad_zero);
            break;
        }
        case 'x':
        case 'X': {
            unsigned int val = va_arg(ap, unsigned int);
            pos += ulong_to_str(buf + pos, size - pos, val, 16, width, pad_zero);
            break;
        }
        case 'f': {
            double val = va_arg(ap, double);
            pos += double_to_str(buf + pos, size - pos, val, prec, 0);
            break;
        }
        case 'g':
        case 'G': {
            double val = va_arg(ap, double);
            pos += double_to_str(buf + pos, size - pos, val, prec >= 0 ? prec : 14, 1);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && pos < size - 1)
                buf[pos++] = *s++;
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            buf[pos++] = c;
            break;
        }
        case 'p': {
            unsigned long val = (unsigned long)va_arg(ap, void *);
            if (pos < size - 1) buf[pos++] = '0';
            if (pos < size - 1) buf[pos++] = 'x';
            pos += ulong_to_str(buf + pos, size - pos, val, 16, 8, 1);
            break;
        }
        case '%':
            buf[pos++] = '%';
            break;
        default:
            buf[pos++] = *format;
            break;
        }
        format++;
    }

    buf[pos] = '\0';
    return pos;
}

int snprintf(char *buf, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(buf, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *buf, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(buf, 4096, format, ap);  /* Assume large buffer */
    va_end(ap);
    return ret;
}
