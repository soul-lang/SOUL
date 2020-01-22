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

bool isWhitespace (char c)           { return c == ' ' || (c <= 13 && c >= 9); }
bool isDigit (char c)                { return c >= '0' && c <= '9'; }
bool isSafeIdentifierChar (char c)   { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || isDigit (c); }

std::string repeatedCharacter (char c, size_t num)
{
    std::string s;
    s.resize (num, c);
    return s;
}

std::string padded (const std::string& s, int minSize)
{
    auto extraNeeded = std::max (1, minSize - (int) s.length());
    return s + repeatedCharacter (' ', (size_t) extraNeeded);
}

bool containsChar (const std::string& s, char c) noexcept
{
    return s.find (c) != std::string::npos;
}

bool containsChar (const char* s, char c) noexcept
{
    if (s == nullptr)
        return false;
    
    for (; *s != 0; ++s)
        if (*s == c)
            return true;

    return false;
}

std::string trimCharacterAtStart (const std::string& s, char charToRemove)
{
    for (auto c = s.begin(); c != s.end(); ++c)
        if (*c != charToRemove)
            return { c, s.end() };

    return {};
}

std::string trimStart (const std::string& s)
{
    for (auto c = s.begin(); c != s.end(); ++c)
        if (! isWhitespace (*c))
            return { c, s.end() };

    return {};
}

std::string trimEnd (std::string s)
{
    s.erase (std::find_if (s.rbegin(), s.rend(), [] (char ch) { return ! isWhitespace (ch); }).base(),
             s.end());

    return s;
}

std::string trim (std::string s)
{
    return trimStart (trimEnd (s));
}

bool endsWith (const std::string& text, const std::string& possibleEnd)
{
    return text.length() >= possibleEnd.length()
            && text.rfind (possibleEnd) == text.length() - possibleEnd.length();
}

std::string addDoubleQuotes (const std::string& text)     { return "\"" + text + "\""; }
std::string addSingleQuotes (const std::string& text)     { return "'" + text + "'"; }

std::string replaceSubString (std::string s, const std::string& toReplace, const std::string& replacement)
{
    for (size_t index = 0;;)
    {
        index = s.find (toReplace, index);

        if (index == std::string::npos)
            return s;

        s.replace (index, toReplace.length(), replacement);
        index += replacement.length();
    }
}

std::string retainCharacters (std::string s, const std::string& charactersToRetain)
{
    s.erase (std::remove_if (s.begin(), s.end(), [&] (char c) { return ! contains (charactersToRetain, c); }), s.end());
    return s;
}

bool stringMatchesOneOf (const std::string& stringToTest, ArrayView<const char*> possibleMatches)
{
    for (auto m : possibleMatches)
        if (stringToTest == m)
            return true;

    return false;
}

template <typename IsDelimiterStart, typename IsDelimiterBody>
std::vector<std::string> split (const std::string& text,
                                IsDelimiterStart&& isStart,
                                IsDelimiterBody&& isBody,
                                bool includeDelimiters)
{
    std::vector<std::string> tokens;
    auto start = text.c_str();
    auto end = start;

    for (;;)
    {
        if (*end == 0)
        {
            if (end != text.c_str())
                tokens.push_back ({ start, end });

            return tokens;
        }

        if (isStart (*end))
        {
            auto startOfDelimiter = end;
            ++end;

            while (isBody (*end))
                ++end;

            if (end != text.c_str())
                tokens.push_back ({ start, includeDelimiters ? end : startOfDelimiter });

            start = end;
            continue;
        }

        ++end;
    }

    return tokens;
}

std::vector<std::string> splitAtDelimiter (const std::string& text, char delimiter)
{
    return split (text,
                  [delimiter] (char c) { return c == delimiter; },
                  [] (char) { return false; },
                  false);
}

std::vector<std::string> splitAtWhitespace (const std::string& text)
{
    return split (text,
                  [] (char c) { return isWhitespace (c); },
                  [] (char c) { return isWhitespace (c); },
                  false);
}

std::vector<std::string> splitIntoLines (const std::string& text)
{
    return split (text,
                  [] (char c) { return c == '\n'; },
                  [] (char) { return false; },
                  true);
}

std::vector<std::string> splitLinesOfCode (const std::string& text, size_t targetLineLength)
{
    std::vector<std::string> result;
    char currentQuoteChar = 0;
    size_t currentTokenLength = 0;
    const char* t = text.c_str();
    auto tokenStart = t;

    auto isBreakChar = [] (char c)  { return c == ' ' || c == '\t' || c == ',' || c == ';' || c == '\n'; };
    auto isQuoteChar = [] (char c)  { return c == '"' || c == '\''; };

    for (;;)
    {
        if (*t == 0)
        {
            if (*tokenStart != 0)
                result.push_back (tokenStart);

            return result;
        }

        auto c = *t++;

        if (++currentTokenLength > targetLineLength && currentQuoteChar == 0 && isBreakChar (c))
        {
            result.push_back (std::string (tokenStart, t));
            tokenStart = t;
            currentTokenLength = 0;
        }

        if (isQuoteChar (c))
        {
            if (currentQuoteChar == 0)
                currentQuoteChar = c;
            else if (currentQuoteChar == c)
                currentQuoteChar = 0;
        }
    }
}

