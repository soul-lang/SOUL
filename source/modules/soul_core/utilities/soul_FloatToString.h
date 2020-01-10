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

#ifdef _MSC_VER
 #include <intrin.h>
 #pragma intrinsic (_umul128)

 #ifdef _WIN64
  #pragma intrinsic (_BitScanReverse64)
 #else
  #pragma intrinsic (_BitScanReverse)
 #endif
#endif

namespace soul
{

/**
    Fast, round-trip-safe function for printing 32- or 64-bit floating point numbers.

    This is a concise, dependency-free implementation of the Grisu3 algorithm as described
    in the paper "Printing Floating-Point Numbers Quickly and Accurately with Integers"
    by Florian Loitsch.

    I've tried to use sensible variable names where possible, but most of the unhelpful
    ones like K, w, kappa, etc are taken from the original paper, where they're attached
    to some rather hard-to-describe concepts.
*/
template <typename FloatType>
struct FloatToString
{
    /** Writes a floating point number into a buffer.
        @param value    the number to print
        @param buffer   the buffer to print it into - the size of this should be at
                        least maxBufferSizeNeeded chars
        @praam maxDecimalPlaces if this is greater than zero, it specifies the maximum
                        number of decimal places to write. If less than zero, it uses
                        maximum precision.
        @returns a pointer to the end of the string that is produced
    */
    static char* write (FloatType value, char* buffer, int maxDecimalPlaces = -1)
    {
        auto asInt = bitCastToInt (value);

        if (isZero (asInt))
            return writeZero (isNegative (asInt) ? write (buffer, '-') : buffer);

        if ((asInt & signMask) != 0)
        {
            value = -value;
            asInt &= ~signMask;
            buffer = write (buffer, '-');
        }

        if (asInt == nanBits)  return write (buffer, 'n', 'a', 'n');
        if (asInt == infBits)  return write (buffer, 'i', 'n', 'f');

        auto v = MantissaAndExponent::fromFloat (value);
        auto boundaries = getNormalizedBoundaries (v);

        int K;
        auto power10 = MantissaAndExponent::getPowerOf10 (boundaries.plus.exponent, K);
        auto w = power10 * v.getNormalized();
        auto upperBound = power10 * boundaries.plus;
        upperBound.mantissa--;
        auto lowerBound = power10 * boundaries.minus;
        lowerBound.mantissa++;

        auto length = generateDigits (upperBound, upperBound.mantissa - w.mantissa, upperBound.mantissa - lowerBound.mantissa, buffer, K);
        return applyBestFormat (buffer, length, K, maxDecimalPlaces < 0 ? 324 : maxDecimalPlaces);
    }

    static constexpr size_t maxBufferSizeNeeded = 32;

private:
    struct MantissaAndExponent
    {
        constexpr MantissaAndExponent (uint64_t m, int e) : mantissa (m), exponent (e) {}

        static MantissaAndExponent fromFloat (FloatType d)
        {
            auto asInt = bitCastToInt (d);
            auto significand = (asInt & significandMask);

            if (auto biasedExponent = static_cast<int> ((asInt & exponentMask) >> numSignificandBits))
                return { significand + hiddenBit, biasedExponent - exponentBias };

            return { significand, 1 - exponentBias };
        }

