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

#if USE_OSX_TARGET_REAL
/* Support limited cross-compiling on OS X to target real type.

   This is not a general solution and probably won't work on other OSes,
   instead just enough to support a single cross compiler on X86 Mac targeting
   OS X variants - MacOS, iOS (ARM) 64-bit real or iOS sim (X86) 80-bit real.
   There is a better solution coming in LDC and this can all get stripped once
   that is fully available.

   Code here and sprinkled about uses typedef longdouble to represent D real.
   Normally it is set to long double but here we override to a limited Real
   value type that does just enough to satisfy needs of dmd code without much
   further code change.  #if USE_OSX_TARGET_REAL is wrapping places where code
   was changed to make it easier to find and disable when this needs to be
   backed out.  Some dmd code uses the float.h LDBL_ constants for real
   attributes, so those are overriden here too.

   Real just wraps a long double but based on `targetReal64` may convert to
   double precision in ctor.  That way it never holds more precision than the
   target.  Once a Real is constructed, it always has target's precision.
   Single operations are done with long double but result is converted again
   if needed.  The key is for CTFE to be close to what real target would
   compute.

   dmd uses sizeof(longdouble) or sizeof(real_t) in some places to determine
   buffer sizes which means oversized when 64-bit real, but only used for
   print buffers, so no worries.  There are other places when address is
   taken, but we ensure that Real bitwise is a valid long double.

   Implementation for Real is in port.c because this is temporary.
*/

#include <assert.h>
#include <math.h>
#include <memory.h>

struct Real                   // to used to D struct for value :-)
{
private:
    // Only member
    long double val;

    // Assumption that has x87 80-bit rep for long double
    static const size_t size = 10;
    static const size_t pad = 6;

    // Set if target real is 64-bits
    static bool targetReal64;
    static bool initialized;

public:
    // Must be called before any other use with 'useReal64' true for targets like
    // ARM real == double, or false for X86.
    static void init(bool useReal64);

    static bool useReal64()
    {
        assert(initialized); // kick out earlier arrivals
        return targetReal64;
    }

    Real() : val(0) {}

    Real(long double x)
    {
        if (useReal64())
            val = (double)x;
        else
            val = x;
    }

    Real operator=(Real x)
    {
        val = x.val;
        return *this;
    }

    // bit for bit comparison
    bool bitsmatch(Real x) const
    {
        return memcmp(&val, &x.val, size) == 0;
    }

    // Could zero out pad of long double type when someone wants raw bits.
    //
    // Note 1: Can make private to find all the address takers - makes compiler
    // error.
    //
    // Note 2: Don't think this is really needed - I was fooled by existing
    // issue discussed in pull #770, so this operator doesn't fix it.  But I
    // think last 6 pad bytes should be undefined.  Expecting zero is wrong.
    //
    // https://github.com/ldc-developers/ldc/pull/770
    /*
    Real* operator&()
    {
        memset(reinterpret_cast<char*>(&val) + size, 0, pad);
        return this;
    }
    */

    // provide enough conversions used by rest of dmd code and make explict to
    // avoid ambiguities
    explicit operator bool() const {return val != 0;}
    explicit operator signed char() const {return val;}
    explicit operator unsigned char() const {return val;}
    explicit operator short() const {return val;}
    explicit operator unsigned short() const {return val;}
    explicit operator int() const {return val;}
    explicit operator unsigned int() const {return val;}
    explicit operator long() const {return val;}
    explicit operator unsigned long() const {return val;}
    explicit operator long long() const {return val;}
    explicit operator unsigned long long() const {return val;}
    explicit operator float() const {return val;}
    explicit operator double() const {return val;}
    explicit operator long double() const {return val;}

    // provide operators used by rest of dmd code
    bool operator<(Real x) const {return val < x.val;}
    bool operator<=(Real x) const {return val <= x.val;}
    bool operator>(Real x) const {return val > x.val;}
    bool operator>=(Real x) const {return val >= x.val;}
    bool operator==(Real x) const {return val == x.val;}
    bool operator!=(Real x) const {return val != x.val;}
    Real operator+(Real x) const {return val + x.val;}
    Real operator-(Real x) const {return val - x.val;}
    Real operator-() const {return -val;}
    Real operator*(Real x) const {return val * x.val;}
    Real operator/(Real x) const {return val / x.val;}

    // provide the math functions used by dmd code
    friend Real sinl(Real x) {return sinl(x.val);}
    friend Real cosl(Real x) {return cosl(x.val);}
    friend Real tanl(Real x) {return tanl(x.val);}
    friend Real fabsl(Real x) {return fabsl(x.val);}
    friend Real sqrtl(Real x) {return sqrtl(x.val);}
    friend Real logl(Real x) {return logl(x.val);}
    friend Real fminl(Real x, Real y) {return fminl(x.val,y.val);}
    friend Real fmaxl(Real x, Real y) {return fmaxl(x.val,y.val);}
    friend Real floor(Real x) {return floorl(x.val);}
    friend Real ceil(Real x) {return ceill(x.val);}
    friend Real trunc(Real x) {return truncl(x.val);}
    friend Real round(Real x) {return roundl(x.val);}

    static int ldbl_dig() {return useReal64() ? __DBL_DIG__ : __LDBL_DIG__;}
    static Real ldbl_max() {return useReal64() ? __DBL_MAX__ : __LDBL_MAX__;}
    static Real ldbl_min() {return useReal64() ? __DBL_MIN__ : __LDBL_MIN__;}
    static Real ldbl_epsilon() {return useReal64() ? __DBL_EPSILON__ : __LDBL_EPSILON__;}
    static int ldbl_mant_dig() {return useReal64() ? __DBL_MANT_DIG__ : __LDBL_MANT_DIG__;}
    static int ldbl_max_exp() {return useReal64() ? __DBL_MAX_EXP__ : __LDBL_MAX_EXP__;}
    static int ldbl_min_exp() {return useReal64() ? __DBL_MIN_EXP__ : __LDBL_MIN_EXP__;}
    static int ldbl_max_10_exp() {return useReal64() ? __DBL_MAX_10_EXP__ : __LDBL_MAX_10_EXP__;}
    static int ldbl_min_10_exp() {return useReal64() ? __DBL_MIN_10_EXP__ : __LDBL_MIN_10_EXP__;}
};


typedef Real longdouble;
// volatile should not be needed with Real
typedef Real volatile_longdouble;

// undo what the compiler and float.h say for LDBL.  By including float.h
// first, we ensure include order elsewhere won't make a difference.
#include <float.h>
#undef LDBL_DIG
#define LDBL_DIG  Real::ldbl_dig()
#undef LDBL_MAX
#define LDBL_MAX  Real::ldbl_max()
#undef LDBL_MIN
#define LDBL_MIN  Real::ldbl_min()
#undef LDBL_EPSILON
#define LDBL_EPSILON  Real::ldbl_epsilon()
#undef LDBL_MANT_DIG
#define LDBL_MANT_DIG Real::ldbl_mant_dig()
#undef LDBL_MAX_EXP
#define LDBL_MAX_EXP Real::ldbl_max_exp()
#undef LDBL_MIN_EXP
#define LDBL_MIN_EXP Real::ldbl_min_exp()
#undef LDBL_MAX_10_EXP
#define LDBL_MAX_10_EXP Real::ldbl_max_10_exp()
#undef LDBL_MIN_10_EXP
#define LDBL_MIN_10_EXP Real::ldbl_min_10_exp()

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
