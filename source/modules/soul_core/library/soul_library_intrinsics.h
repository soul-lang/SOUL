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

    The intrinsics namespace contains basic functions, many of which will be performed
    by a JIT engine in more efficient ways than the default function implementations
    that are supplied here. The "[[intrin]]" annotation is used as a hint to the performer
    back-end that this function should be replaced by a native implementation if
    one is available. (Note that some of the functions here have non-functional
    implementations and rely on the performer to use a native implementation for them to
    work at all).
*/
R"library(

namespace soul::intrinsics
{
    /** Familiar abs() function, accepting most scalar types. */
    T abs<T> (T n)                         [[intrin: "abs"]]       { static_assert (T.isScalar, "abs() only works with scalar types"); return n < 0 ? -n : n; }
    /** Returns the greater of two scalar values. */
    T max<T>       (T a, T b)              [[intrin: "max"]]       { static_assert (T.isScalar, "max() only works with scalar types"); return a > b ? a : b; }
    /** Returns the lesser of two scalar values. */
    T min<T>       (T a, T b)              [[intrin: "min"]]       { static_assert (T.isScalar, "min() only works with scalar types"); return a < b ? a : b; }
    /** Returns the lesser of two scalar values. */
    int32 min      (int32 a, int32 b)      [[intrin: "min"]]       { return a < b ? a : b; }

    /** Clamps a scalar value to the nearest value within a given range. */
    T clamp<T>     (T n, T low, T high)    [[intrin: "clamp"]]     { static_assert (T.isScalar, "clamp() only works with scalar types"); return n < low ? low : (n > high ? high : n); }
    /** Performs a negative-number-aware modulo operation to wrap a number to a zero-based range. */
    T wrap<T>      (T n, T range)          [[intrin: "wrap"]]      { static_assert (T.isScalar, "wrap() only works with scalar types");  if (range == 0) return 0; let x = n % range; if (x < 0) return x + range; return x; }
    /** Performs a negative-number-aware integer modulo operation. */
    int32 wrap     (int32 n, int32 range)  [[intrin: "wrap"]]      { if (range == 0) return 0; let x = n % range; if (x < 0) return x + range; return x; }
    /** Performs a C++-compatible floor function on a scalar floating point value. */
    T floor<T>     (T n)                   [[intrin: "floor"]]     { static_assert (T.isScalar && T.primitiveType.isFloat, "floor() only works with scalar floating point types");     return T (int (n)); }
    /** Performs a C++-compatible ceil function on a scalar floating point value. */
    T ceil<T>      (T n)                   [[intrin: "ceil"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "ceil() only works with scalar floating point types");      return T (int (n + 1)); }
    /** Returns a linearly-interpolated value between two scalar values. */
    T lerp<T>      (T start, T stop, T amount)                     { static_assert (T.isScalar && T.primitiveType.isFloat, "lerp() only works with scalar floating point types");      return start + (stop - start) * amount; }

    /** Performs a C++-compatible fmod function on some scalar floating point values. */
    T fmod<T>      (T x, T y)              [[intrin: "fmod"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "fmod() only works with scalar floating point types");      return x - (y * T (int (x / y))); }
    /** Performs a C++-compatible remainder function on some scalar floating point values. */
    T remainder<T> (T x, T y)              [[intrin: "remainder"]] { static_assert (T.isScalar && T.primitiveType.isFloat, "remainder() only works with scalar floating point types"); return x - (y * T (int (0.5f + x / y))); }
    /** Returns the square root of a scalar floating point value. */
    T sqrt<T>      (T n)                   [[intrin: "sqrt"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "sqrt() only works with scalar floating point types");      return T(); }
    /** Raises a scalar floating point value to the given power. */
    T pow<T>       (T a, T b)              [[intrin: "pow"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "pow() only works with scalar floating point types");       return T(); }
    /** Returns the exponential of a scalar floating point value. */
    T exp<T>       (T n)                   [[intrin: "exp"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "exp() only works with scalar floating point types");       return T(); }
    /** Returns the log-e of a scalar floating point value. */
    T log<T>       (T n)                   [[intrin: "log"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "log() only works with scalar floating point types");       return T(); }
    /** Returns the log 10 of a scalar floating point value. */
    T log10<T>     (T n)                   [[intrin: "log10"]]     { static_assert (T.isScalar && T.primitiveType.isFloat, "log10() only works with scalar floating point types");     return T(); }

