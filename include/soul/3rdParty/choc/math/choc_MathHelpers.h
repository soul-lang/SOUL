/*
    ██████ ██   ██  ██████   ██████
   ██      ██   ██ ██    ██ ██         Clean Header-Only Classes
   ██      ███████ ██    ██ ██         Copyright (C)2020 Julian Storer
   ██      ██   ██ ██    ██ ██
    ██████ ██   ██  ██████   ██████    https://github.com/julianstorer/choc

   This file is part of the CHOC C++ collection - see the github page to find out more.

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose with
   or without fee is hereby granted, provided that the above copyright notice and this
   permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
   THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
   SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
   CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
   OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef CHOC_MATH_HELPERS_HEADER_INCLUDED
#define CHOC_MATH_HELPERS_HEADER_INCLUDED

#include <cstdlib>

#ifdef _MSC_VER
 #include <intrin.h>
 #pragma intrinsic (_umul128)

 #ifdef _WIN64
  #pragma intrinsic (_BitScanReverse64)
 #else
  #pragma intrinsic (_BitScanReverse)
 #endif
#endif

namespace choc::math
{

//==============================================================================
/// Returns true if the given value is 2^something
template <typename Integer>
constexpr bool isPowerOf2 (Integer n)       { return n > 0 && (n & (n - 1)) == 0; }

/// Returns the number of contiguously-clear upper bits in a 64-bit value
inline uint32_t countUpperClearBits (uint64_t value)
{
   #ifdef _MSC_VER
    unsigned long result = 0;
    #ifdef _WIN64
     _BitScanReverse64 (&result, value);
    #else
     if (_BitScanReverse (&result, static_cast<unsigned long> (value >> 32))) return static_cast<uint32_t> (31u - result);
     _BitScanReverse (&result, static_cast<unsigned long> (value));
    #endif
    return static_cast<uint32_t> (63u - result);
   #else
    return static_cast<uint32_t> (__builtin_clzll (value));
   #endif
}


/// Returns the number of decimal digits required to print a given unsigned number
inline int getNumDecimalDigits (uint32_t n)
{
    return n < 1000 ? (n < 10 ? 1 : (n < 100 ? 2 : 3))
         : n < 1000000 ? (n < 10000 ? 4 : (n < 100000 ? 5 : 6))
         : n < 100000000 ? (n < 10000000 ? 7 : 8)
         : n < 1000000000 ? 9 : 10;
}


//==============================================================================
/// Used as a return type for multiply128()
struct Int128
{
    uint64_t high, low;
};

/// A cross-platform function to multiply two 64-bit numbers and return a 128-bit result
inline Int128 multiply128 (uint64_t a, uint64_t b)
{
   #ifdef _MSC_VER
    Int128 result;
    result.low = _umul128 (a, b, &result.high);
    return result;
   #elif __LP64__
    auto total = static_cast<unsigned __int128> (a) * static_cast<unsigned __int128> (b);
    return { static_cast<uint64_t> (total >> 64), static_cast<uint64_t> (total) };
   #else
    uint64_t a0 = static_cast<uint32_t> (a), a1 = a >> 32,
             b0 = static_cast<uint32_t> (b), b1 = b >> 32;
    auto p10 = a1 * b0, p00 = a0 * b0,
         p11 = a1 * b1, p01 = a0 * b1;
    auto middleBits = p10 + static_cast<uint32_t> (p01) + (p00 >> 32);
    return { p11 + (middleBits >> 32) + (p01 >> 32), (middleBits << 32) | static_cast<uint32_t> (p00) };
   #endif
}


} // namespace choc::math

#endif
