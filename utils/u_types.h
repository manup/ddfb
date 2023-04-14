#ifndef U_TYPES_H
#define U_TYPES_H

#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

  #include <stdint.h>
  typedef uint8_t u8;
  typedef uint16_t u16;
  typedef uint32_t u32;
  typedef uint64_t u64;
  typedef int8_t i8;
  typedef int16_t i16;
  typedef int32_t i32;
  typedef int64_t i64;
  typedef float f32;
  typedef double f64;

  #define _U_HAS_TYPES
  #define _U_HAS_S64_TYPE
  #define _U_HAS_U64_TYPE

  #define U_FUNCTION __FUNCTION__

#else /* ANSI C */

  typedef unsigned char u8;
  typedef unsigned short u16;
  typedef unsigned int u32;
  typedef signed char i8;
  typedef signed short i16;
  typedef signed int i32;
  /* following don't work with pedantic */
  /* typedef unsigned long long u64; */
  /* typedef signed long long i64; */
  typedef float f32;
  typedef double f64;

  #define _U_HAS_TYPES

  #define U_FUNCTION "fun"

#ifdef __GNUC__
    #ifdef __U64_TYPE
    typedef __U64_TYPE u64;
    #endif

    #ifdef __S64_TYPE
    typedef __S64_TYPE i64;
    #endif
#endif /* __GNUC__ */

#endif

#ifndef _U_HAS_TYPES
      #error "TODO stdint.h not available, C89 non GNU"
#endif

#endif /* U_TYPES_H */