        static MantissaAndExponent getPowerOf10 (int exponentBase2, int& K)
        {
            static constexpr MantissaAndExponent powersOf10[] =
            {
                { 0xfa8fd5a0081c0288ull, -1220 },  { 0xbaaee17fa23ebf76ull, -1193 },  { 0x8b16fb203055ac76ull, -1166 },  { 0xcf42894a5dce35eaull, -1140 },
                { 0x9a6bb0aa55653b2dull, -1113 },  { 0xe61acf033d1a45dfull, -1087 },  { 0xab70fe17c79ac6caull, -1060 },  { 0xff77b1fcbebcdc4full, -1034 },
                { 0xbe5691ef416bd60cull, -1007 },  { 0x8dd01fad907ffc3cull,  -980 },  { 0xd3515c2831559a83ull,  -954 },  { 0x9d71ac8fada6c9b5ull,  -927 },
                { 0xea9c227723ee8bcbull,  -901 },  { 0xaecc49914078536dull,  -874 },  { 0x823c12795db6ce57ull,  -847 },  { 0xc21094364dfb5637ull,  -821 },
                { 0x9096ea6f3848984full,  -794 },  { 0xd77485cb25823ac7ull,  -768 },  { 0xa086cfcd97bf97f4ull,  -741 },  { 0xef340a98172aace5ull,  -715 },
                { 0xb23867fb2a35b28eull,  -688 },  { 0x84c8d4dfd2c63f3bull,  -661 },  { 0xc5dd44271ad3cdbaull,  -635 },  { 0x936b9fcebb25c996ull,  -608 },
                { 0xdbac6c247d62a584ull,  -582 },  { 0xa3ab66580d5fdaf6ull,  -555 },  { 0xf3e2f893dec3f126ull,  -529 },  { 0xb5b5ada8aaff80b8ull,  -502 },
                { 0x87625f056c7c4a8bull,  -475 },  { 0xc9bcff6034c13053ull,  -449 },  { 0x964e858c91ba2655ull,  -422 },  { 0xdff9772470297ebdull,  -396 },
                { 0xa6dfbd9fb8e5b88full,  -369 },  { 0xf8a95fcf88747d94ull,  -343 },  { 0xb94470938fa89bcfull,  -316 },  { 0x8a08f0f8bf0f156bull,  -289 },
                { 0xcdb02555653131b6ull,  -263 },  { 0x993fe2c6d07b7facull,  -236 },  { 0xe45c10c42a2b3b06ull,  -210 },  { 0xaa242499697392d3ull,  -183 },
                { 0xfd87b5f28300ca0eull,  -157 },  { 0xbce5086492111aebull,  -130 },  { 0x8cbccc096f5088ccull,  -103 },  { 0xd1b71758e219652cull,   -77 },
                { 0x9c40000000000000ull,   -50 },  { 0xe8d4a51000000000ull,   -24 },  { 0xad78ebc5ac620000ull,     3 },  { 0x813f3978f8940984ull,    30 },
                { 0xc097ce7bc90715b3ull,    56 },  { 0x8f7e32ce7bea5c70ull,    83 },  { 0xd5d238a4abe98068ull,   109 },  { 0x9f4f2726179a2245ull,   136 },
                { 0xed63a231d4c4fb27ull,   162 },  { 0xb0de65388cc8ada8ull,   189 },  { 0x83c7088e1aab65dbull,   216 },  { 0xc45d1df942711d9aull,   242 },
                { 0x924d692ca61be758ull,   269 },  { 0xda01ee641a708deaull,   295 },  { 0xa26da3999aef774aull,   322 },  { 0xf209787bb47d6b85ull,   348 },
                { 0xb454e4a179dd1877ull,   375 },  { 0x865b86925b9bc5c2ull,   402 },  { 0xc83553c5c8965d3dull,   428 },  { 0x952ab45cfa97a0b3ull,   455 },
                { 0xde469fbd99a05fe3ull,   481 },  { 0xa59bc234db398c25ull,   508 },  { 0xf6c69a72a3989f5cull,   534 },  { 0xb7dcbf5354e9beceull,   561 },
                { 0x88fcf317f22241e2ull,   588 },  { 0xcc20ce9bd35c78a5ull,   614 },  { 0x98165af37b2153dfull,   641 },  { 0xe2a0b5dc971f303aull,   667 },
                { 0xa8d9d1535ce3b396ull,   694 },  { 0xfb9b7cd9a4a7443cull,   720 },  { 0xbb764c4ca7a44410ull,   747 },  { 0x8bab8eefb6409c1aull,   774 },
                { 0xd01fef10a657842cull,   800 },  { 0x9b10a4e5e9913129ull,   827 },  { 0xe7109bfba19c0c9dull,   853 },  { 0xac2820d9623bf429ull,   880 },
                { 0x80444b5e7aa7cf85ull,   907 },  { 0xbf21e44003acdd2dull,   933 },  { 0x8e679c2f5e44ff8full,   960 },  { 0xd433179d9c8cb841ull,   986 },
                { 0x9e19db92b4e31ba9ull,  1013 },  { 0xeb96bf6ebadf77d9ull,  1039 },  { 0xaf87023b9bf0ee6bull,  1066 }
            };

            auto dk = (-61 - exponentBase2) * 0.30102999566398114;
            auto ik = static_cast<int> (dk);
            auto index = ((ik + (dk > ik ? 348 : 347)) >> 3) + 1;
            K = 348 - (index << 3);
            return powersOf10[index];
        }

