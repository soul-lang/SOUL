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

namespace soul
{
    /** List of all SOUL built-in intrinsics. */
    enum class IntrinsicType
    {
        none,
        abs,
        min,
        max,
        clamp,
        wrap,
        fmod,
        remainder,
        floor,
        ceil,
        addModulo2Pi,
        sqrt,
        pow,
        exp,
        log,
        log10,
        sin,
        cos,
        tan,
        sinh,
        cosh,
        tanh,
        asinh,
        acosh,
        atanh,
        asin,
        acos,
        atan,
        atan2,
        isnan,
        isinf,
        sum,
        roundToInt,
        product,
        get_array_size,
        read,
        readLinearInterpolated
    };

    /** Used for compile-time evaluation of an intrinsic function */
    Value performIntrinsic (IntrinsicType, ArrayView<Value> args);

    /** All intrinsics have function declarations in a dedicated namespace with this name. */
    constexpr const char* getIntrinsicsNamespaceName()              { return "soul::intrinsics"; }

    IntrinsicType getIntrinsicTypeFromName (std::string_view);
    const char* getIntrinsicName (IntrinsicType);
    std::string getFullyQualifiedIntrinsicName (IntrinsicType);

}
