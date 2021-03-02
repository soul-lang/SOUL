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

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

namespace soul
{

//==============================================================================
namespace CompileTimeIntrinsicEvaluation
{
    template <typename ReturnType, typename ArgType>
    static Value perform (choc::span<Value> args, ReturnType (*fn)(ArgType))
    {
        SOUL_ASSERT (args.size() == 1); return Value (fn (static_cast<ArgType> (args.front())));
    }

    template <typename ReturnType, typename ArgType>
    static Value perform (choc::span<Value> args, ReturnType (*fn)(ArgType, ArgType))
    {
        SOUL_ASSERT (args.size() == 2); return Value (fn (static_cast<ArgType> (args[0]), static_cast<ArgType> (args[1])));
    }

    template <typename ReturnType, typename ArgType>
    static Value perform (choc::span<Value> args, ReturnType (*fn)(ArgType, ArgType, ArgType))
    {
        SOUL_ASSERT (args.size() == 3); return Value (fn (static_cast<ArgType> (args[0]), static_cast<ArgType> (args[1]), static_cast<ArgType> (args[2])));
    }

    static int64_t  abs_i          (int64_t n)                             { return std::abs (n); }
    static double   abs_d          (double n)                              { return std::abs (n); }
    static int64_t  min_i          (int64_t v1, int64_t v2)                { return std::min (v1, v2); }
    static double   min_d          (double v1, double v2)                  { return std::min (v1, v2); }
    static int64_t  max_i          (int64_t v1, int64_t v2)                { return std::max (v1, v2); }
    static double   max_d          (double v1, double v2)                  { return std::max (v1, v2); }
    static int64_t  clamp_i        (int64_t n, int64_t low, int64_t high)  { return n < low ? low : (n > high ? high : n); }
    static double   clamp_d        (double n, double low, double high)     { return n < low ? low : (n > high ? high : n); }
    static int64_t  wrap_i         (int64_t n, int64_t range)              { if (range == 0) return 0; n = n % range; if (n < 0) n += range; return n; }
    static double   wrap_d         (double n, double range)                { if (range == 0) return 0; n = std::fmod (n, range); if (n < 0) n += range; return n; }
    static double   fmod_d         (double a, double b)                    { return b != 0 ? std::fmod (a, b) : 0; }
    static double   remainder_d    (double a, double b)                    { return b != 0 ? std::remainder (a, b) : 0; }
    static double   floor_d        (double n)                              { return std::floor (n); }
    static double   ceil_d         (double n)                              { return std::ceil (n); }
    static double   addModulo2Pi_d (double v, double inc)                  { v += inc; if (v >= twoPi) v = remainder_d (v, twoPi); return v; }
    static double   sqrt_d         (double n)                              { return std::sqrt (n); }
    static double   pow_d          (double a, double b)                    { return std::pow (a, b); }
    static double   exp_d          (double n)                              { return std::exp (n); }
    static double   log_d          (double n)                              { return std::log (n); }
    static double   log10_d        (double n)                              { return std::log10 (n); }
    static double   sin_d          (double n)                              { return std::sin (n); }
    static double   cos_d          (double n)                              { return std::cos (n); }
    static double   tan_d          (double n)                              { return std::tan (n); }
    static double   acos_d         (double n)                              { return std::acos (n); }
    static double   asin_d         (double n)                              { return std::asin (n); }
    static double   atan_d         (double n)                              { return std::atan (n); }
    static double   sinh_d         (double n)                              { return std::sinh (n); }
    static double   cosh_d         (double n)                              { return std::cosh (n); }
    static double   tanh_d         (double n)                              { return std::tanh (n); }
    static double   asinh_d        (double n)                              { return std::asinh (n); }
    static double   acosh_d        (double n)                              { return std::acosh (n); }
    static double   atanh_d        (double n)                              { return std::atanh (n); }
    static double   atan2_d        (double a, double b)                    { return std::atan2 (a, b); }
    static bool     isnan_d        (double n)                              { return std::isnan (n); }
    static bool     isinf_d        (double n)                              { return std::isinf (n); }

