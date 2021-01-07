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
// NB: declaration order matters here for operators of different lengths that start the same way
#define SOUL_OPERATORS(X) \
    X(semicolon,          ";")              X(dot,                ".")     X(comma,        ",") \
    X(openParen,          "(")              X(closeParen,         ")") \
    X(openBrace,          "{")              X(closeBrace,         "}") \
    X(openDoubleBracket,  "[[")             X(closeDoubleBracket, "]]") \
    X(openBracket,        "[")              X(closeBracket,       "]") \
    X(doubleColon,        "::")             X(colon,              ":")     X(question,     "?") \
    X(equals,             "==")             X(assign,             "=") \
    X(notEquals,          "!=")             X(logicalNot,         "!") \
    X(plusEquals,         "+=")             X(plusplus,           "++")    X(plus,         "+") \
    X(minusEquals,        "-=")             X(minusminus,         "--")    X(rightArrow,   "->")   X(minus, "-") \
    X(timesEquals,        "*=")             X(times,              "*")   \
    X(divideEquals,       "/=")             X(divide,             "/")   \
    X(moduloEquals,       "%=")             X(modulo,             "%")   \
    X(xorEquals,          "^=")             X(bitwiseXor,         "^")   \
    X(bitwiseAndEquals,   "&=")             X(logicalAndEquals,   "&&=") \
    X(logicalAnd,         "&&")             X(bitwiseAnd,         "&")   \
    X(bitwiseOrEquals,    "|=")             X(logicalOrEquals,    "||=") \
    X(logicalOr,          "||")             X(bitwiseOr,          "|")   \
    X(bitwiseNot,         "~") \
    X(leftShiftEquals,    "<<=")            X(leftShift,          "<<") \
    X(lessThanOrEqual,    "<=")             X(lessThan,           "<") \
    X(rightShiftUnsignedEquals, ">>>=")     X(rightShiftEquals,   ">>=") \
    X(rightShiftUnsigned, ">>>")            X(rightShift,         ">>") \
    X(greaterThanOrEqual, ">=")             X(greaterThan,        ">")

//==============================================================================
/** Deals with types and compile-time execution of unary operations */
namespace UnaryOp
{
    #define SOUL_UNARY_OPS(X) \
        X(negate,      -) \
        X(logicalNot,  !) \
        X(bitwiseNot,  ~) \

    enum class Op
    {
        #define SOUL_DECLARE_UNARY_OP(name, op)    name,
        SOUL_UNARY_OPS (SOUL_DECLARE_UNARY_OP)
        #undef SOUL_DECLARE_UNARY_OP
        unknown
    };

    static inline const char* getSymbol (Op o) noexcept
    {
        #define SOUL_GET_UNARY_OP_NAME(name, op)   if (o == Op::name) return #op;
        SOUL_UNARY_OPS (SOUL_GET_UNARY_OP_NAME)
        #undef SOUL_GET_UNARY_OP_NAME
        SOUL_ASSERT_FALSE;
        return "";
    }

    inline bool isTypeSuitable (Op op, const Type& type)
    {
        if (! type.isPrimitiveOrVector())
            return false;

        if (op == Op::negate)       return type.isInteger() || type.isFloatingPoint() || type.isComplex();
        if (op == Op::bitwiseNot)   return type.isInteger();
        if (op == Op::logicalNot)   return type.isBool();

        SOUL_ASSERT_FALSE;
        return false;
    }

    inline bool apply (Value& value, Op op)
    {
        if (! isTypeSuitable (op, value.getType()))
            return false;

        if (op == Op::negate)
        {
            value = value.negated();
        }
        else if (op == Op::bitwiseNot)
        {
            if (value.getType().isInteger32())
                value = Value::createInt32 (~value.getAsInt32());
            else
                value = Value::createInt64 (~value.getAsInt64());
        }
        else if (op == Op::logicalNot)
        {
            value = Value (value.getAsDouble() == 0);
        }

        return true;
    }
}