        MantissaAndExponent operator* (MantissaAndExponent other) const
        {
            uint64_t high;
            auto low = multiply128 (mantissa, other.mantissa, high);
            return { high + (low >> 63), exponent + other.exponent + 64 };
        }

        MantissaAndExponent getNormalized() const
        {
            auto s = countUpperClearBits (mantissa);
            return { mantissa << s, exponent - s };
        }

        uint64_t mantissa;
        int exponent;
    };

    static int generateDigits (MantissaAndExponent upperBound, uint64_t mantissaDiff, uint64_t delta, char* buffer, int& K)
    {
        static constexpr uint32_t powersOf10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };
        auto one = MantissaAndExponent { 1ull << -upperBound.exponent, upperBound.exponent };
        auto p1 = static_cast<uint32_t> (upperBound.mantissa >> -one.exponent);
        auto p2 = upperBound.mantissa & (one.mantissa - 1);
        auto kappa = countDecimalDigits (p1);
        int length = 0;

        auto roundAndWeed = [&buffer, &length, &delta] (uint64_t rest, uint64_t tenToPowerKappa, uint64_t diff)
        {
            while (rest < diff && delta - rest >= tenToPowerKappa
                    && (rest + tenToPowerKappa < diff || diff - rest > rest + tenToPowerKappa - diff))
            {
                --(buffer[length - 1]);
                rest += tenToPowerKappa;
            }
        };

        while (kappa > 0)
        {
            auto digit = p1;

            switch (kappa--)
            {
                case 1:                         p1 = 0;           break;
                case 2:  digit /= 10;           p1 %= 10;         break;
                case 3:  digit /= 100;          p1 %= 100;        break;
                case 4:  digit /= 1000;         p1 %= 1000;       break;
                case 5:  digit /= 10000;        p1 %= 10000;      break;
                case 6:  digit /= 100000;       p1 %= 100000;     break;
                case 7:  digit /= 1000000;      p1 %= 1000000;    break;
                case 8:  digit /= 10000000;     p1 %= 10000000;   break;
                case 9:  digit /= 100000000;    p1 %= 100000000;  break;
                default: break;
            }

            writeIfNotLeadingZero (buffer, length, digit);
            auto n = p2 + (static_cast<uint64_t> (p1) << -one.exponent);

            if (n <= delta)
            {
                roundAndWeed (n, static_cast<uint64_t> (powersOf10[kappa]) << -one.exponent, mantissaDiff);
                K += kappa;
                return length;
            }
        }

        for (;;)
        {
            --kappa;
            p2 *= 10;
            delta *= 10;
            writeIfNotLeadingZero (buffer, length, static_cast<uint32_t> (p2 >> -one.exponent));
            p2 &= one.mantissa - 1;

            if (p2 < delta)
            {
                roundAndWeed (p2, one.mantissa, kappa > -9 ? mantissaDiff * powersOf10[-kappa] : 0);
                K += kappa;
                return length;
            }
        }
    }

