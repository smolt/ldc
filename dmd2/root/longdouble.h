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
// dano - TODO: hack this in for now
#if 0
typedef long double longdouble;
typedef volatile long double volatile_longdouble;
#else
typedef double longdouble;
typedef volatile double volatile_longdouble;
#undef LDBL_DIG
#undef LDBL_MAX
#undef LDBL_MIN
#undef LDBL_EPSILON
#undef LDBL_MANT_DIG
#undef LDBL_MAX_EXP
#undef LDBL_MIN_EXP
#undef LDBL_MAX_10_EXP
#undef LDBL_MIN_10_EXP

#define LDBL_DIG  DBL_DIG
#define LDBL_MAX  DBL_MAX
#define LDBL_MIN  DBL_MIN
#define LDBL_EPSILON  DBL_EPSILON
#define LDBL_MANT_DIG DBL_MANT_DIG
#define LDBL_MAX_EXP DBL_MAX_EXP
#define LDBL_MIN_EXP DBL_MIN_EXP
#define LDBL_MAX_10_EXP DBL_MAX_10_EXP
#define LDBL_MIN_10_EXP DBL_MIN_10_EXP
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
        char sfmt[5] = "%#Lg";
        sfmt[3] = fmt;
        return sprintf(str, sfmt, x);
    }
    else
    {
        char sfmt[4] = "%Lg";
        sfmt[2] = fmt;
        return sprintf(str, sfmt, x);
    }
}

#if __MINGW32__
#undef sprintf
#endif

#endif // __LONG_DOUBLE_H__
