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
/** Implements various rules and operations relating to type casting. */
struct TypeRules
{
    enum class CastType
    {
        notPossible,
        identity,
        primitiveNumericLossless,
        primitiveNumericReduction,
        arrayElementLossless,
        arrayElementReduction,
        valueToArray,
        singleElementVectorToScalar,
        fixedSizeArrayToDynamicArray,
        wrapValue,
        clampValue
    };

    static CastType getCastType (PrimitiveType dest, PrimitiveType source)
    {
        if (dest.isValid() && source.isValid())
        {
            if (dest == source)
                return CastType::identity;

            if (dest.isVoid() || source.isVoid())
                return CastType::notPossible;

            if (source.isBool())
                return CastType::primitiveNumericReduction;

            if (source.isComplex() && ! dest.isComplex())
                return CastType::notPossible;

            if (dest.isInteger32() && source.isInteger64())
                return CastType::primitiveNumericReduction;

            if (dest.isInteger())
                return source.isFloatingPoint() ? CastType::primitiveNumericReduction
                                                : CastType::primitiveNumericLossless;

            if (dest.isFloat32())
                return CastType::primitiveNumericReduction;

            if (dest.isFloat64())
                return CastType::primitiveNumericLossless;

            if (dest.isComplex32())
            {
                if (source.isComplex64())
                    return CastType::primitiveNumericReduction;

                return getCastType (PrimitiveType::float32, source);
            }

            if (dest.isComplex64())
            {
                if (source.isComplex32())
                    return CastType::primitiveNumericLossless;

                return getCastType (PrimitiveType::float64, source);
            }

            if (dest.isBool())
                return CastType::primitiveNumericReduction;

            if (dest.isFixed())
                throwError (Errors::notYetImplemented ("Fixed point type support"));
        }

        return CastType::notPossible;
    }

    static CastType getCastType (const Type& dest, const Type& source)
    {
        if (dest.isValid() && source.isValid())
        {
            if (dest.isEqual (source, Type::ignoreReferences))
                return CastType::identity;

            if (dest.isVoid() || source.isVoid())
                return CastType::notPossible;

            if ((dest.isPrimitive() || dest.isVectorOfSize1()) &&
                (source.isPrimitive() || source.isVectorOfSize1()) && ! dest.isReference())
                return getCastType (dest.getPrimitiveType(), source.getPrimitiveType());

            if (! dest.isReference())
            {
                if (source.isReference())
                {
                    if (source.isConst() && source.isEqual (dest, Type::ignoreConst | Type::ignoreReferences))
                        return CastType::identity;
                }
                else
                {
                    if (source.isEqual (dest, Type::ignoreConst))
                        return CastType::identity;
                }
            }

            if (dest.isArray())
            {
                if (dest.isUnsizedArray()
                     && source.isFixedSizeArray()
                     && source.getElementType().isIdentical (dest.getElementType()))
                    return CastType::fixedSizeArrayToDynamicArray;

                if ((source.isPrimitive() || source.isVectorOfSize1())
                      && getCastType (dest.getArrayElementType(), source.getPrimitiveType()) != CastType::notPossible)
                    return CastType::valueToArray;

                if (source.isFixedSizeArray() && source.getArrayOrVectorSize() == dest.getArrayOrVectorSize())
                {
                    auto elementCast = getCastType (dest.getArrayElementType(), source.getArrayElementType());

                    if (elementCast == CastType::primitiveNumericReduction)   return CastType::arrayElementReduction;
                    if (elementCast == CastType::primitiveNumericLossless)    return CastType::arrayElementLossless;
                    if (elementCast == CastType::singleElementVectorToScalar) return CastType::arrayElementLossless;
                    if (elementCast == CastType::valueToArray)                return CastType::arrayElementLossless;
                    if (elementCast == CastType::identity)                    return CastType::identity;
                }

                if (source.isStruct() && dest.isFixedSizeArray() && dest.getArrayElementType().isIdentical (source))
                    return CastType::valueToArray;

                return CastType::notPossible;
            }

            if (dest.isVector())
            {
                if ((source.isPrimitive() || source.isVectorOfSize1())
                      && getCastType (dest.getVectorElementType(), source.getPrimitiveType()) != CastType::notPossible)
                    return CastType::valueToArray;

                if (source.isVector() && source.getArrayOrVectorSize() == dest.getArrayOrVectorSize())
                {
                    auto elementCast = getCastType (dest.getVectorElementType(), source.getVectorElementType());

                    if (elementCast == CastType::identity)                   return CastType::identity;
                    if (elementCast == CastType::primitiveNumericReduction)  return CastType::arrayElementReduction;
                    if (elementCast == CastType::primitiveNumericLossless)   return CastType::arrayElementLossless;
                }

                return CastType::notPossible;
            }

            if (dest.isStruct() || source.isStruct())
                return CastType::notPossible;

            if (dest.isBoundedInt())
            {
                if (source.isBoundedInt() && dest.getBoundedIntLimit() >= source.getBoundedIntLimit())
                    return CastType::identity;

                if (dest.isWrapped())
                    if (source.isBoundedInt() || source.isPrimitiveInteger() || source.isPrimitiveFloat())
                        return CastType::wrapValue;

                if (dest.isClamped())
                    if (source.isBoundedInt() || source.isPrimitiveInteger() || source.isPrimitiveFloat())
                        return CastType::clampValue;

                return CastType::notPossible;
            }

            if (source.isBoundedInt())
                return getCastType (dest, PrimitiveType::int32);

            if (dest.isStringLiteral() && source.isStringLiteral())
                return CastType::identity;

            if (dest.isPrimitive() && source.isVectorOfSize1()
                 && (dest.isEqual (source, Type::ignoreConst | Type::ignoreVectorSize1)
                      || canSilentlyCastTo (dest.getPrimitiveType(), source.getPrimitiveType())))
                return CastType::singleElementVectorToScalar;
        }

        return CastType::notPossible;
    }

