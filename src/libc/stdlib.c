#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <x86.h>

void abort(void)
{
  /* Disable interrupts and halt */
  cli();
  for (;;)
    hlt();
}

void exit(int status)
{
  (void)status;
  abort();
}

/* errno location */
static int _errno = 0;
int *__errno_location(void)
{
  return &_errno;
}

/* assert failure - should never be called if asserts disabled */
void __assert_fail(const char *expr, const char *file, int line, const char *func)
{
  (void)expr; (void)file; (void)line; (void)func;
  abort();
}

/* time stub - return a pseudo-random value based on some hardware counter */
time_t time(time_t *tloc)
{
  /* Read TSC (timestamp counter) for some randomness */
  unsigned int lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  time_t t = (time_t)lo;
  if (tloc)
    *tloc = t;
  return t;
}

static int char_to_digit(char c, int base)
{
  int val;
  if (isdigit(c))
    val = c - '0';
  else if (isalpha(c))
    val = tolower(c) - 'a' + 10;
  else
    return -1;
  return val < base ? val : -1;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
  const char *s = nptr;
  unsigned long result = 0;
  int neg = 0;

  // Skip whitespace
  while (isspace(*s))
    s++;

  // Handle sign
  if (*s == '-') {
    neg = 1;
    s++;
  } else if (*s == '+') {
    s++;
  }

  // Detect base from prefix
  if (base == 0 || base == 16) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
      if (base == 0)
        base = 16;
      s += 2;
    } else if (base == 0) {
      base = (s[0] == '0') ? 8 : 10;
    }
  }

  // Convert digits
  int digit;
  while ((digit = char_to_digit(*s, base)) >= 0) {
    result = result * base + digit;
    s++;
  }

  if (endptr)
    *endptr = (char *)s;

  return neg ? -result : result;
}

long strtol(const char *nptr, char **endptr, int base)
{
  const char *s = nptr;
  long result = 0;
  int neg = 0;

  // Skip whitespace
  while (isspace(*s))
    s++;

  // Handle sign
  if (*s == '-') {
    neg = 1;
    s++;
  } else if (*s == '+') {
    s++;
  }

  // Detect base from prefix
  if (base == 0 || base == 16) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
      if (base == 0)
        base = 16;
      s += 2;
    } else if (base == 0) {
      base = (s[0] == '0') ? 8 : 10;
    }
  }

  // Convert digits
  int digit;
  while ((digit = char_to_digit(*s, base)) >= 0) {
    result = result * base + digit;
    s++;
  }

  if (endptr)
    *endptr = (char *)s;

  return neg ? -result : result;
}

int atoi(const char *s)
{
  return (int)strtol(s, NULL, 10);
}

long atol(const char *s)
{
  return strtol(s, NULL, 10);
}

double strtod(const char *nptr, char **endptr)
{
  const char *s = nptr;
  double result = 0.0;
  double frac = 0.0;
  double frac_div = 1.0;
  int neg = 0;
  int exp = 0;
  int exp_neg = 0;

  /* Skip whitespace */
  while (isspace(*s))
    s++;

  /* Handle sign */
  if (*s == '-') {
    neg = 1;
    s++;
  } else if (*s == '+') {
    s++;
  }

  /* Handle special values */
  if ((s[0] == 'i' || s[0] == 'I') &&
      (s[1] == 'n' || s[1] == 'N') &&
      (s[2] == 'f' || s[2] == 'F')) {
    s += 3;
    if (endptr) *endptr = (char *)s;
    return neg ? -1.0/0.0 : 1.0/0.0;
  }
  if ((s[0] == 'n' || s[0] == 'N') &&
      (s[1] == 'a' || s[1] == 'A') &&
      (s[2] == 'n' || s[2] == 'N')) {
    s += 3;
    if (endptr) *endptr = (char *)s;
    return 0.0/0.0;  /* NaN */
  }

  /* Integer part */
  while (isdigit(*s)) {
    result = result * 10.0 + (*s - '0');
    s++;
  }

  /* Fractional part */
  if (*s == '.') {
    s++;
    while (isdigit(*s)) {
      frac_div *= 10.0;
      frac += (*s - '0') / frac_div;
      s++;
    }
    result += frac;
  }

  /* Exponent */
  if (*s == 'e' || *s == 'E') {
    s++;
    if (*s == '-') {
      exp_neg = 1;
      s++;
    } else if (*s == '+') {
      s++;
    }
    while (isdigit(*s)) {
      exp = exp * 10 + (*s - '0');
      s++;
    }

    /* Apply exponent */
    double mult = 1.0;
    while (exp > 0) {
      mult *= 10.0;
      exp--;
    }
    if (exp_neg)
      result /= mult;
    else
      result *= mult;
  }

  if (endptr)
    *endptr = (char *)s;

  return neg ? -result : result;
}

double atof(const char *s)
{
  return strtod(s, NULL);
}

// Simple swap for qsort
static void swap(char *a, char *b, size_t size)
{
  char tmp;
  while (size--) {
    tmp = *a;
    *a++ = *b;
    *b++ = tmp;
  }
}

// Simple insertion sort for small arrays, quicksort for larger
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
  char *arr = base;

  if (nmemb <= 1)
    return;

  // Use insertion sort for small arrays
  if (nmemb <= 16) {
    for (size_t i = 1; i < nmemb; i++) {
      size_t j = i;
      while (j > 0 && compar(arr + (j-1)*size, arr + j*size) > 0) {
        swap(arr + (j-1)*size, arr + j*size, size);
        j--;
      }
    }
    return;
  }

  // Quicksort: pick middle element as pivot
  size_t pivot_idx = nmemb / 2;
  swap(arr + pivot_idx*size, arr + (nmemb-1)*size, size);

  size_t store = 0;
  for (size_t i = 0; i < nmemb - 1; i++) {
    if (compar(arr + i*size, arr + (nmemb-1)*size) < 0) {
      swap(arr + i*size, arr + store*size, size);
      store++;
    }
  }
  swap(arr + store*size, arr + (nmemb-1)*size, size);

  // Recursively sort partitions
  qsort(arr, store, size, compar);
  qsort(arr + (store+1)*size, nmemb - store - 1, size, compar);
}
