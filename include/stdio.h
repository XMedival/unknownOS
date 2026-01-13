#pragma once
#include <types.h>

/* FILE type - minimal definition for freestanding */
struct _FILE {
  int dummy;  /* Placeholder - we don't support real files */
};
typedef struct _FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)
#define BUFSIZ 1024

/* From EGA.h */
void putchar(int c);
void printf(const char *format, ...);

/* Buffer-based printf variants */
int snprintf(char *buf, size_t size, const char *format, ...);
int sprintf(char *buf, const char *format, ...);

/* va_list based variants */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

int vsnprintf(char *buf, size_t size, const char *format, va_list ap);

/* File operations - stubs for freestanding */
FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int getc(FILE *stream);
int fflush(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);

/* Error string */
char *strerror(int errnum);