    static char* write (char* buffer, char c)                                               { *buffer = c; return buffer + 1; }
    template <typename... Args>  static char* write (char* buffer, char c, Args... others)  { return write (write (buffer, c), others...); }
    static char* writeZero (char* buffer)                                                   { return write (buffer, '0', '.', '0'); }
    template <typename IntType> static char decimalDigitChar (IntType digit)                { return static_cast<char> ('0' + digit); }
    static void writeIfNotLeadingZero (char* buffer, int& length, uint32_t digit)           { if (digit != 0 || length != 0) buffer[length++] = decimalDigitChar (digit); }

    static void insert (char* buffer, int bufferLength, char c, int num)
    {
        std::memmove (buffer + num, buffer, (size_t) bufferLength);

        for (int i = 0; i < num; ++i)
            buffer[i] = c;
    }

    static char* writeExponent (char* buffer, int e)
    {
        buffer = write (buffer, 'e');

        if (e < 0)
        {
            e = -e;
            buffer = write (buffer, '-');
        }

        if (e >= 100) return write (buffer, decimalDigitChar (e / 100), decimalDigitChar ((e / 10) % 10), decimalDigitChar (e % 10));
        if (e >= 10)  return write (buffer, decimalDigitChar (e / 10),  decimalDigitChar (e % 10));

        return write (buffer, decimalDigitChar (e));
    }

    static char* writeWithExponentNotation (char* buffer, int length, int fullLength)
    {
        insert (buffer + 1, length - 1, '.', 1);

        while (buffer[length] == '0' && length > 2)
            --length;

        return writeExponent (buffer + (length + 1), fullLength - 1);
    }

    static char* writeLargeNumberWithoutExponent (char* buffer, int length, int fullLength, int K, int maxDecimalPlaces)
    {
        if (K >= 0)
        {
            buffer += length;

            for (int i = length; i < fullLength; ++i)
                buffer = write (buffer, '0');

            return write (buffer, '.', '0');
        }

        insert (buffer + fullLength, length - fullLength, '.', 1);

        if (K + maxDecimalPlaces >= 0)
            return buffer + (length + 1);

        for (int i = fullLength + maxDecimalPlaces; i > fullLength + 1; --i)
            if (buffer[i] != '0')
                return buffer + (i + 1);

        return buffer + (fullLength + 2);
    }

    static char* writeSmallNumberWithoutExponent (char* buffer, int length, int fullLength, int maxDecimalPlaces)
    {
        auto extraPaddingNeeded = 2 - fullLength;
        insert (buffer, length, '0', extraPaddingNeeded);
        buffer[1] = '.';

        if (length - fullLength > maxDecimalPlaces)
        {
            for (int i = maxDecimalPlaces + 1; i > 2; --i)
                if (buffer[i] != '0')
                    return buffer + (i + 1);

            return buffer + 3;
        }

        length += extraPaddingNeeded;

        while (buffer[length - 1] == '0' && length > 3)
            --length;

        return buffer + length;
    }

    static char* applyBestFormat (char* buffer, int length, int K, int maxDecimalPlaces)
    {
        auto fullLength = length + K;

        if (fullLength > 0 && fullLength <= 21)  return writeLargeNumberWithoutExponent (buffer, length, fullLength, K, maxDecimalPlaces);
        if (fullLength <= 0 && fullLength > -6)  return writeSmallNumberWithoutExponent (buffer, length, fullLength, maxDecimalPlaces);
        if (fullLength < -maxDecimalPlaces)      return writeZero (buffer);
        if (length == 1)                         return writeExponent (buffer + 1, fullLength - 1);

        return writeWithExponentNotation (buffer, length, fullLength);
    }

    static uint64_t bitCastToInt (double value) noexcept    { uint64_t i; memcpy (&i, &value, sizeof (i)); return i; }
    static uint64_t bitCastToInt (float value) noexcept     { uint32_t i; memcpy (&i, &value, sizeof (i)); return i; }
    static bool isNegative (uint64_t floatAsInt) noexcept   { return (floatAsInt & signMask) != 0; }
    static bool isZero (uint64_t floatAsInt) noexcept       { return (floatAsInt & (exponentMask | significandMask)) == 0; }

