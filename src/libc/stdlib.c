#include <stdlib.h>
#include <x86.h>

void abort(void)
{
  cli();
  for (;;)
    hlt();
}

void exit(int status)
{
  (void)status;
  abort();
}

static int _errno = 0;
int *__errno_location(void)
{
  return &_errno;
}