std::string replaceLine (const std::string& text, size_t line, const std::string& replacementLine)
{
    auto lines = splitIntoLines (text);
    lines[line] = replacementLine + (containsChar (lines[line], '\r') ? "\r\n" : "\n");
    return joinStrings (lines, {});
}

std::string loadFileAsString (const char* filename)
{
    if (std::ifstream stream { filename, std::ios::binary | std::ios::ate })
    {
        auto size = stream.tellg();

        if (size > 0)
        {
            std::string content ((size_t) size, '\0');
            stream.seekg (0);

            if (stream.read (&content[0], static_cast<std::streamsize> (size)))
                return content;
        }
    }

    return {};
}

std::string makeSafeIdentifierName (std::string s)
{
    for (auto& c : s)
        if (containsChar (" ,./;", c))
            c = '_';

    s.erase (std::remove_if (s.begin(), s.end(), [&] (char c) { return ! isSafeIdentifierChar (c); }), s.end());

    // Identifiers can't start with a digit
    if (isDigit (s[0]))
        s = "_" + s;

    return s;
}

bool isSafeIdentifierName (std::string s)
{
    return s == makeSafeIdentifierName (s);
}

std::string makeIdentifierRemovingColons (std::string s)
{
    return makeSafeIdentifierName (replaceSubString (trimCharacterAtStart (s, ':'), "::", "_"));
}

std::string toStringWithDecPlaces (double n, size_t numDecPlaces)
{
    auto s = std::to_string (n);
    auto dot = s.find ('.');
    return dot == std::string::npos ? s : s.substr (0, std::min (s.length(), dot + 1 + numDecPlaces));
}

template <typename FloatType>
static std::string floatToString (FloatType value)
{
    char buffer[FloatToString<FloatType>::maxBufferSizeNeeded];
    auto end = FloatToString<FloatType>::write (value, buffer);
    return std::string (buffer, end);
}

std::string floatToAccurateString (float n)     { return floatToString (n); }
std::string doubleToAccurateString (double n)   { return floatToString (n); }

std::string getDescriptionOfTimeInSeconds (double numSeconds)
{
    return numSeconds < 1.0 ? (toStringWithDecPlaces (numSeconds * 1000.0, numSeconds < 0.1 ? 2 : 1) + " ms")
                            : (toStringWithDecPlaces (numSeconds, 2) + " sec");
}

int getHexDigitValue (uint32_t digit) noexcept
{
    auto d = digit - (uint32_t) '0';

    if (d < (uint32_t) 10)
        return (int) d;

    d += (uint32_t) ('0' - 'a');

    if (d < (uint32_t) 6)
        return (int) d + 10;

    d += (uint32_t) ('a' - 'A');

    if (d < (uint32_t) 6)
        return (int) d + 10;

    return -1;
}

std::string toHexString (int64_t value)
{
    std::ostringstream s;
    s << std::hex << value;
    return s.str();
}

std::string toHexString (int64_t value, int numDigits)
{
    std::ostringstream s;
    s << std::setw (numDigits) << std::setfill ('0') << std::hex << value;
    return s.str();
}

std::string doubleToJSONString (double n)
{
    if (std::isfinite (n))
        return doubleToAccurateString (n);

    if (std::isnan (n))
        return "\"NaN\"";

    return n < 0 ? "\"-Infinity\""
                 : "\"Infinity\"";
}

std::string getReadableDescriptionOfByteSize (uint64_t bytes)
{
    if (bytes == 1)                  return "1 byte";
    if (bytes < 1024)                return std::to_string (bytes) + " bytes";
    if (bytes < 1024 * 1024)         return toStringWithDecPlaces (double (bytes) / 1024.0, 1)                     + " KB";
    if (bytes < 1024 * 1024 * 1024)  return toStringWithDecPlaces (double (bytes) / (1024.0 * 1024.0), 1)          + " MB";
    else                             return toStringWithDecPlaces (double (bytes) / (1024.0 * 1024.0 * 1024.0), 1) + " GB";
}

std::string convertToString (const std::string& name)        { return name; }
std::string convertToString (const Identifier& name)         { return name.toString(); }
std::string convertToString (const IdentifierPath& name)     { return Program::stripRootNamespaceFromQualifiedPath (name.toString()); }

std::string quoteName (const std::string& name)        { return addSingleQuotes (convertToString (name)); }
std::string quoteName (const Identifier& name)         { return addSingleQuotes (convertToString (name)); }
std::string quoteName (const IdentifierPath& name)     { return addSingleQuotes (convertToString (name)); }