//==============================================================================
/** Deals with types and compile-time execution of binary operations */
namespace BinaryOp
{
    #define SOUL_BINARY_OPS(X) \
        X(add,                  +) \
        X(subtract,             -) \
        X(multiply,             *) \
        X(divide,               /) \
        X(modulo,               %) \
        X(bitwiseOr,            |) \
        X(bitwiseAnd,           &) \
        X(bitwiseXor,           ^) \
        X(logicalOr,            ||) \
        X(logicalAnd,           &&) \
        X(equals,               ==) \
        X(notEquals,            !=) \
        X(lessThan,             <)  \
        X(lessThanOrEqual,      <=) \
        X(greaterThan,          >)  \
        X(greaterThanOrEqual,   >=) \
        X(leftShift,            <<) \
        X(rightShift,           >>) \
        X(rightShiftUnsigned,   >>>) \

    enum class Op
    {
        #define SOUL_DECLARE_BINARY_OP(name, op)   name,
        SOUL_BINARY_OPS (SOUL_DECLARE_BINARY_OP)
        #undef SOUL_DECLARE_BINARY_OP
        unknown
    };

    static inline const char* getSymbol (Op o) noexcept
    {
        #define SOUL_GET_BINARY_OP_NAME(name, op)  if (o == Op::name) return #op;
        SOUL_BINARY_OPS (SOUL_GET_BINARY_OP_NAME)
        #undef SOUL_GET_BINARY_OP_NAME
        SOUL_ASSERT_FALSE;
        return "";
    }

    inline bool isLogicalOperator (Op t) noexcept
    {
        return t == Op::logicalAnd || t == Op::logicalOr;
    }

    inline bool isEqualityOperator (Op t) noexcept
    {
        return t == Op::equals      || t == Op::notEquals;
    }

    inline bool isComparisonOperator (Op t) noexcept
    {
        return t == Op::lessThan    || t == Op::lessThanOrEqual
            || t == Op::greaterThan || t == Op::greaterThanOrEqual;
    }

    inline bool isBitwiseOperator (Op t) noexcept
    {
        return t == Op::bitwiseOr  || t == Op::bitwiseAnd || t == Op::bitwiseXor
            || t == Op::leftShift  || t == Op::rightShift || t == Op::rightShiftUnsigned;
    }

    inline bool isArithmeticOperator (Op t) noexcept
    {
        return t == Op::add       || t == Op::subtract
            || t == Op::multiply  || t == Op::divide
            || t == Op::modulo;
    }

    inline TypeRules::BinaryOperatorTypes getTypes (Op op, const Type& a, const Type& b)
    {
        if (isLogicalOperator (op))       return TypeRules::getTypesForLogicalOp (a, b);
        if (isBitwiseOperator (op))       return TypeRules::getTypesForBitwiseOp (a, b);
        if (isEqualityOperator (op))      return TypeRules::getTypesForEqualityOp (a, b);
        if (isComparisonOperator (op))    return TypeRules::getTypesForComparisonOp (a, b);
        if (isArithmeticOperator (op))    return TypeRules::getTypesForArithmeticOp (a, b, false);

        SOUL_ASSERT_FALSE;
        return {};
    }

    // Returns 0 for "unknown", -1 for "always false", 1 for "always true"
    inline int getResultOfComparisonWithBoundedType (Op op, const Type& a, const Value& b)
    {
        SOUL_ASSERT (isComparisonOperator (op));

        if (a.isBoundedInt() && TypeRules::canSilentlyCastTo (PrimitiveType::int64, b.getType()))
        {
            auto getZone = [] (int64_t rangeStart, int64_t rangeEnd, int64_t value) -> int
            {
                return value < rangeStart ? -1 : (value > rangeEnd ? 1 : 0);
            };

            auto limit = a.getBoundedIntLimit();
            auto constant = b.getAsInt64();

            if (op == Op::lessThan)             return  getZone (1, limit - 1, constant);
            if (op == Op::lessThanOrEqual)      return  getZone (0, limit - 2, constant);
            if (op == Op::greaterThan)          return -getZone (0, limit - 1, constant);
            if (op == Op::greaterThanOrEqual)   return -getZone (1, limit - 2, constant);
        }

        return 0;
    }

