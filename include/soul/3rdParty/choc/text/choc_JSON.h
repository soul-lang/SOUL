//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_JSON_HEADER_INCLUDED
#define CHOC_JSON_HEADER_INCLUDED

#include <limits>
#include <sstream>
#include <string_view>

#include "choc_UTF8.h"
#include "choc_FloatToString.h"
#include "../containers/choc_Value.h"

#undef max   // It's never a smart idea to include any C headers before your C++ ones, as it
#undef min   // risks polluting your namespace with all kinds of dangerous macros like these ones.

namespace choc::json
{

//==============================================================================
/** A parse exception, thrown by choc::json::parse() as needed. */
struct ParseError
{
    const char* message;
    choc::text::LineAndColumn lineAndColumn;
};

/** Parses some JSON text into a choc::value::Value object, using the given pool.
    Any errors will result in a ParseError exception being thrown.
*/
value::Value parse (text::UTF8Pointer);

/** Parses some JSON text into a choc::value::Value object, using the given pool.
    Any errors will result in a ParseError exception being thrown.
*/
value::Value parse (std::string_view);

//==============================================================================
/** Formats a value as a JSON string. */
std::string toString (const value::ValueView&);

/** Writes a version of a string to an output stream, with any illegal or non-ascii
    written as their equivalent JSON escape sequences.
*/
template <typename OutputStreamType>
void writeWithEscapeCharacters (OutputStreamType&, text::UTF8Pointer sourceString);

/** Returns a version of a string with illegal or non-ascii converted into the
    equivalent JSON escape sequences.
*/
std::string addEscapeCharacters (text::UTF8Pointer sourceString);

/** Returns a version of a string with illegal or non-ascii converted into the
    equivalent JSON escape sequences.
*/
std::string addEscapeCharacters (std::string_view sourceString);

/** Returns a version of a string with illegal or non-ascii converted into the
    equivalent JSON escape sequences.
*/
std::string getEscapedQuotedString (std::string_view sourceString);

/** Converts a double to a JSON-format string representation. */
std::string doubleToString (double value);

//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

template <typename OutputStreamType>
void writeWithEscapeCharacters (OutputStreamType& out, text::UTF8Pointer source)
{
    auto hexDigit = [] (auto value) -> char { return "0123456789abcdef"[value & 15]; };

    for (;;)
    {
        auto c = *source;

        switch (c)
        {
            case 0:  return;

            case '\"':   out << "\\\""; break;
            case '\\':   out << "\\\\"; break;
            case '\n':   out << "\\n";  break;
            case '\r':   out << "\\r";  break;
            case '\t':   out << "\\t";  break;
            case '\a':   out << "\\a";  break;
            case '\b':   out << "\\b";  break;
            case '\f':   out << "\\f";  break;

            default:
                if (c > 31 && c < 127)
                    out << (char) c;
                else
                    out << "\\u" << hexDigit (c >> 12) << hexDigit (c >> 8) << hexDigit (c >> 4) << hexDigit (c);

                break;
        }

        ++source;
    }
}

inline std::string addEscapeCharacters (text::UTF8Pointer source)
{
    std::ostringstream result;
    writeWithEscapeCharacters (result, source);
    return result.str();
}

inline std::string addEscapeCharacters (std::string_view source)
{
    return addEscapeCharacters (text::UTF8Pointer (std::string (source).c_str()));
}

inline std::string getEscapedQuotedString (std::string_view s)
{
    std::ostringstream result;
    result << '"';
    writeWithEscapeCharacters (result, text::UTF8Pointer (std::string (s).c_str()));
    result << '"';
    return result.str();
}

inline std::string doubleToString (double value)
{
    if (std::isfinite (value))    return choc::text::floatToString (value);
    if (std::isnan (value))       return "\"NaN\"";

    return value >= 0 ?  "\"Infinity\""
                      : "\"-Infinity\"";
}

//==============================================================================
template <typename Stream>
void writeAsJSON (Stream& output, const value::ValueView& value)
{
    struct Writer
    {
        Stream& out;

        void dump (const value::ValueView& v)
        {
            if (v.isVoid())                   { out << "null"; return; }
            if (v.isString())                 { out << getEscapedQuotedString (v.getString()); return; }
            if (v.isBool())                   { out << (v.getBool() ? "true" : "false"); return; }
            if (v.isFloat())                  { out << v.get<double>(); return; }
            if (v.isInt())                    { out << v.get<int64_t>(); return; }
            if (v.isObject())                 return dumpObject (v);
            if (v.isArray() || v.isVector())  return dumpArrayOrVector (v);
        }

        void dumpArrayOrVector (const value::ValueView& v)
        {
            out << '[';
            auto num = v.size();

            for (uint32_t i = 0; i < num; ++i)
            {
                if (i != 0) out << ", ";
                dump (v[i]);
            }

            out << ']';
        }

        void dumpObject (const value::ValueView& object)
        {
            out << '{';
            auto numMembers = object.size();

            for (uint32_t i = 0; i < numMembers; ++i)
            {
                if (i != 0) out << ", ";

                auto member = object.getObjectMemberAt (i);
                out << getEscapedQuotedString (member.name) << ": ";
                dump (member.value);
            }

            out << '}';
        }
    };

    Writer { output }.dump (value);
}

inline std::string toString (const value::ValueView& v)
{
    std::ostringstream out;
    writeAsJSON (out, v);
    return out.str();
}

//==============================================================================
[[noreturn]] static inline void throwParseError (const char* error, text::UTF8Pointer source, text::UTF8Pointer errorPos)
{
    throw ParseError { error, text::findLineAndColumn (source, errorPos) };
}

inline value::Value parse (text::UTF8Pointer text)
{
    struct Parser
    {
        text::UTF8Pointer source, current;

        bool isEOF() const            { return current.empty(); }
        uint32_t peek() const         { return *current; }
        uint32_t pop()                { return current.popFirstChar(); }
        bool popIf (char c)           { return current.skipIfStartsWith (c); }
        bool popIf (const char* c)    { return current.skipIfStartsWith (c); }

        static bool isWhitespace (uint32_t c)   { return c == ' ' || (c <= 13 && c >= 9); }
        void skipWhitespace()                   { auto p = current; while (isWhitespace (p.popFirstChar())) current = p; }

        [[noreturn]] void throwError (const char* error, text::UTF8Pointer errorPos)    { throwParseError (error, source, errorPos); }
        [[noreturn]] void throwError (const char* error)                                { throwError (error, current); }

        value::Value parseTopLevel()
        {
            skipWhitespace();

            if (popIf ('[')) return parseArray();
            if (popIf ('{')) return parseObject();
            if (! isEOF()) throwError ("Expected an object or array");
            return {};
        }

        value::Value parseArray()
        {
            auto result = value::createEmptyArray();
            auto arrayStart = current;

            skipWhitespace();
            if (popIf (']')) return result;

            for (;;)
            {
                skipWhitespace();
                if (isEOF())  throwError ("Unexpected EOF in array declaration", arrayStart);

                result.addArrayElement (parseValue());
                skipWhitespace();

                if (popIf (',')) continue;
                if (popIf (']')) break;
                throwError ("Expected ',' or ']'");
            }

            return result;
        }

        value::Value parseObject()
        {
            auto result = value::createObject ("JSON");
            auto objectStart = current;

            skipWhitespace();
            if (popIf ('}')) return result;

            for (;;)
            {
                skipWhitespace();
                if (isEOF())  throwError ("Unexpected EOF in object declaration", objectStart);

                if (! popIf ('"')) throwError ("Expected a name");
                auto errorPos = current;
                auto name = parseString ('"');

                if (name.empty())
                    throwError ("Property names cannot be empty", errorPos);

                skipWhitespace();
                errorPos = current;
                if (! popIf (':')) throwError ("Expected ':'");
                result.addMember (std::move (name), parseValue());
                skipWhitespace();

                if (popIf (',')) continue;
                if (popIf ('}')) break;
                throwError ("Expected ',' or '}'");
            }

            return result;
        }

        value::Value parseValue()
        {
            skipWhitespace();
            auto startPos = current;

            switch (pop())
            {
                case '[':                                 return parseArray();
                case '{':                                 return parseObject();
                case '"':                                 return value::createString (parseString ('"'));
                case '-':                                 skipWhitespace(); return parseNumber (true);
                case '0': case '1': case '2':
                case '3': case '4': case '5':
                case '6': case '7': case '8': case '9':   current = startPos; return parseNumber (false);
                default:                                  break;
            }

            current = startPos;
            if (popIf ("null"))   return {};
            if (popIf ("true"))   return value::createBool (true);
            if (popIf ("false"))  return value::createBool (false);

            throwError ("Syntax error");
        }

        value::Value parseNumber (bool negate)
        {
            auto startPos = current;
            bool isDouble = false;

            for (;;)
            {
                auto lastPos = current;
                auto c = pop();

                if (c >= '0' && c <= '9')
                    continue;

                if (! isDouble && (c == 'e' || c == 'E' || c == '.'))
                {
                    isDouble = true;
                    continue;
                }

                if (isWhitespace (c) || c == ',' || c == '}' || c == ']' || c == 0)
                {
                    current = lastPos;
                    char* endOfParsedNumber = nullptr;

                    if (! isDouble)
                    {
                        auto v = std::strtoll (startPos.data(), &endOfParsedNumber, 10);

                        if (endOfParsedNumber == lastPos.data()
                             && v != std::numeric_limits<long long>::max()
                             && v != std::numeric_limits<long long>::min())
                            return value::createInt64 (static_cast<int64_t> (negate ? -v : v));
                    }

                    auto v = std::strtod (startPos.data(), &endOfParsedNumber);

                    if (endOfParsedNumber == lastPos.data())
                        return value::createFloat64 (negate ? -v : v);
                }

                throwError ("Syntax error in number", lastPos);
            }
        }

        std::string parseString (uint32_t terminatingQuote)
        {
            std::ostringstream s;

            for (;;)
            {
                auto c = pop();

                if (c == terminatingQuote)
                    break;

                if (c == '\\')
                {
                    auto errorPos = current;
                    c = pop();

                    switch (c)
                    {
                        case 'a':  c = '\a'; break;
                        case 'b':  c = '\b'; break;
                        case 'f':  c = '\f'; break;
                        case 'n':  c = '\n'; break;
                        case 'r':  c = '\r'; break;
                        case 't':  c = '\t'; break;
                        case 'u':  c = parseUnicodeCharacterNumber (false); break;
                        case 0:    throwError ("Unexpected EOF in string constant", errorPos);
                        default:   break;
                    }
                }

                char utf8Bytes[8];
                auto numBytes = text::convertUnicodeCodepointToUTF8 (utf8Bytes, c);

                for (uint32_t i = 0; i < numBytes; ++i)
                    s << utf8Bytes[i];
            }

            return s.str();
        }

        uint32_t parseUnicodeCharacterNumber (bool isLowSurrogate)
        {
            uint32_t result = 0;

            for (int i = 4; --i >= 0;)
            {
                auto errorPos = current;
                auto digit = pop();

                if (digit >= '0' && digit <= '9')         digit -= '0';
                else if (digit >= 'a' && digit <= 'f')    digit = 10 + (digit - 'a');
                else if (digit >= 'A' && digit <= 'F')    digit = 10 + (digit - 'A');
                else throwError ("Syntax error in unicode character", errorPos);

                result = (result << 4) + digit;
            }

            if (isLowSurrogate && ! text::isUnicodeLowSurrogate (result))
                throwError ("Expected a unicode low surrogate codepoint");

            if (text::isUnicodeHighSurrogate (result))
            {
                if (! isLowSurrogate && popIf ("\\u"))
                    return text::createUnicodeFromHighAndLowSurrogates (result, parseUnicodeCharacterNumber (true));

                throwError ("Expected a unicode low surrogate codepoint");
            }

            return result;
        }
    };

    Parser p { text, text };
    return p.parseTopLevel();
}

inline value::Value parse (const char* text, size_t numbytes)
{
    CHOC_ASSERT (text != nullptr);

    if (auto error = text::findInvalidUTF8Data (text, numbytes))
        throwParseError ("Illegal UTF8 data", text::UTF8Pointer (text), text::UTF8Pointer (error));

    return parse (text::UTF8Pointer (text));
}

inline value::Value parse (std::string_view text)       { return parse (text.data(), text.length()); }


} // namespace choc::json

#endif
