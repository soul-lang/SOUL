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
/** Used by Tokeniser to represent a token type */
struct TokenType  final
{
    constexpr TokenType() = default;
    explicit constexpr TokenType (const char* t) : text (t) {}

    const char* text = nullptr;

    bool operator== (const TokenType& other) const noexcept
    {
        // unfortunately we need to do a strcmp when the pointers are different here,
        // because the compiler *may* have generated multiple instances of the statics
        return (text == other.text)
                || (text != nullptr && other.text != nullptr && operator== (other.text));
    }

    bool operator== (const char* other) const noexcept          { return std::strcmp (text, other) == 0; }
    bool operator!= (const TokenType& other) const noexcept     { return ! operator== (other); }
    bool operator!= (const char* other) const noexcept          { return ! operator== (other); }

    operator bool() const noexcept                              { return text != nullptr; }
    operator std::string() const                                { return getDescription(); }

    std::string getDescription() const
    {
        return text[0] == '$' ? std::string (text + 1)
                              : choc::text::addDoubleQuotes (text);
    }
};

#define SOUL_DECLARE_TOKEN(name, str)  static constexpr TokenType name { str };

/** Some standard token types used when parsing both SOUL and HEART */
namespace Token
{
    SOUL_DECLARE_TOKEN (eof,            "$eof")
    SOUL_DECLARE_TOKEN (literalInt32,   "$integer32")
    SOUL_DECLARE_TOKEN (literalInt64,   "$integer64")
    SOUL_DECLARE_TOKEN (literalFloat32, "$float32")
    SOUL_DECLARE_TOKEN (literalFloat64, "$float64")
    SOUL_DECLARE_TOKEN (literalImag32,  "$imag32")
    SOUL_DECLARE_TOKEN (literalImag64,  "$imag64")
    SOUL_DECLARE_TOKEN (literalString,  "$string literal")
    SOUL_DECLARE_TOKEN (identifier,     "$identifier")
    SOUL_DECLARE_TOKEN (comment,        "$comment")
}

//==============================================================================
/** Low-level tokeniser which allows raw source code to be iterated as tokens.
    This handles recognising keywords, operators, and also literals.
*/
template <typename KeywordList, typename OperatorList, typename IdentifierMatcher>
struct Tokeniser
{
    Tokeniser() = default;
    virtual ~Tokeniser() = default;

    void initialise (const CodeLocation& code)
    {
        startLocation = code;
        location = code;
        input = location.location;
        skip();
    }

    TokenType skip()
    {
        if (shouldIgnoreComments)
            skipWhitespaceAndComments();
        else
            input = findEndOfWhitespace (input);

        location.location = input;
        auto last = currentType;
        currentType = matchNextToken();
        return last;
    }

    choc::text::UTF8Pointer getCurrentTokeniserPosition() const noexcept     { return location.location; }

    void resetPosition (choc::text::UTF8Pointer newPos)
    {
        if (input != newPos)
        {
            input = newPos;
            skip();
        }
    }

    bool matches (TokenType t) const noexcept                   { return currentType == t; }
    bool matches (const char* name) const                       { return matches (Token::identifier) && currentStringValue == name; }

    template <typename Type>
    bool matchesAny (Type t) const                              { return matches (t); }

    template <typename Type1, typename... Args>
    bool matchesAny (Type1 t1, Args... others) const            { return matches (t1) || matchesAny (others...); }

    template <typename Type>
    bool matchIf (Type expected)                                { if (matches (expected)) { skip(); return true; } return false; }

    template <typename Type, typename ReplacementType>
    bool matchAndReplaceIf (Type expected, ReplacementType replaceWith) { if (matches (expected)) { currentType = replaceWith; return true; } return false; }

    bool matchIfKeywordOrIdentifier (const char* text)          { if (matches (text) || currentType == text) { skip(); return true; } return false; }

    template <typename Type>
    void expect (Type expected)
    {
        if (! matchIf (expected))
            throwError (Errors::foundWhenExpecting (currentType, expected));
    }

    std::string readIdentifier()
    {
        auto name = currentStringValue;
        expect (Token::identifier);
        return name;
    }

    [[noreturn]] virtual void throwError (const CompileMessage& message) const = 0;

    CodeLocation startLocation, location;
    TokenType currentType;

    int64_t literalIntValue = 0;
    double literalDoubleValue = 0;
    std::string currentStringValue;
    bool shouldIgnoreComments = true;

    constexpr static const size_t maxIdentifierLength = 256;

private:
    choc::text::UTF8Pointer input;
    TokenType literalType = {};

    static bool skipIfStartsWithAny (choc::text::UTF8Pointer& p, const char* text)                      { return p.skipIfStartsWith (text); }
    template <typename... Args>
    static bool skipIfStartsWithAny (choc::text::UTF8Pointer& p, const char* text, Args... others)      { return p.skipIfStartsWith (text) || skipIfStartsWithAny (p, others...); }

