/*
    _____ _____ _____ __
   |   __|     |  |  |  |      The SOUL language
   |__   |  |  |  |  |  |__    Copyright (c) 2019 - ROLI Ltd.
   |_____|_____|_____|_____|

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose
   with or without fee is hereby granted, provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*  The following string literal forms part of a set of SOUL code chunks that form
    the built-in library. (See the soul::getBuiltInLibraryCode() function)
*/
R"library(

namespace soul::intrinsics
{
    T sin<T>   (T n)  [[intrin: "sin"]]      { static_assert (T.isPrimitive || T.isVector, "sin() only works with floating point types");   static_assert (T.primitiveType.isFloat, "sin() only works with floating point types");   return T(); }
    T cos<T>   (T n)  [[intrin: "cos"]]      { static_assert (T.isPrimitive || T.isVector, "cos() only works with floating point types");   static_assert (T.primitiveType.isFloat, "cos() only works with floating point types");   return T(); }
    T tan<T>   (T n)  [[intrin: "tan"]]      { static_assert (T.isPrimitive || T.isVector, "tan() only works with floating point types");   static_assert (T.primitiveType.isFloat, "tan() only works with floating point types");   return sin (n) / cos (n); }
    T sinh<T>  (T n)  [[intrin: "sinh"]]     { static_assert (T.isPrimitive || T.isVector, "sinh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "sinh() only works with floating point types");  return (exp (n) - exp (-n)) / 2; }
    T cosh<T>  (T n)  [[intrin: "cosh"]]     { static_assert (T.isPrimitive || T.isVector, "cosh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "cosh() only works with floating point types");  return (exp (n) + exp (-n)) / 2; }
    T tanh<T>  (T n)  [[intrin: "tanh"]]     { static_assert (T.isPrimitive || T.isVector, "tanh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "tanh() only works with floating point types");  return sinh (n) / cosh (n); }
    T asinh<T> (T n)  [[intrin: "asinh"]]    { static_assert (T.isPrimitive || T.isVector, "asinh() only works with floating point types"); static_assert (T.primitiveType.isFloat, "asinh() only works with floating point types"); return log (n + sqrt (n * n + 1)); }
    T acosh<T> (T n)  [[intrin: "acosh"]]    { static_assert (T.isPrimitive || T.isVector, "acosh() only works with floating point types"); static_assert (T.primitiveType.isFloat, "acosh() only works with floating point types"); return log (n + sqrt (n * n - 1)); }
    T atanh<T> (T n)  [[intrin: "atanh"]]    { static_assert (T.isPrimitive || T.isVector, "atanh() only works with floating point types"); static_assert (T.primitiveType.isFloat, "atanh() only works with floating point types"); return 0.5f * log ((1 + n) / (1 - n)); }
    T asin<T>  (T n)  [[intrin: "asin"]]     { static_assert (T.isPrimitive || T.isVector, "asin() only works with floating point types");  static_assert (T.primitiveType.isFloat, "asin() only works with floating point types");  return atan (n / (sqrt (1 - (n * n)))); }
    T acos<T>  (T n)  [[intrin: "acos"]]     { static_assert (T.isPrimitive || T.isVector, "acos() only works with floating point types");  static_assert (T.primitiveType.isFloat, "acos() only works with floating point types");  return atan ((sqrt (1 - (n * n))) / n); }
    T atan<T>  (T n)  [[intrin: "atan"]]     { static_assert (T.isPrimitive || T.isVector, "atan() only works with floating point types");  static_assert (T.primitiveType.isFloat, "atan() only works with floating point types");  return n < 0 ? -helpers::atanHelperPositive (-n) : helpers::atanHelperPositive (n); }

    T atan2<T> (T y, T x) [[intrin: "atan2"]]
    {
        static_assert (T.isPrimitive || T.isVector, "atan2() only works with floating point types");
        static_assert (T.primitiveType.isFloat, "atan2() only works with floating point types");

        if (x == 0)
        {
            if (y > 0) return T (pi /  2.0);
            if (y < 0) return T (pi / -2.0);

            return 0;  // Undefined case: return 0
        }

        let atanYoverX = atan (y / x);

        if (x > 0)
            return atanYoverX;

        return y >= 0 ? atanYoverX + T (pi)
                      : atanYoverX - T (pi);
    }

    namespace helpers
    {
        T atanHelperPositive<T> (T n)
        {
            return n > 1.0f ? (T (pi / 2.0) - atanHelper0to1 (1 / n))
                            : atanHelper0to1 (n);
        }

        T atanHelper0to1<T> (T n)
        {
            let sqrt3 = T (sqrt (3.0));

            if (n > T (2.0 - sqrt3))
            {
                let n2 = (sqrt3 * n - 1) / (sqrt3 + n);

                if (n2 < 0)
                    return T (pi / 6.0) - atanHelperApprox (-n2);

                return T (pi / 6.0) + atanHelperApprox (n2);
            }

            return atanHelperApprox (n);
        }

        T atanHelperApprox<T> (T n)
        {
            return n - (n * n * n) / 3.0f + (n * n * n * n * n) / 5.0f;
        }
    }
}

)library"
