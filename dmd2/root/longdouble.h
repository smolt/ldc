/* Copyright (c) 1999-2014 by Digital Mars
 * All Rights Reserved, written by Rainer Schuetze
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 * https://github.com/D-Programming-Language/dmd/blob/master/src/root/longdouble.h
 */

// 80 bit floating point value implementation for Microsoft compiler

#ifndef __LONG_DOUBLE_H__
#define __LONG_DOUBLE_H__

#include <stdio.h>

#if USE_REAL64
/* Kludge to support cross-compiling.

   Code here and sprinkled about dmd will assume real type is same as double
   instead of the native long double when USE_REAL_64 is set.  All uses of
   long double are through "longdouble" and real_t typedef but some of dmd
   uses the LDBL_ constants for real attributes, so override the float.h
   constants.  This all helps when cross-compiling from a host with a greater
   precision long double and targeting a cpu with 64-bit double.
*/
typedef double longdouble;
typedef volatile double volatile_longdouble;

// undo what the compiler and float.h say for LDBL.  By including float.h
// first, we ensure include order elsewhere won't make a difference.
#include <float.h>
#undef LDBL_DIG
#define LDBL_DIG  DBL_DIG
#undef LDBL_MAX
#define LDBL_MAX  DBL_MAX
#undef LDBL_MIN
#define LDBL_MIN  DBL_MIN
#undef LDBL_EPSILON
#define LDBL_EPSILON  DBL_EPSILON
#undef LDBL_MANT_DIG
#define LDBL_MANT_DIG DBL_MANT_DIG
#undef LDBL_MAX_EXP
#define LDBL_MAX_EXP DBL_MAX_EXP
#undef LDBL_MIN_EXP
#define LDBL_MIN_EXP DBL_MIN_EXP
#undef LDBL_MAX_10_EXP
#define LDBL_MAX_10_EXP DBL_MAX_10_EXP
#undef LDBL_MIN_10_EXP
#define LDBL_MIN_10_EXP DBL_MIN_10_EXP

#else // use native long double (what ever it might be)
typedef long double longdouble;
typedef volatile long double volatile_longdouble;
#endif

// also used from within C code, so use a #define rather than a template
// template<typename T> longdouble ldouble(T x) { return (longdouble) x; }
#define ldouble(x) ((longdouble)(x))

#if __MINGW32__
// MinGW supports 80 bit reals, but the formatting functions map to versions
// from the MSVC runtime by default which don't.
#define sprintf __mingw_sprintf
#endif

inline size_t ld_sprint(char* str, int fmt, longdouble x)
{
    if (((longdouble)(unsigned long long)x) == x)
    {   // ((1.5 -> 1 -> 1.0) == 1.5) is false
        // ((1.0 -> 1 -> 1.0) == 1.0) is true
        // see http://en.cppreference.com/w/cpp/io/c/fprintf
#if USE_REAL64
        char sfmt[4] = "%#g";
        sfmt[2] = fmt;
#else
        char sfmt[5] = "%#Lg";
        sfmt[3] = fmt;
#endif
        return sprintf(str, sfmt, x);
    }
    else
    {
#if USE_REAL64
        char sfmt[3] = "%g";
        sfmt[1] = fmt;
#else
        char sfmt[4] = "%Lg";
        sfmt[2] = fmt;
#endif
        return sprintf(str, sfmt, x);
    }
}

#if __MINGW32__
#undef sprintf
#endif

#endif // __LONG_DOUBLE_H__
