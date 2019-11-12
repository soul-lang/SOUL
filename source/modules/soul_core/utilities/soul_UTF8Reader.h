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

using UnicodeChar = uint32_t;

//==============================================================================
/** Reads a stream of UTF8 data and extracts 32-bit unicode chars from it. */
class UTF8Reader  final
{
public:
    UTF8Reader() noexcept;
    explicit UTF8Reader (const char* utf8);
    UTF8Reader (const UTF8Reader&);
    UTF8Reader& operator= (const UTF8Reader&);

    bool operator== (UTF8Reader) const noexcept;
    bool operator!= (UTF8Reader) const noexcept;
    bool operator<= (UTF8Reader) const noexcept;
    bool operator<  (UTF8Reader) const noexcept;
    bool operator>= (UTF8Reader) const noexcept;
    bool operator>  (UTF8Reader) const noexcept;

    const char* getAddress() const noexcept;

    bool isEmpty() const noexcept;
    bool isNotEmpty() const noexcept;

    UnicodeChar operator*() const;
    UnicodeChar getAndAdvance();

    UTF8Reader& operator++();
    UTF8Reader operator--();

    void operator+= (int numToSkip);
    UTF8Reader operator+ (int numToSkip) const;

    bool startsWith (const char* text) const;
    bool advanceIfStartsWith (const char* text);

    template <typename... Args>
    bool advanceIfStartsWith (const char* text, Args... others)      { return advanceIfStartsWith (text) || advanceIfStartsWith (others...); }

    UTF8Reader find (const char* searchString) const;

    bool isWhitespace() const noexcept;
    bool isDigit() const noexcept;

    UTF8Reader findEndOfWhitespace() const;

    const char* findInvalidData() const;
    std::string createEscapedVersion() const;

private:
    const char* data;
};

} // namespace soul