    struct Boundaries { MantissaAndExponent minus, plus; };

    static Boundaries getNormalizedBoundaries (MantissaAndExponent value)
    {
        auto plus  = getNormalizedBoundary ({ (value.mantissa << 1) + 1, value.exponent - 1 });
        auto minus = value.mantissa == hiddenBit ? MantissaAndExponent { (value.mantissa << 2) - 1, value.exponent - 2 }
                                                 : MantissaAndExponent { (value.mantissa << 1) - 1, value.exponent - 1 };
        minus.mantissa <<= minus.exponent - plus.exponent;
        minus.exponent = plus.exponent;

        return { minus, plus };
    }

    static MantissaAndExponent getNormalizedBoundary (MantissaAndExponent value)
    {
        while ((value.mantissa & (hiddenBit << 1)) == 0)
        {
            value.mantissa <<= 1;
            value.exponent--;
        }

        static constexpr auto significandShiftNeeded = (int) (sizeof (value.mantissa) * 8 - numSignificandBits - 2);
        value.mantissa <<= significandShiftNeeded;
        value.exponent -=  significandShiftNeeded;
        return value;
    }

    static int countDecimalDigits (uint32_t n) noexcept
    {
        return n < 10 ? 1
             : n < 100 ? 2
             : n < 1000 ? 3
             : n < 10000 ? 4
             : n < 100000 ? 5
             : n < 1000000 ? 6
             : n < 10000000 ? 7
             : n < 100000000 ? 8 : 9;
    }

    static int countUpperClearBits (uint64_t n)
    {
       #ifdef _WIN64
        unsigned long count = 0;
        _BitScanReverse64 (&count, n);
        return 63 - (int) count;
       #elif defined (_MSC_VER)
        unsigned long count = 0;
        if (_BitScanReverse (&count, static_cast<unsigned long> (n >> 32)))
            return 31 - (int) count;

        _BitScanReverse (&count, static_cast<unsigned long> (n));
        return 63 - (int) count;
       #else
        return __builtin_clzll (n);
       #endif
    }

    static uint64_t multiply128 (uint64_t a, uint64_t b, uint64_t& high)
    {
       #ifdef _MSC_VER
        return _umul128 (a, b, &high);
       #elif __LP64__
        auto result = static_cast<unsigned __int128> (a) * static_cast<unsigned __int128> (b);
        high = static_cast<uint64_t> (result >> 64);
        return static_cast<uint64_t> (result);
       #else
        uint64_t a0 = static_cast<uint32_t> (a), a1 = a >> 32,
                 b0 = static_cast<uint32_t> (b), b1 = b >> 32;
        auto p10 = a1 * b0, p00 = a0 * b0, p11 = a1 * b1, p01 = a0 * b1;
        auto middle = p10 + static_cast<uint32_t> (p01) + (p00 >> 32);
        high = p11 + (middle >> 32) + (p01 >> 32);
        return (middle << 32) | static_cast<uint32_t> (p00);
       #endif
    }

    static constexpr int      numSignificandBits = sizeof (FloatType) == 8 ? 52 : 23;
    static constexpr uint64_t signMask           = 1ull << (sizeof (FloatType) * 8 - 1);
    static constexpr uint64_t hiddenBit          = 1ull << numSignificandBits;
    static constexpr uint64_t significandMask    = hiddenBit - 1;
    static constexpr uint64_t exponentMask       = (sizeof (FloatType) == 8 ? 0x7ffull : 0xffull) << numSignificandBits;
    static constexpr int      exponentBias       = (sizeof (FloatType) == 8 ? 0x3ff : 0x7f) + numSignificandBits;
    static constexpr uint64_t nanBits            = sizeof (FloatType) == 8 ? 0x7ff8000000000000ull : 0x7fc00000ull;
    static constexpr uint64_t infBits            = sizeof (FloatType) == 8 ? 0x7ff0000000000000ull : 0x7f800000ull;
};

} // namespace soul