    // Returns 0 for "unknown", -1 for "always false", 1 for "always true"
    inline int getResultOfComparisonWithBoundedType (Op op, const Value& a, const Type& b)
    {
        if (op == Op::lessThan)             return getResultOfComparisonWithBoundedType (Op::greaterThanOrEqual,  b, a);
        if (op == Op::lessThanOrEqual)      return getResultOfComparisonWithBoundedType (Op::greaterThan,         b, a);
        if (op == Op::greaterThan)          return getResultOfComparisonWithBoundedType (Op::lessThanOrEqual,     b, a);
        if (op == Op::greaterThanOrEqual)   return getResultOfComparisonWithBoundedType (Op::lessThan,            b, a);

        return 0;
    }

    template <typename Type, typename HandleError>
    bool checkDivideByZero (Type n, HandleError&& handleError)
    {
        if (n != 0)
            return true;

        handleError (Errors::divideByZero());
        return false;
    }

    template <typename Type, typename HandleError>
    bool checkModuloZero (Type n, HandleError&& handleError)
    {
        if (n != 0)
            return true;

        handleError (Errors::moduloZero());
        return false;
    }

    template <typename IntType, typename HandleError>
    bool SOUL_NO_SIGNED_INTEGER_OVERFLOW_WARNING applyInt (Value& lhs, IntType a, IntType b, Op op, HandleError&& handleError)
    {
        if (op == Op::add)                { lhs = Value (a + b);  return true; }
        if (op == Op::subtract)           { lhs = Value (a - b);  return true; }
        if (op == Op::multiply)           { lhs = Value (a * b);  return true; }
        if (op == Op::bitwiseOr)          { lhs = Value (a | b);  return true; }
        if (op == Op::bitwiseAnd)         { lhs = Value (a & b);  return true; }
        if (op == Op::bitwiseXor)         { lhs = Value (a ^ b);  return true; }
        if (op == Op::logicalOr)          { lhs = Value (a || b); return true; }
        if (op == Op::logicalAnd)         { lhs = Value (a && b); return true; }

        if (op == Op::divide)
        {
            if (! checkDivideByZero (b, handleError))
                return false;

            lhs = Value (a / b);
            return true;
        }

        if (op == Op::modulo)
        {
            if (! checkModuloZero (b, handleError))
                return false;

            lhs = Value (a % b);
            return true;
        }

        if (op == Op::leftShift)
        {
            lhs = (b >= 0) ? Value ((IntType) (((uint64_t) a) << b))
                           : Value::zeroInitialiser (lhs.getType());
            return true;
        }

        if (op == Op::rightShift)
        {
            if (b >= 0)
                lhs = Value (a >> b);
            else
                lhs = (a >= 0) ? Value::zeroInitialiser (lhs.getType())
                               : Value (-1);
            return true;
        }

        if (op == Op::rightShiftUnsigned)
        {
            if (sizeof (a) == sizeof (int32_t))
                lhs = (b >= 0) ? Value ((IntType) (((uint32_t) a) >> b))
                               : Value::zeroInitialiser (lhs.getType());
            else
                lhs = (b >= 0) ? Value ((IntType) (((uint64_t) a) >> b))
                               : Value::zeroInitialiser (lhs.getType());
            return true;
        }

        if (op == Op::lessThan)             { lhs = Value (a <  b); return true; }
        if (op == Op::lessThanOrEqual)      { lhs = Value (a <= b); return true; }
        if (op == Op::greaterThan)          { lhs = Value (a >  b); return true; }
        if (op == Op::greaterThanOrEqual)   { lhs = Value (a >= b); return true; }

        return false;
    }

