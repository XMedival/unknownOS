#pragma once
#include <panic.h>

#define assert(expr) ((void)0)

#ifdef NDEBUG
  #define ASSERT(expr) ((void)0)
#else
  #define ASSERT(expr) do { if (!(expr)) panic(__FILE__,__LINE__,__func__,#expr); } while (0)
#endif

#define static_assert _Static_assert