size_t levenshteinDistance (const std::string& s1, const std::string& s2)
{
    auto m = s1.size(),
         n = s2.size();

    if (m == 0) return n;
    if (n == 0) return m;

    std::vector<size_t> costs;
    costs.reserve (n + 1);

    for (size_t i = 0; i <= n; ++i)
        costs.push_back (i);

    size_t i = 0;

    for (auto c1 : s1)
    {
        costs.front() = i + 1;
        size_t corner = i, j = 0;

        for (auto c2 : s2)
        {
            auto upper = costs[j + 1];
            costs[j + 1] = c1 == c2 ? corner : (std::min (costs[j], std::min (upper, corner)) + 1);
            corner = upper;
            ++j;
        }

        ++i;
    }

    return costs[n];
}

HashBuilder& HashBuilder::operator<< (char c) noexcept
{
    auto n = (uint8_t) c;
    data[(index + n) & 255] = (data[(index + n) & 255] * 7) ^ n;
    ++index;
    data[index & 255] = (data[index & 255] * 31) + n + (index % 511);
    ++index;
    data[index & 255] = (data[index & 255] * 137) - n;
    ++index;

    return *this;
}

HashBuilder& HashBuilder::operator<< (const std::string& s) noexcept
{
    for (auto& c : s)
        *this << c;

    return *this;
}

HashBuilder& HashBuilder::operator<< (ArrayView<char> s) noexcept
{
    for (auto c : s)
        *this << c;

    return *this;
}

std::string HashBuilder::toString() const
{
    char result[32] = {};
    uint8_t i = 0;

    for (auto& resultChar : result)
    {
        uint32_t n = 0;

        for (int j = 0; j < 256; j += 32)
            n ^= data[i + j];

        ++i;
        n = (n ^ (n >> 6) ^ (n >> 12) ^ (n >> 18) ^ (n >> 24) ^ (n >> 30)) % 36;
        resultChar = (char) (n < 10 ? (n + '0') : (n + 'a' - 10));
    }

    return std::string (result, result + 32);
}

std::string toCppStringLiteral (const std::string& text,
                                int maxCharsOnLine, bool breakAtNewLines,
                                bool replaceSingleQuotes, bool allowStringBreaks)
{
    std::ostringstream out;
    out << '"';

    int charsOnLine = 0;
    bool lastWasHexEscapeCode = false;
    bool trigraphDetected = false;
    auto utf8 = text.c_str();
    auto numBytesToRead = (int) text.length();

    for (int i = 0; i < numBytesToRead || numBytesToRead < 0; ++i)
    {
        auto c = (unsigned char) utf8[i];
        bool startNewLine = false;

        switch (c)
        {
            case '\t':  out << "\\t";  trigraphDetected = false; lastWasHexEscapeCode = false; charsOnLine += 2; break;
            case '\r':  out << "\\r";  trigraphDetected = false; lastWasHexEscapeCode = false; charsOnLine += 2; break;
            case '\n':  out << "\\n";  trigraphDetected = false; lastWasHexEscapeCode = false; charsOnLine += 2; startNewLine = breakAtNewLines; break;
            case '\\':  out << "\\\\"; trigraphDetected = false; lastWasHexEscapeCode = false; charsOnLine += 2; break;
            case '\"':  out << "\\\""; trigraphDetected = false; lastWasHexEscapeCode = false; charsOnLine += 2; break;

            case '?':
                if (trigraphDetected)
                {
                    out << "\\?";
                    charsOnLine++;
                    trigraphDetected = false;
                }
                else
                {
                    out << "?";
                    trigraphDetected = true;
                }

                lastWasHexEscapeCode = false;
                charsOnLine++;
                break;

            case 0:
                if (numBytesToRead < 0)
                {
                    out << '"';
                    return out.str();
                }

                out << "\\0";
                lastWasHexEscapeCode = true;
                trigraphDetected = false;
                charsOnLine += 2;
                break;

            default:
                if (c == '\'' && replaceSingleQuotes)
                {
                    out << "\\\'";
                    lastWasHexEscapeCode = false;
                    trigraphDetected = false;
                    charsOnLine += 2;
                }
                else if (c >= 32 && c < 127 && ! (lastWasHexEscapeCode  // (have to avoid following a hex escape sequence with a valid hex digit)
                                                   && getHexDigitValue (c) >= 0))
                {
                    out << (char) c;
                    lastWasHexEscapeCode = false;
                    trigraphDetected = false;
                    ++charsOnLine;
                }
                else if (allowStringBreaks && lastWasHexEscapeCode && c >= 32 && c < 127)
                {
                    out << "\"\n\"" << (char) c;
                    lastWasHexEscapeCode = false;
                    trigraphDetected = false;
                    charsOnLine += 3;
                }
                else
                {
                    out << (c < 16 ? "\\x0" : "\\x") << toHexString ((int) c);
                    lastWasHexEscapeCode = true;
                    trigraphDetected = false;
                    charsOnLine += 4;
                }

                break;
        }

        if ((startNewLine || (maxCharsOnLine > 0 && charsOnLine >= maxCharsOnLine))
             && (numBytesToRead < 0 || i < numBytesToRead - 1))
        {
            charsOnLine = 0;
            out << "\"\n\"";
            lastWasHexEscapeCode = false;
        }
    }

    out << '"';
    return out.str();
}

} // namespace soul