    static bool canCastTo (PrimitiveType dest, PrimitiveType source)
    {
        return getCastType (dest, source) != CastType::notPossible;
    }

    static bool canCastTo (const Type& dest, const Type& source)
    {
        return getCastType (dest, source) != CastType::notPossible;
    }

    static bool isSilentCast (CastType type)
    {
        return type == CastType::identity
            || type == CastType::primitiveNumericLossless
            || type == CastType::arrayElementLossless
            || type == CastType::valueToArray
            || type == CastType::singleElementVectorToScalar
            || type == CastType::fixedSizeArrayToDynamicArray;
    }

    static bool canSilentlyCastTo (PrimitiveType dest, PrimitiveType source)
    {
        return isSilentCast (getCastType (dest, source));
    }

    static bool canSilentlyCastTo (const Type& dest, const Type& source)
    {
        return isSilentCast (getCastType (dest, source));
    }

    // allows literal constants to be silently cast as long as their value
    // can survive the conversion process
    static bool canSilentlyCastTo (const Type& dest, const Value& value)
    {
        const auto& type = value.getType();

        if (dest.isUnsizedArray())
            return false;

        if (canSilentlyCastTo (dest, type))
            return true;

        if (dest.isBoundedInt() && (type.isInteger() || type.isBoundedInt()))
            return dest.isValidBoundedIntIndex (value.getAsInt64());

        if (dest.isFloat32())
        {
            if (type.isFloat64())
                return static_cast<double> (value.getAsFloat()) == value.getAsDouble();

            if (type.isInteger())
                return value.getAsFloat() == static_cast<float> (value.getAsInt32());
        }

        if (dest.isInteger())
        {
            if (type.isFloat64())
                return value.getAsInt32() == value.getAsDouble();

            if (type.isFloat32())
                return static_cast<float> (value.getAsInt32()) == value.getAsFloat();
        }

        if (dest.isComplex32())
        {
            if (type.isFloat64())
                return static_cast<double> (value.getAsFloat()) == value.getAsDouble();

            if (type.isInteger())
                return value.getAsFloat() == static_cast<float> (value.getAsInt32());
        }

        return false;
    }

    static bool canPassAsArgumentTo (const Type& dest, const Type& source, bool mustBeExactMatch)
    {
        if (! (dest.isValid() && source.isValid()))
            return false;

        if (dest.isNonConstReference() && source.isConst())
            return false;

        if (dest.isUnsizedArray() && source.isArray())
            if (dest.getElementType().isIdentical (source.getElementType()))
                return true;

        if (dest.isEqual (source, Type::ignoreConst | Type::ignoreReferences | Type::ignoreVectorSize1))
            return true;

        if (! (mustBeExactMatch || dest.isReference()))
            if (canSilentlyCastTo (dest, source))
                return true;

        if (source.isBoundedInt() && dest.isPrimitiveInteger() && dest.getPrimitiveType() == source.getPrimitiveType())
            return true;

        return false;
    }

