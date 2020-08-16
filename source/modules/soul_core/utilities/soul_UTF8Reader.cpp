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

UTF8Reader::UTF8Reader() noexcept : data (nullptr) {}
UTF8Reader::UTF8Reader (const char* utf8) : data (utf8)  { SOUL_ASSERT (data != nullptr); }
UTF8Reader::UTF8Reader (const UTF8Reader&) = default;
UTF8Reader& UTF8Reader::operator= (const UTF8Reader&) = default;

bool UTF8Reader::operator== (UTF8Reader other) const noexcept      { return data == other.data; }
bool UTF8Reader::operator!= (UTF8Reader other) const noexcept      { return data != other.data; }
bool UTF8Reader::operator<= (UTF8Reader other) const noexcept      { return data <= other.data; }
bool UTF8Reader::operator<  (UTF8Reader other) const noexcept      { return data <  other.data; }
bool UTF8Reader::operator>= (UTF8Reader other) const noexcept      { return data >= other.data; }
bool UTF8Reader::operator>  (UTF8Reader other) const noexcept      { return data >  other.data; }

const char* UTF8Reader::getAddress() const noexcept      { return data; }

bool UTF8Reader::isEmpty() const noexcept                { return *data == 0; }
bool UTF8Reader::isNotEmpty() const noexcept             { return *data != 0; }

UnicodeChar UTF8Reader::operator*() const
{
    auto byte = (signed char) *data;

    if (byte >= 0)
        return (UnicodeChar) (unsigned char) byte;

    auto n = (UnicodeChar) (unsigned char) byte;
    uint32_t mask = 0x7f;
    uint32_t bit = 0x40;
    int numExtraValues = 0;

    while ((n & bit) != 0 && bit > 0x8)
    {
        mask >>= 1;
        ++numExtraValues;
        bit >>= 1;
    }

    n &= mask;

    for (int i = 1; i <= numExtraValues; ++i)
    {
        auto nextByte = (uint32_t) (unsigned char) data[i];

        if ((nextByte & 0xc0) != 0x80)
            break;

        n <<= 6;
        n |= (nextByte & 0x3f);
    }

    return n;
}

UTF8Reader& UTF8Reader::operator++()
{
    SOUL_ASSERT (*data != 0); // trying to advance past the end of the string?
    auto n = (signed char) *data++;

    if (n < 0)
    {
        uint32_t bit = 0x40;

        while ((static_cast<uint32_t> (n) & bit) != 0 && bit > 0x8)
        {
            ++data;
            bit >>= 1;
        }
    }

    return *this;
}

UTF8Reader UTF8Reader::operator--()
{
    int count = 0;

    while ((*--data & 0xc0) == 0x80 && ++count < 4)
    {}

    return *this;
}

UnicodeChar UTF8Reader::getAndAdvance()
{
    auto byte = (signed char) *data++;

    if (byte >= 0)
        return (UnicodeChar) (unsigned char) byte;

    auto n = (UnicodeChar) (unsigned char) byte;
    uint32_t mask = 0x7f;
    uint32_t bit = 0x40;
    int numExtraValues = 0;

    while ((n & bit) != 0 && bit > 0x8)
    {
        mask >>= 1;
        ++numExtraValues;
        bit >>= 1;
    }

    n &= mask;

    while (--numExtraValues >= 0)
    {
        auto nextByte = (uint32_t) (unsigned char) *data;

        if ((nextByte & 0xc0) != 0x80)
            break;

        ++data;
        n <<= 6;
        n |= (nextByte & 0x3f);
    }

    return n;
}

void UTF8Reader::operator+= (int numToSkip)
{
    SOUL_ASSERT (numToSkip >= 0);

    while (--numToSkip >= 0)
        ++*this;
}

UTF8Reader UTF8Reader::operator+ (int numToSkip) const
{
    auto temp = *this;
    temp += numToSkip;
    return temp;
}

bool UTF8Reader::startsWith (const char* text) const
{
    for (auto t = *this;;)
    {
        auto charToMatch = *text++;

        if (charToMatch == 0)
            return true;

        if (t.getAndAdvance() != (UnicodeChar) (unsigned char) charToMatch)
            return false;
    }
}

bool UTF8Reader::advanceIfStartsWith (const char* text)
{
    for (auto t = *this;;)
    {
        auto charToMatch = *text++;

        if (charToMatch == 0)
        {
            *this = t;
            return true;
        }

        if (t.getAndAdvance() != (UnicodeChar) (unsigned char) charToMatch)
            return false;
    }
}

UTF8Reader UTF8Reader::find (const char* searchString) const
{
    for (auto t = *this;; ++t)
        if (t.startsWith (searchString) || t.isEmpty())
            return t;
}

bool UTF8Reader::isWhitespace() const noexcept          { return choc::text::isWhitespace (*data); }
bool UTF8Reader::isDigit() const noexcept               { return soul::isDigit (*data); }

UTF8Reader UTF8Reader::findEndOfWhitespace() const
{
    auto t = *this;

    while (t.isWhitespace())
        ++t;

    return t;
}

const char* UTF8Reader::findInvalidData() const
{
    for (auto t = data; *t != 0;)
    {
        auto byte = (signed char) *t++;

        if (byte < 0)
        {
            auto errorPos = t - 1;
            int bit = 0x40;
            int numExtraValues = 0;

            while ((byte & bit) != 0)
            {
                if (bit < 8)
                    return errorPos;

                ++numExtraValues;
                bit >>= 1;

                if (bit == 8 && *UTF8Reader (t) > 0x10ffff)
                    return errorPos;
            }

            if (numExtraValues == 0)
                return errorPos;

            while (--numExtraValues >= 0)
                if ((*t++ & 0xc0) != 0x80)
                    return errorPos;
        }
    }

    return nullptr;
}

std::string UTF8Reader::createEscapedVersion() const
{
    std::ostringstream out;
    auto hexDigit = [] (UnicodeChar n) { return "0123456789abcdef"[n & 15]; };

    for (auto utf8 = *this;;)
    {
        auto c = utf8.getAndAdvance();

        switch (c)
        {
            case 0:  return out.str();

            case '\"':  out << "\\\""; break;
            case '\\':  out << "\\\\"; break;
            case '\a':  out << "\\a";  break;
            case '\b':  out << "\\b";  break;
            case '\f':  out << "\\f";  break;
            case '\t':  out << "\\t";  break;
            case '\r':  out << "\\r";  break;
            case '\n':  out << "\\n";  break;

            default:
            {
                if (c >= 32 && c < 127)
                {
                    out << (char) c;
                }
                else
                {
                    out << "\\u"
                        << hexDigit (c >> 12)
                        << hexDigit (c >> 8)
                        << hexDigit (c >> 4)
                        << hexDigit (c);
                }

                break;
            }
        }
    }
}

} // namespace soul
