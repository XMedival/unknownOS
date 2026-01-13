#include <types.h>
#include <x86.h>
#include <serial.h>
#include <string.h>

void*
memset(void *dst, int c, size_t n)
{
  if ((size_t)dst%4 == 0 && n%4 == 0){
    c &= 0xFF;
    stosl(dst, (c<<24)|(c<<16)|(c<<8)|c, n/4);
  } else
    stosb(dst, c, n);
  return dst;
}

int
memcmp(const void *v1, const void *v2, size_t n)
{
  const uchar *s1, *s2;

  s1 = v1;
  s2 = v2;
  while(n-- > 0){
    if(*s1 != *s2)
      return *s1 - *s2;
    s1++, s2++;
  }

  return 0;
}

void*
memmove(void *dst, const void *src, size_t n)
{
  const char *s = src;
  char *d = dst;

  if(s < d && s + n > d){
    // Backward copy (overlapping, dst after src)
    s += n;
    d += n;
    // Align to 4-byte boundary first (copy trailing bytes)
    while(n > 0 && ((size_t)d & 3)) {
      *--d = *--s;
      n--;
    }
    // Copy dwords backward
    while(n >= 4) {
      d -= 4;
      s -= 4;
      *(uint*)d = *(const uint*)s;
      n -= 4;
    }
    // Copy remaining bytes
    while(n-- > 0)
      *--d = *--s;
  } else {
    // Forward copy - use rep movsl when aligned
    if(((size_t)d & 3) == 0 && ((size_t)s & 3) == 0 && n >= 4) {
      size_t dwords = n / 4;
      asm volatile("rep movsl"
        : "+D"(d), "+S"(s), "+c"(dwords)
        :
        : "memory");
      n &= 3;  // remaining bytes
    }
    while(n-- > 0)
      *d++ = *s++;
  }

  return dst;
}

// memcpy exists to placate GCC.  Use memmove.
void*
memcpy(void *dst, const void *src, size_t n)
{
  return memmove(dst, src, n);
}

void*
memchr(const void *s, int c, size_t n)
{
  const uchar *p = s;
  while(n-- > 0) {
    if(*p == (uchar)c)
      return (void*)p;
    p++;
  }
  return NULL;
}

int
strcmp(const char *s1, const char *s2)
{
  while(*s1 && *s1 == *s2)
    s1++, s2++;
  return (uchar)*s1 - (uchar)*s2;
}

int
strncmp(const char *p, const char *q, size_t n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

char*
strchr(const char *s, int c)
{
  for(; *s; s++)
    if(*s == (char)c)
      return (char*)s;
  return (c == '\0') ? (char*)s : NULL;
}

char*
strrchr(const char *s, int c)
{
  const char *last = NULL;
  for(; *s; s++)
    if(*s == (char)c)
      last = s;
  return (c == '\0') ? (char*)s : (char*)last;
}

char*
strstr(const char *haystack, const char *needle)
{
  size_t nlen = strlen(needle);
  if(nlen == 0) return (char*)haystack;
  for(; *haystack; haystack++) {
    if(*haystack == *needle && strncmp(haystack, needle, nlen) == 0)
      return (char*)haystack;
  }
  return NULL;
}

char*
strncpy(char *s, const char *t, size_t n)
{
  char *os;

  os = s;
  while(n-- > 0 && (*s++ = *t++) != 0)
    ;
  while(n-- > 0)
    *s++ = 0;
  return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char*
safestrcpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  if(n <= 0)
    return os;
  while(--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

size_t
strlen(const char *s)
{
  size_t n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

char*
strcpy(char *dst, const char *src)
{
  char *d = dst;
  while((*d++ = *src++) != '\0')
    ;
  return dst;
}

char*
strpbrk(const char *s, const char *accept)
{
  for(; *s; s++) {
    for(const char *a = accept; *a; a++) {
      if(*s == *a)
        return (char*)s;
    }
  }
  return NULL;
}

size_t
strspn(const char *s, const char *accept)
{
  size_t n = 0;
  for(; *s; s++) {
    const char *a;
    for(a = accept; *a && *a != *s; a++)
      ;
    if(!*a)
      break;
    n++;
  }
  return n;
}

size_t
strcspn(const char *s, const char *reject)
{
  size_t n = 0;
  for(; *s; s++) {
    for(const char *r = reject; *r; r++) {
      if(*s == *r)
        return n;
    }
    n++;
  }
  return n;
}

int
strcoll(const char *s1, const char *s2)
{
  /* No locale support - just use strcmp */
  return strcmp(s1, s2);
}