    TokenType matchNextToken()
    {
        if (IdentifierMatcher::isIdentifierStart (*input))
        {
            auto end = input;
            size_t len = 1;

            while (IdentifierMatcher::isIdentifierBody (*++end))
                if (++len > maxIdentifierLength)
                    throwError (Errors::identifierTooLong());

            if (auto keyword = KeywordList::match ((int) len, input))
            {
                input += len;
                return keyword;
            }

            currentStringValue = std::string (input.data(), end.data());
            input = end;
            return IdentifierMatcher::categoriseIdentifier (currentStringValue);
        }

        if (isDigit (input))
            return parseNumericLiteral (false);

        auto currentChar = *input;

        if (currentChar == '-' && isDigit (input + 1))
        {
            ++input;
            auto tok = parseNumericLiteral (true);

            if (tok == Token::literalInt32 || tok == Token::literalInt64)
                literalIntValue = -literalIntValue;
            else
                literalDoubleValue = -literalDoubleValue;

            return tok;
        }

        if (parseStringLiteral (currentChar))
            return Token::literalString;

        if (currentChar == '.' && parseFloatLiteral())
            return literalType;

        if (currentChar == '/' && ! shouldIgnoreComments)
        {
            auto c2 = *(input + 1);

            if (c2 == '/')
            {
                auto end = input.find ("\n");
                currentStringValue = std::string (input.data(), end.data());
                input = end;
                return Token::comment;
            }

            if (c2 == '*')
            {
                location.location = input;
                auto end = (input + 2).find ("*/");
                if (end.empty()) throwError (Errors::unterminatedComment());
                end += 2;
                currentStringValue = std::string (input.data(), end.data());
                input = end;
                return Token::comment;
            }
        }

        if (auto op = OperatorList::match (input))
            return op;

        if (currentChar == '_' && IdentifierMatcher::isIdentifierBody (*(input + 1)))
            throwError (Errors::noLeadingUnderscoreAllowed());

        if (! input.empty())
            throwError (Errors::illegalCharacter (std::string (input.data(), (input + 1).data())));

        return Token::eof;
    }

    void skipWhitespaceAndComments()
    {
        for (;;)
        {
            input = findEndOfWhitespace (input);

            if (*input == '/')
            {
                auto c2 = *(input + 1);

                if (c2 == '/')  { input = input.find ("\n"); continue; }

                if (c2 == '*')
                {
                    location.location = input;
                    input = (input + 2).find ("*/");
                    if (input.empty()) throwError (Errors::unterminatedComment());
                    input += 2; continue;
                }
            }

            break;
        }
    }

    TokenType parseNumericLiteral (bool isNegative)
    {
        if (parseHexLiteral())      return checkIntLiteralRange (isNegative);
        if (parseFloatLiteral())    return literalType;
        if (parseOctalLiteral())    throwError (Errors::noOctalLiterals());
        if (parseBinaryLiteral())   return checkIntLiteralRange (isNegative);
        if (parseDecimalLiteral())  return checkIntLiteralRange (isNegative);

        throwError (Errors::errorInNumericLiteral());
        return {};
    }

    TokenType checkIntLiteralRange (bool isNegative)
    {
        if (literalType == Token::literalInt32)
        {
            if (isNegative && literalIntValue > -static_cast<int64_t> (std::numeric_limits<int32_t>::min()))
                throwError (Errors::integerLiteralNeedsSuffix());

            if (! isNegative && literalIntValue > static_cast<int64_t> (std::numeric_limits<int32_t>::max()))
                throwError (Errors::integerLiteralNeedsSuffix());
        }

        return literalType;
    }

    void checkCharacterImmediatelyAfterLiteral()
    {
        if (isDigit (input) || IdentifierMatcher::isIdentifierBody (*input))
        {
            location.location = input;
            throwError (Errors::unrecognisedLiteralSuffix());
        }
    }

    TokenType parseSuffixForIntLiteral()
    {
        if (skipIfStartsWithAny (input, "i64", "_i64", "L", "_L"))   return Token::literalInt64;
        if (skipIfStartsWithAny (input, "i32", "_i32"))              return Token::literalInt32;

        return Token::literalInt32;
    }

    bool parseDecimalLiteral()
    {
        return parseIntegerWithBase<10> (input, [] (UnicodeChar c) -> int { auto digit = (int) (c - '0'); return digit < 10 ? digit : -1; });
    }

    bool parseHexLiteral()
    {
        auto t = input;

        if (skipIfStartsWithAny (t, "0x", "0X"))
            return parseIntegerWithBase<16> (t, [] (UnicodeChar c) -> int { return choc::text::hexDigitToInt (c); });

        return false;
    }

    bool parseBinaryLiteral()
    {
        auto t = input;

        if (skipIfStartsWithAny (t, "0b", "0B"))
            return parseIntegerWithBase<2> (t, [] (UnicodeChar c) -> int { return c == '0' ? 0 : (c == '1' ? 1 : -1); });

        return false;
    }

