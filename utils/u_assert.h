#ifndef U_ASSERT_H
#define U_ASSERT_H

#include <assert.h>

#if _MSC_VER
  #define U_ASSERT(c) if (!(c)) __debugbreak()
#elif __GNUC__
  #define U_ASSERT(c) if (!(c)) __builtin_trap()
#else
  #define U_ASSERT assert
#endif

/*** static assert (since C11)****************************************/

#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 201112L

#ifdef static_assert
  #define U_STATIC_ASSERT(expr, str) static_assert(expr, str)
#endif

#endif

/* older compilers */
#ifndef U_STATIC_ASSERT
/*  #define U_STATIC_ASSERT(expr,str) */
#endif

#endif /* U_ASSERT_H */