    static bool canBeConvertedAllowingFixedToUnsizedArrays (const Type& dest, const Type& source)
    {
        if (dest.hasIdenticalLayout (source))
            return true;

        if (dest.isUnsizedArray() && source.isArray())
            return dest.getElementType().hasIdenticalLayout (source.getElementType());

        if (dest.isStruct() && source.isStruct())
        {
            auto& destStruct = dest.getStructRef();
            auto& sourceStruct = source.getStructRef();

            if (destStruct.getNumMembers() == sourceStruct.getNumMembers())
            {
                for (size_t i = 0; i < sourceStruct.getNumMembers(); ++i)
                    if (! canBeConvertedAllowingFixedToUnsizedArrays (destStruct.getMemberType (i), sourceStruct.getMemberType (i)))
                        return false;

                return true;
            }
        }

        return false;
    }

    /** Used to hold the result type and the type to which all the operands must be coerced
        for a binary arithmetic operator.
    */
    struct BinaryOperatorTypes
    {
        Type resultType, operandType;
    };

    static bool isTypeSuitableForBinaryOp (const Type& t)
    {
        return ! (t.isStruct() || t.isArray() || t.isStringLiteral());
    }

    static bool areTypesSuitableForBinaryOp (const Type& a, const Type& b)
    {
        return isTypeSuitableForBinaryOp (a) && isTypeSuitableForBinaryOp (b);
    }

    static BinaryOperatorTypes getTypesForArithmeticOp (const Type& a, const Type& b, bool allowBoolOperands)
    {
        if (a.isReference())  return getTypesForArithmeticOp (a.removeReference(), b, allowBoolOperands);
        if (b.isReference())  return getTypesForArithmeticOp (a, b.removeReference(), allowBoolOperands);

        if (areTypesSuitableForBinaryOp (a, b))
        {
            if (! allowBoolOperands && (a.isBool() || b.isBool()))
                return {};

            if (a.isIdentical (b))
                return { a, a };

            // If either side is a bounded int, only allow the other side to be an integer
            if (a.isBoundedInt())  { if (b.isPrimitiveInteger()) return { b, b }; return {}; }
            if (b.isBoundedInt())  { if (a.isPrimitiveInteger()) return { a, a }; return {}; }

            if (canSilentlyCastTo (a, b)) return { a, a };
            if (canSilentlyCastTo (b, a)) return { b, b };

            // Allow silent promotion of ints to float32s
            if (a.isPrimitiveFloat() && b.isInteger()) return { a, a };
            if (b.isPrimitiveFloat() && a.isInteger()) return { b, b };

            // Allow silent promotion of ints to complex
            if (a.isPrimitiveComplex() && b.isInteger()) return { a, a };
            if (b.isPrimitiveComplex() && a.isInteger()) return { b, b };
        }

        return {};
    }

    static BinaryOperatorTypes getTypesForLogicalOp (const Type& a, const Type& b)
    {
        if (areTypesSuitableForBinaryOp (a, b) && a.getVectorSize() == b.getVectorSize())
            return { PrimitiveType::bool_, PrimitiveType::bool_ };

        return {};
    }

    static BinaryOperatorTypes getTypesForEqualityOp (const Type& a, const Type& b)
    {
        // Special case for string literals - they support ==, != but are unordered, so you can't do other comparisons
        if (a.isStringLiteral() && b.isStringLiteral())
            return { PrimitiveType::bool_, a };

        if (a.isComplex() || b.isComplex())
        {
            auto operandType = getTypesForArithmeticOp (a, b, true).operandType;

            if (operandType.isValid()
                && a.getVectorSize() == b.getVectorSize())
            {
                if (a.isVector() || b.isVector())
                    return { Type::createVector (PrimitiveType::bool_, a.getVectorSize()), operandType };

                return { PrimitiveType::bool_, operandType };
            }

            return {};
        }

        return getTypesForComparisonOp (a, b);
    }