    bool parseOctalLiteral()
    {
        auto t = input;

        return *t == '0' && isDigit (t + 1)
                && parseIntegerWithBase<8> (t, [this] (UnicodeChar c) -> int
                                               {
                                                    if (c >= '0' && c <= '7')  return ((int) c) - '0';
                                                    if (c == '8' || c == '9')  throwError (Errors::decimalDigitInOctal());
                                                    return -1;
                                               });
    }

    template <int base, typename GetNextDigitFn>
    bool parseIntegerWithBase (choc::text::UTF8Pointer t, GetNextDigitFn&& getNextDigit)
    {
        uint64_t v = 0;
        size_t numDigits = 0;

        for (;;)
        {
            auto possibleDigit = getNextDigit (*t);

            if (possibleDigit < 0)
                break;

            auto digit = (uint64_t) possibleDigit;

            if (v > std::numeric_limits<uint64_t>::max() / base)
                throwError (Errors::integerLiteralTooLarge());

            v = v * base;

            if (v > std::numeric_limits<uint64_t>::max() - digit)
                throwError (Errors::integerLiteralTooLarge());

            v += digit;
            ++numDigits;
            ++t;
        }

        if (numDigits == 0)
            return false;

        input = t;
        literalIntValue = (int64_t) v;
        literalType = parseSuffixForIntLiteral();
        checkCharacterImmediatelyAfterLiteral();
        return true;
    }

    TokenType parseSuffixForFloatLiteral()
    {
        if (skipIfStartsWithAny (input, "f32i", "_f32i", "fi"))    return Token::literalImag32;
        if (skipIfStartsWithAny (input, "f64i", "_f64i", "i"))     return Token::literalImag64;
        if (skipIfStartsWithAny (input, "f64", "_f64"))            return Token::literalFloat64;
        if (skipIfStartsWithAny (input, "f32", "_f32", "f", "_f")) return Token::literalFloat32;

        return Token::literalFloat64;
    }

    bool parseFloatLiteral()
    {
        int numDigits = 0;
        auto t = input;
        while (isDigit (t))  { ++t; ++numDigits; }

        const bool hasPoint = (*t == '.');

        if (hasPoint)
            while (isDigit (++t))  ++numDigits;

        if (numDigits == 0)
            return false;

        auto c = *t;
        const bool hasExponent = (c == 'e' || c == 'E');

        if (hasExponent)
        {
            c = *++t;
            if (c == '+' || c == '-')  ++t;
            if (! isDigit (t)) return false;
            while (isDigit (++t)) {}
        }

        if (! (hasExponent || hasPoint))
            return false;

        literalDoubleValue = std::stod (std::string (input.data(), t.data()));
        input = t;
        literalType = parseSuffixForFloatLiteral();
        checkCharacterImmediatelyAfterLiteral();
        return true;
    }

    bool parseStringLiteral (UnicodeChar quoteChar)
    {
        if (quoteChar != '"' && quoteChar != '\'')
            return false;

        ++input;
        currentStringValue = {};

        for (;;)
        {
            auto c = input.popFirstChar();

            if (c == quoteChar)
                break;

            if (c == '\\')
            {
                c = input.popFirstChar();

                switch (c)
                {
                    case '"':
                    case '\'':
                    case '\\':
                    case '/':  break;

                    case 'a':  c = '\a'; break;
                    case 'b':  c = '\b'; break;
                    case 'f':  c = '\f'; break;
                    case 'n':  c = '\n'; break;
                    case 'r':  c = '\r'; break;
                    case 't':  c = '\t'; break;

                    case 'u':
                    {
                        c = 0;

                        for (int i = 4; --i >= 0;)
                        {
                            auto digitValue = choc::text::hexDigitToInt (input.popFirstChar());

                            if (digitValue < 0)
                            {
                                location.location = input;
                                throwError (Errors::errorInEscapeCode());
                            }

                            c = (UnicodeChar) ((c << 4) + static_cast<UnicodeChar> (digitValue));
                        }

                        break;
                    }
                }
            }

            if (c == 0)
                throwError (Errors::endOfInputInStringConstant());

            appendUTF8 (currentStringValue, c);
        }

        checkCharacterImmediatelyAfterLiteral();
        return true;
    }

    static void appendUTF8 (std::string& target, UnicodeChar charToWrite)
    {
        auto c = (uint32_t) charToWrite;

        if (c >= 0x80)
        {
            int numExtraBytes = 1;

            if (c >= 0x800)
            {
                ++numExtraBytes;

                if (c >= 0x10000)
                    ++numExtraBytes;
            }

            target += (char) ((uint32_t) (0xff << (7 - numExtraBytes)) | (c >> (numExtraBytes * 6)));

            while (--numExtraBytes >= 0)
                target += (char) (0x80 | (0x3f & (c >> (numExtraBytes * 6))));
        }
        else
        {
            target += (char) c;
        }
    }
};

} // namespace soul