    template <typename HandleError>
    inline bool apply (Value& lhs, Value rhs, Op op, HandleError&& handleError)
    {
        auto types = getTypes (op, lhs.getType(), rhs.getType());

        lhs = lhs.tryCastToType (types.operandType);
        rhs = rhs.tryCastToType (types.operandType);

        if (! (lhs.isValid() && rhs.isValid()))
            return {};

        if (op == Op::equals)              { lhs = Value (lhs == rhs); return true; }
        if (op == Op::notEquals)           { lhs = Value (lhs != rhs); return true; }

        if (lhs.getType().isVector())
        {
            auto size = lhs.getType().getVectorSize();

            for (size_t i = 0; i < size; ++i)
            {
                auto e = lhs.getSubElement (i);

                if (! apply (e, rhs.getSubElement (i), op, handleError))
                    return false;

                lhs.modifySubElementInPlace (i, e);
            }

            return true;
        }

        if (lhs.getType().isComplex32() || rhs.getType().isComplex32())
        {
            auto a = lhs.getAsComplex32();
            auto b = rhs.getAsComplex32();

            if (op == Op::add)                  { lhs = Value (a + b); return true; }
            if (op == Op::subtract)             { lhs = Value (a - b); return true; }
            if (op == Op::multiply)             { lhs = Value (a * b); return true; }
            if (op == Op::divide)               { lhs = Value (a / b); return true; }

            return {};
        }

        if (lhs.getType().isComplex64() || rhs.getType().isComplex64())
        {
            auto a = lhs.getAsComplex64();
            auto b = rhs.getAsComplex64();

            if (op == Op::add)                  { lhs = Value (a + b); return true; }
            if (op == Op::subtract)             { lhs = Value (a - b); return true; }
            if (op == Op::multiply)             { lhs = Value (a * b); return true; }
            if (op == Op::divide)               { lhs = Value (a / b); return true; }

            return {};
        }

        if (lhs.getType().isFloat64() || rhs.getType().isFloat64())
        {
            auto a = lhs.getAsDouble();
            auto b = rhs.getAsDouble();

            if (op == Op::add)                  { lhs = Value (a +  b); return true; }
            if (op == Op::subtract)             { lhs = Value (a -  b); return true; }
            if (op == Op::multiply)             { lhs = Value (a *  b); return true; }
            if (op == Op::lessThan)             { lhs = Value (a <  b); return true; }
            if (op == Op::lessThanOrEqual)      { lhs = Value (a <= b); return true; }
            if (op == Op::greaterThan)          { lhs = Value (a >  b); return true; }
            if (op == Op::greaterThanOrEqual)   { lhs = Value (a >= b); return true; }

            if (op == Op::divide)
            {
                if (! checkDivideByZero (b, handleError))
                    return false;

                lhs = Value (a /  b);
                return true;
            }

            if (op == Op::modulo)
            {
                if (! checkModuloZero (b, handleError))
                    return false;

                lhs = Value (std::fmod (a, b));
                return true;
            }
        }

        if (lhs.getType().isFloat32() || rhs.getType().isFloat32())
        {
            auto a = lhs.getAsDouble();
            auto b = rhs.getAsDouble();

            if (op == Op::add)                  { lhs = Value ((float) (a + b)); return true; }
            if (op == Op::subtract)             { lhs = Value ((float) (a - b)); return true; }
            if (op == Op::multiply)             { lhs = Value ((float) (a * b)); return true; }
            if (op == Op::lessThan)             { lhs = Value (a <  b); return true; }
            if (op == Op::lessThanOrEqual)      { lhs = Value (a <= b); return true; }
            if (op == Op::greaterThan)          { lhs = Value (a >  b); return true; }
            if (op == Op::greaterThanOrEqual)   { lhs = Value (a >= b); return true; }

            if (op == Op::divide)
            {
                if (! checkDivideByZero (b, handleError))
                    return false;

                lhs = Value ((float) (a / b));
                return true;
            }

            if (op == Op::modulo)
            {
                if (! checkModuloZero (b, handleError))
                    return false;

                lhs = Value ((float) std::fmod (a, b));
                return true;
            }
        }

        if (lhs.getType().isInteger64() || rhs.getType().isInteger64())
            return applyInt (lhs, lhs.getAsInt64(), rhs.getAsInt64(), op, handleError);

        return applyInt (lhs, lhs.getAsInt32(), rhs.getAsInt32(), op, handleError);
    }
}

} // namespace soul