    Value perform (IntrinsicType i, choc::span<Value> args, bool isFloat)
    {
        switch (i)
        {
            case IntrinsicType::none:                    SOUL_ASSERT_FALSE; break;
            case IntrinsicType::abs:                     return isFloat ? perform (args, abs_d)   : perform (args, abs_i);
            case IntrinsicType::min:                     return isFloat ? perform (args, min_d)   : perform (args, min_i);
            case IntrinsicType::max:                     return isFloat ? perform (args, max_d)   : perform (args, max_i);
            case IntrinsicType::clamp:                   return isFloat ? perform (args, clamp_d) : perform (args, clamp_i);
            case IntrinsicType::wrap:                    return isFloat ? perform (args, wrap_d)  : perform (args, wrap_i);
            case IntrinsicType::fmod:                    return perform (args, fmod_d);
            case IntrinsicType::remainder:               return perform (args, remainder_d);
            case IntrinsicType::floor:                   return perform (args, floor_d);
            case IntrinsicType::ceil:                    return perform (args, ceil_d);
            case IntrinsicType::addModulo2Pi:            return perform (args, addModulo2Pi_d);
            case IntrinsicType::sqrt:                    return perform (args, sqrt_d);
            case IntrinsicType::pow:                     return perform (args, pow_d);
            case IntrinsicType::exp:                     return perform (args, exp_d);
            case IntrinsicType::log:                     return perform (args, log_d);
            case IntrinsicType::log10:                   return perform (args, log10_d);
            case IntrinsicType::sin:                     return perform (args, sin_d);
            case IntrinsicType::cos:                     return perform (args, cos_d);
            case IntrinsicType::tan:                     return perform (args, tan_d);
            case IntrinsicType::sinh:                    return perform (args, sinh_d);
            case IntrinsicType::cosh:                    return perform (args, cosh_d);
            case IntrinsicType::tanh:                    return perform (args, tanh_d);
            case IntrinsicType::asinh:                   return perform (args, asinh_d);
            case IntrinsicType::acosh:                   return perform (args, acosh_d);
            case IntrinsicType::atanh:                   return perform (args, atanh_d);
            case IntrinsicType::asin:                    return perform (args, asin_d);
            case IntrinsicType::acos:                    return perform (args, acos_d);
            case IntrinsicType::atan:                    return perform (args, atan_d);
            case IntrinsicType::atan2:                   return perform (args, atan2_d);
            case IntrinsicType::isnan:                   return perform (args, isnan_d);
            case IntrinsicType::isinf:                   return perform (args, isinf_d);
            case IntrinsicType::roundToInt:              return {};
            case IntrinsicType::sum:                     return {};
            case IntrinsicType::product:                 return {};
            case IntrinsicType::get_array_size:          return {};
            case IntrinsicType::read:                    return {};
            case IntrinsicType::readLinearInterpolated:  return {};
        }

        return {};
    }
}

Value performIntrinsic (IntrinsicType i, choc::span<Value> args)
{
    auto argType = args.front().getType();

    for (auto& a : args)
    {
        const auto& t = a.getType();

        if (! (t.isPrimitiveInteger() || t.isPrimitiveFloat()))
            return {};

        if (! TypeRules::canPassAsArgumentTo (argType, t, false))
            argType = t;
    }

    ArrayWithPreallocation<Value, 4> castArgs;

    for (auto& a : args)
        castArgs.push_back (a.castToTypeExpectingSuccess (argType));

    auto result = CompileTimeIntrinsicEvaluation::perform (i, castArgs, argType.isFloatingPoint());

    if (! result.isValid())
        return {};

    if (result.getType().isBool())
        return result;

    return result.castToTypeExpectingSuccess (argType.withConstAndRefFlags (false, false));
}

#define SOUL_INTRINSICS(X) \
    X(abs) \
    X(min) \
    X(max) \
    X(clamp) \
    X(wrap) \
    X(fmod) \
    X(remainder) \
    X(floor) \
    X(ceil) \
    X(addModulo2Pi) \
    X(sqrt) \
    X(pow) \
    X(exp) \
    X(log) \
    X(log10) \
    X(sin) \
    X(cos) \
    X(tan) \
    X(sinh) \
    X(cosh) \
    X(tanh) \
    X(asinh) \
    X(acosh) \
    X(atanh) \
    X(asin) \
    X(acos) \
    X(atan) \
    X(atan2) \
    X(isnan) \
    X(isinf) \
    X(roundToInt) \
    X(sum) \
    X(product) \
    X(get_array_size) \
    X(read) \
    X(readLinearInterpolated) \

IntrinsicType getIntrinsicTypeFromName (std::string_view s)
{
    #define SOUL_MATCH_INTRINSIC(i)  if (s == #i) return IntrinsicType::i;
    SOUL_INTRINSICS (SOUL_MATCH_INTRINSIC)
    #undef SOUL_MATCH_INTRINSIC

    return IntrinsicType::none;
}

const char* getIntrinsicName (IntrinsicType target)
{
    #define SOUL_MATCH_INTRINSIC(i)  if (target == IntrinsicType::i) return #i;
    SOUL_INTRINSICS (SOUL_MATCH_INTRINSIC)
    #undef SOUL_MATCH_INTRINSIC

    SOUL_ASSERT_FALSE;
    return nullptr;
}

std::string getFullyQualifiedIntrinsicName (IntrinsicType intrinsic)
{
    return TokenisedPathString::join (getIntrinsicsNamespaceName(),
                                      getIntrinsicName (intrinsic));
}

static constexpr bool isUserCallable (IntrinsicType t)
{
    return t != IntrinsicType::none && t != IntrinsicType::get_array_size;
}

std::vector<std::string> getListOfCallableIntrinsicsAndConstants()
{
    std::vector<std::string> results;

    for (auto c : builtInConstants)
        results.push_back (std::string (c));

    #define SOUL_LIST_INTRINSIC(i)  if (isUserCallable (IntrinsicType::i)) results.push_back (#i);
    SOUL_INTRINSICS (SOUL_LIST_INTRINSIC)
    #undef SOUL_LIST_INTRINSIC

    return results;
}

} // namespace soul
