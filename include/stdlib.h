#pragma once
#include <types.h>

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
int atoi(const char *s);
long atol(const char *s);
double atof(const char *s);

static inline int abs(int n) {
  return n < 0 ? -n : n;
}

static inline long labs(long n) {
  return n < 0 ? -n : n;
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

void abort(void) __attribute__((noreturn));
void exit(int status) __attribute__((noreturn));

/* Memory allocation - simple wrappers */
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);

/* errno support */
extern int *__errno_location(void);
#define errno (*__errno_location())

/* assert support */
void __assert_fail(const char *expr, const char *file, int line, const char *func);

/* time stub */
typedef long time_t;
time_t time(time_t *tloc);