    /** Adds an delta to a value, and returns the resulting value modulo PI/2.
        A typical use-case for this is in incrementing the phase of an oscillator.
    */
    T addModulo2Pi<T> (T value, T increment) [[intrin: "addModulo2Pi"]]
    {
        value += increment;
        let pi2 = T (twoPi);

        if (value >= pi2)
        {
            if (value >= pi2 * 2)
                return value % pi2;

            return value - pi2;
        }

        if (value < 0)
            return value % pi2 + pi2;

        return value;
    }

    /** Returns the sum of an array or vector of scalar values. */
    Array.elementType sum<Array> (Array n) [[intrin: "sum"]]
    {
        static_assert (Array.isFixedSizeArray || Array.isVector, "sum() only works with fixed-size arrays or vectors");
        static_assert (Array.elementType.isScalar, "sum() only works with arrays of scalar values");

        var total = n[0];
        wrap<Array.size> i;

        loop (Array.size - 1)
            total += n[++i];

        return total;
    }

    /** Returns the product of an array or vector of scalar values. */
    Array.elementType product<Array> (Array n) [[intrin: "product"]]
    {
        static_assert (Array.isFixedSizeArray || Array.isVector, "product() only works with fixed-size arrays or vectors");
        static_assert (Array.elementType.isScalar, "product() only works with arrays of scalar values");

        var product = n[0];
        wrap<Array.size> i;

        loop (Array.size - 1)
            product *= n[++i];

        return product;
    }

    /** Reads an element from an array, allowing the index to be any type of floating point type.
        If a floating point index is used, it will be rounded down to an integer index - for an
        interpolated read operation, see readLinearInterpolated(). Indexes beyond the range of the
        array will be wrapped.
    */
    Array.elementType read<Array, IndexType> (const Array& array, IndexType index) [[intrin: "read"]]
    {
        static_assert (Array.isArray, "read() only works with array types");
        static_assert (IndexType.isPrimitive || IndexType.isInt, "The index for read() must be a floating point or integer value");

        return array.at (int (index));
    }

    /** Reads a linearly-interpolated value from an array of some kind of scalar values (probably
        a float or float-vector type). Indexes beyond the range of the array will be wrapped.
    */
    Array.elementType readLinearInterpolated<Array, IndexType> (const Array& array, IndexType index) [[intrin: "readLinearInterpolated"]]
    {
        static_assert (Array.isArray, "readLinearInterpolated() only works with array types");
        static_assert (Array.elementType.isPrimitive || array.elementType.isVector, "readLinearInterpolated() only works with arrays of primitive or vector values");
        static_assert (Array.elementType.primitiveType.isFloat, "readLinearInterpolated() only works with arrays of float values");
        static_assert (IndexType.isFloat && IndexType.isPrimitive, "The index for readLinearInterpolated() must be a floating point value");

        let size = array.size;

        if (size == 0)
            return Array.elementType();

        let intIndex = int (index);
        let wrappedIndex = int (wrap (intIndex, size));
        let sample1 = array.at (wrappedIndex);
        let sample2 = array.at (wrappedIndex != size - 1 ? wrappedIndex + 1 : 0);

        return sample1 + (sample2 - sample1) * Array.elementType (index - IndexType (intIndex));
    }

    // NB: this is used internally, not something you'd want to call from user code
    int get_array_size<Array> (const Array& array) [[intrin: "get_array_size"]];
}

)library"
