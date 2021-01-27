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

//==============================================================================
/**
    A basic scalar type. For complex types, see the Type class.
    @see Type, TypeRules
*/
struct PrimitiveType  final
{
    enum Primitive
    {
        invalid = 0,
        void_,
        float32,
        float64,
        fixed,
        complex32,
        complex64,
        int32,
        int64,
        bool_
    };

    constexpr PrimitiveType() = default;
    constexpr PrimitiveType (Primitive t) : type (t) {}

    PrimitiveType (const PrimitiveType&);
    PrimitiveType& operator= (const PrimitiveType&);

    bool operator== (const PrimitiveType&) const;
    bool operator!= (const PrimitiveType&) const;

    bool isValid() const;
    bool isVoid() const;
    bool isFloat32() const;
    bool isFloat64() const;
    bool isFloatingPoint() const;
    bool isFixed() const;
    bool isComplex() const;
    bool isComplex32() const;
    bool isComplex64() const;
    bool isInteger() const;
    bool isInteger32() const;
    bool isInteger64() const;
    bool isBool() const;
    bool canBeVectorElementType() const;
    bool canBeArrayElementType() const;

    const char* getDescription() const;
    const char* getShortIdentifierDescription() const;

    uint64_t getPackedSizeInBytes() const;

    Primitive type = Primitive::invalid;
};

} // namespace soul