    static BinaryOperatorTypes getTypesForComparisonOp (const Type& a, const Type& b)
    {
        if (a.isComplex() && b.isComplex())
            return {};

        if (a.isBoundedInt())   return getTypesForComparisonOp (PrimitiveType::int32, b);
        if (b.isBoundedInt())   return getTypesForComparisonOp (a, PrimitiveType::int32);

        auto operandType = getTypesForArithmeticOp (a, b, true).operandType;

        if (operandType.isValid()
             && a.getVectorSize() == b.getVectorSize())
        {
            if (a.isVector() || b.isVector())
                return { Type::createVector (PrimitiveType::bool_, a.getVectorSize()), operandType };

            return { PrimitiveType::bool_, operandType };
        }

        return {};
    }

    static bool isTypeSuitableForBitwiseOp (const Type& t)
    {
        return t.isInteger() && isTypeSuitableForBinaryOp (t);
    }

    static BinaryOperatorTypes getTypesForBitwiseOp (const Type& a, const Type& b)
    {
        if (a.isReference())  return getTypesForBitwiseOp (a.removeReference(), b);
        if (b.isReference())  return getTypesForBitwiseOp (a, b.removeReference());

        if (a.isBoundedInt())  return getTypesForBitwiseOp (PrimitiveType::int32, b.removeReferenceIfPresent());
        if (b.isBoundedInt())  return getTypesForBitwiseOp (a.removeReferenceIfPresent(), PrimitiveType::int32);

        if (isTypeSuitableForBitwiseOp (a)
             && isTypeSuitableForBitwiseOp (b)
             && a.getVectorSize() == b.getVectorSize()
             && a.isVector() == b.isVector())
        {
            auto intType = (a.isInteger64() || b.isInteger64()) ? PrimitiveType::int64 : PrimitiveType::int32;

            if (! a.isVector())
                return { intType, intType };

            auto vecType = Type::createVector (intType, a.getVectorSize());
            return { vecType, vecType };
        }

        return {};
    }

    template <typename Thrower, typename SizeType>
    static Type::ArraySize checkArraySizeAndThrowErrorIfIllegal (Thrower&& errorContext, SizeType size)
    {
        if (! Type::canBeSafelyCastToArraySize ((int64_t) size))
            errorContext.throwError (size > 0 ? Errors::tooManyElements()
                                              : Errors::illegalArraySize());

        return static_cast<Type::ArraySize> (size);
    }

    static bool arraySizeTypeIsOK (const Type& sizeType)
    {
        return (sizeType.isPrimitiveInteger() || sizeType.isBoundedInt()) && ! sizeType.isReference();
    }

    template <typename Thrower>
    static int64_t checkAndGetArrayIndex (Thrower&& errorContext, const Value& index)
    {
        if (! arraySizeTypeIsOK (index.getType()))
            errorContext.throwError (Errors::nonIntegerArrayIndex());

        return index.getAsInt64();
    }

    template <typename Thrower>
    static void checkConstantArrayIndex (Thrower&& errorContext, int64_t index, Type::ArraySize arraySize)
    {
        if (index < 0 || index >= (int64_t) arraySize)
            errorContext.throwError (Errors::indexOutOfRange());
    }

    template <typename Thrower>
    static Type::ArraySize checkAndGetArrayIndex (Thrower&& errorContext, const Value& index, const Type& arrayOrVectorType)
    {
        auto fixedIndex = checkAndGetArrayIndex (errorContext, index);

        if (arrayOrVectorType.isVector() || arrayOrVectorType.isFixedSizeArray())
        {
            auto i = arrayOrVectorType.convertArrayOrVectorIndexToValidRange (fixedIndex);
            checkConstantArrayIndex (errorContext, (int64_t) i, arrayOrVectorType.getArrayOrVectorSize());
            return i;
        }

        return (Type::ArraySize) fixedIndex;
    }

    template <typename Thrower>
    static Type::ArraySize checkAndGetArraySize (Thrower&& errorContext, const Value& size)
    {
        if (! arraySizeTypeIsOK (size.getType()))
            errorContext.throwError (Errors::nonIntegerArraySize());

        return checkArraySizeAndThrowErrorIfIllegal (errorContext, size.getAsInt64());
    }

    TypeRules() = delete;
};

} // namespace soul
