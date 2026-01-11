#include <types.h>
#include <x86.h>
#include <serial.h>
#include <string.h>

void*
memset(void *dst, int c, uint n)
{
  if ((int)dst%4 == 0 && n%4 == 0){
    c &= 0xFF;
    stosl(dst, (c<<24)|(c<<16)|(c<<8)|c, n/4);
  } else
    stosb(dst, c, n);
  return dst;
}

int
memcmp(const void *v1, const void *v2, uint n)
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
memmove(void *dst, const void *src, uint n)
{
  const char *s = src;
  char *d = dst;

  if(s < d && s + n > d){
    // Backward copy (overlapping, dst after src)
    s += n;
    d += n;
    // Align to 4-byte boundary first (copy trailing bytes)
    while(n > 0 && ((uint)d & 3)) {
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
    if(((uint)d & 3) == 0 && ((uint)s & 3) == 0 && n >= 4) {
      uint dwords = n / 4;
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
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}

int
strncmp(const char *p, const char *q, uint n)
{
  while(n > 0 && *p && *p == *q)
    n--, p++, q++;
  if(n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

char*
strncpy(char *s, const char *t, int n)
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

int
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}
