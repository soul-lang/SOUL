/*
    ██████ ██   ██  ██████   ██████
   ██      ██   ██ ██    ██ ██         Clean Header-Only Classes
   ██      ███████ ██    ██ ██         Copyright (C)2020 Julian Storer
   ██      ██   ██ ██    ██ ██
    ██████ ██   ██  ██████   ██████    https://github.com/julianstorer/choc

   This file is part of the CHOC C++ collection - see the github page to find out more.

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose with
   or without fee is hereby granted, provided that the above copyright notice and this
   permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
   THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT
   SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR
   ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
   CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
   OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef CHOC_UTF8_HEADER_INCLUDED
#define CHOC_UTF8_HEADER_INCLUDED

#include <cstddef>
#include <cassert>

namespace choc::text
{

//==============================================================================
/** A non-owning pointer which can iterate over a chunk of null-terminated UTF8 text
    and read it as wide unicode characters.
*/
struct UTF8Pointer
{
    explicit constexpr UTF8Pointer (const char* utf8Text) noexcept  : text (utf8Text) {}

    UTF8Pointer() = default;
    UTF8Pointer (const UTF8Pointer&) = default;
    UTF8Pointer& operator= (const UTF8Pointer&) = default;

    /** Returns the raw data that this points to. */
    const char* data() const noexcept                   { return text; }

    /** Returns true if the pointer is not null. */
    operator bool() const noexcept                      { return text != nullptr; }

    /** Returns true if the pointer is either null or points to a null terminator char. */
    bool empty() const                                  { return text == nullptr || *text == 0; }

    /** Returns the length by iterating all unicode chars and counting them.
        Note that this is slow, and is not a count of the number of bytes in the string!
    */
    size_t length() const;

    //==============================================================================
    /** Returns the first unicode character in the string. */
    uint32_t operator*() const;

    /** Skips past the first unicode character.
        Moving beyond the end of the string is undefined behaviour and will trigger an assertion.
    */
    UTF8Pointer& operator++();

    /** Skips past the first unicode character.
        Moving beyond the end of the string is undefined behaviour and will trigger an assertion.
    */
    UTF8Pointer operator++ (int);

    /** Moves backwards to the previous unicode character.
        Moving beyond the end of the string is undefined behaviour.
    */
    UTF8Pointer operator--();

    /** Skips past the given number of unicode characters.
        Moving beyond the end of the string is undefined behaviour and will trigger an assertion.
    */
    UTF8Pointer& operator+= (int numCharsToSkip);

    /** Returns the first unicode character, and also moves the pointer forwards to the next character.
        Reading beyond the end of the string is undefined behaviour and will trigger an assertion.
    */
    UTF8Pointer operator+ (int numCharsToSkip) const;

    /** Skips past the first unicode character and returns it as a code-point.
        Calling this when the current character is the terminator will leave the pointer in an
        invalid state.
    */
    uint32_t popFirstChar();

    /** If the first character matches the given one, this will advance the pointer and return true. */
    bool skipIfStartsWith (char charToMatch);

    /** If the start of the text matches the given string, this will advance this pointer to skip
        past it, and return true. If not, it will return false without modifying this pointer.
    */
    bool skipIfStartsWith (const char* textToMatch);

    //==============================================================================
    struct EndIterator {};

    struct Iterator
    {
        explicit constexpr Iterator (const char* t) : text (t) {}
        Iterator (const Iterator&) = default;
        Iterator& operator= (const Iterator&) = default;

        uint32_t operator*() const              { return *UTF8Pointer (text); }
        Iterator& operator++()                  { UTF8Pointer p (text); ++p; text = p.text; return *this; }
        Iterator operator++ (int)               { auto old = *this; ++*this; return old; }
        bool operator== (EndIterator) const     { return *text == 0; }
        bool operator!= (EndIterator) const     { return *text != 0; }

    private:
        const char* text;
    };

    Iterator begin() const;
    EndIterator end() const;

    //==============================================================================
    /** This does a pointer comparison, NOT a comparison of the text itself! */
    bool operator== (UTF8Pointer other) const noexcept      { return text == other.text; }
    /** This does a pointer comparison, NOT a comparison of the text itself! */
    bool operator!= (UTF8Pointer other) const noexcept      { return text != other.text; }
    /** This does a pointer comparison, NOT a comparison of the text itself! */
    bool operator<  (UTF8Pointer other) const noexcept      { return text <  other.text; }
    /** This does a pointer comparison, NOT a comparison of the text itself! */
    bool operator>  (UTF8Pointer other) const noexcept      { return text >  other.text; }
    /** This does a pointer comparison, NOT a comparison of the text itself! */
    bool operator<= (UTF8Pointer other) const noexcept      { return text <= other.text; }
    /** This does a pointer comparison, NOT a comparison of the text itself! */
    bool operator>= (UTF8Pointer other) const noexcept      { return text >= other.text; }

    bool operator== (decltype(nullptr)) const noexcept      { return text == nullptr; }
    bool operator!= (decltype(nullptr)) const noexcept      { return text != nullptr; }

private:
    const char* text = nullptr;
};

//==============================================================================
/** Checks a given chunk of data to see whether it's valid, null-terminated UTF8.
    If no errors are found, this returns nullptr. If an error is found, it returns the address
    of the offending byte.
*/
const char* findInvalidUTF8Data (const void* dataToCheck, size_t numbytes);

/** Writes the bytes for a unicode character, and returns the number of bytes that were needed.
    The buffer passed in needs to have at least 4 bytes capacity.
*/
uint32_t convertUnicodeCodepointToUTF8 (char* dest, uint32_t unicodeChar);

/** Checks whether a given codepoint is a high-surrogate */
bool isUnicodeHighSurrogate (uint32_t codepoint);

/** Checks whether a given codepoint is a low-surrogate */
bool isUnicodeLowSurrogate (uint32_t codepoint);

/** Combines a high and low surrogate into a single codepoint. */
uint32_t createUnicodeFromHighAndLowSurrogates (uint32_t high, uint32_t low);



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

#ifndef CHOC_ASSERT
 #define CHOC_ASSERT(x)  assert(x);
#endif

inline size_t UTF8Pointer::length() const
{
    size_t count = 0;

    if (text != nullptr)
        for (auto p = *this; *p.text != 0; ++p)
            ++count;

    return count;
}

inline const char* findInvalidUTF8Data (const void* dataToCheck, size_t numBytes)
{
    CHOC_ASSERT (dataToCheck != nullptr);
    auto source = static_cast<const char*> (dataToCheck);
    size_t offset = 0;

    for (;;)
    {
        if (offset >= numBytes)
            return nullptr;

        auto byte = static_cast<signed char> (source[offset]);

        if (byte > 0)
        {
            ++offset;
            continue;
        }

        if (byte == 0)
            return nullptr;

        int testBit = 0x40, numExtraBytes = 0;

        while ((byte & testBit) != 0)
        {
            testBit >>= 1;
            ++numExtraBytes;

            if (numExtraBytes > 3
                || offset + static_cast<size_t> (numExtraBytes) >= numBytes
                || (numExtraBytes == 3 && *UTF8Pointer (source + offset) > 0x10ffff))
            {
                numExtraBytes = 0;
                break;
            }
        }

        if (numExtraBytes == 0)
            break;

        ++offset;

        for (int i = 0; i < numExtraBytes; ++i)
            if ((source[offset++] & 0xc0) != 0x80)
                break;
    }

    return source + offset;
}

inline uint32_t UTF8Pointer::operator*() const
{
    return UTF8Pointer (*this).popFirstChar();
}

inline UTF8Pointer& UTF8Pointer::operator++()
{
    CHOC_ASSERT (! empty());  // can't advance past the zero-terminator
    auto firstByte = static_cast<signed char> (*text++);

    if (firstByte >= 0)
        return *this;

    uint32_t testBit = 0x40, unicodeChar = static_cast<unsigned char> (firstByte);

    while ((unicodeChar & testBit) != 0 && testBit > 8)
    {
        ++text;
        testBit >>= 1;
    }

    return *this;
}

inline UTF8Pointer UTF8Pointer::operator++ (int)
{
    auto prev = *this;
    operator++();
    return prev;
}

inline UTF8Pointer UTF8Pointer::operator--()
{
    CHOC_ASSERT (text != nullptr); // mustn't use this on nullptrs
    uint32_t bytesSkipped = 0;

    while ((*--text & 0xc0) == 0x80)
    {
        CHOC_ASSERT (bytesSkipped < 3);
        ++bytesSkipped;
    }

    return *this;
}

inline UTF8Pointer& UTF8Pointer::operator+= (int numCharsToSkip)
{
    CHOC_ASSERT (numCharsToSkip >= 0);

    while (--numCharsToSkip >= 0)
        operator++();

    return *this;
}

inline UTF8Pointer UTF8Pointer::operator+ (int numCharsToSkip) const
{
    auto p = *this;
    p += numCharsToSkip;
    return p;
}

inline uint32_t UTF8Pointer::popFirstChar()
{
    CHOC_ASSERT (text != nullptr); // mustn't use this on nullptrs
    auto firstByte = static_cast<signed char> (*text++);
    uint32_t unicodeChar = static_cast<unsigned char> (firstByte);

    if (firstByte < 0)
    {
        uint32_t bitMask = 0x7f, numExtraBytes = 0;

        for (uint32_t testBit = 0x40; (unicodeChar & testBit) != 0 && testBit > 8; ++numExtraBytes)
        {
            bitMask >>= 1;
            testBit >>= 1;
        }

        unicodeChar &= bitMask;

        for (uint32_t i = 0; i < numExtraBytes; ++i)
        {
            uint32_t nextByte = static_cast<unsigned char> (*text);

            CHOC_ASSERT ((nextByte & 0xc0) == 0x80); // error in the data - you should always make sure the source
                                                        // gets validated before iterating a UTF8Pointer over it

            unicodeChar = (unicodeChar << 6) | (nextByte & 0x3f);
            ++text;
        }
    }

    return unicodeChar;
}

inline bool UTF8Pointer::skipIfStartsWith (char charToMatch)
{
    if (text != nullptr && *text == charToMatch)
    {
        ++text;
        return true;
    }

    return false;
}

inline bool UTF8Pointer::skipIfStartsWith (const char* textToMatch)
{
    CHOC_ASSERT (textToMatch != nullptr);

    if (auto p = text)
    {
        while (*textToMatch != 0)
            if (*textToMatch++ != *p++)
                return false;

        text = p;
        return true;
    }

    return false;
}

inline UTF8Pointer::Iterator UTF8Pointer::begin() const      { CHOC_ASSERT (text != nullptr); return Iterator (text); }
inline UTF8Pointer::EndIterator UTF8Pointer::end() const     { return EndIterator(); }

//==============================================================================
inline uint32_t convertUnicodeCodepointToUTF8 (char* dest, uint32_t unicodeChar)
{
    if (unicodeChar < 0x80)
    {
        *dest = static_cast<char> (unicodeChar);
        return 1;
    }

    uint32_t extraBytes = 1;

    if (unicodeChar >= 0x800)
    {
        ++extraBytes;

        if (unicodeChar >= 0x10000)
            ++extraBytes;
    }

    dest[0] = static_cast<char> ((0xffu << (7 - extraBytes)) | (unicodeChar >> (extraBytes * 6)));

    for (uint32_t i = 1; i <= extraBytes; ++i)
        dest[i] = static_cast<char> (0x80u | (0x3fu & (unicodeChar >> ((extraBytes - i) * 6))));

    return extraBytes + 1;
}

inline bool isUnicodeHighSurrogate (uint32_t codepoint)   { return codepoint >= 0xd800 && codepoint <= 0xdbff; }
inline bool isUnicodeLowSurrogate  (uint32_t codepoint)   { return codepoint >= 0xdc00 && codepoint <= 0xdfff; }

inline uint32_t createUnicodeFromHighAndLowSurrogates (uint32_t codepoint1, uint32_t codepoint2)
{
    if (! isUnicodeHighSurrogate (codepoint1))   return codepoint1;
    if (! isUnicodeLowSurrogate (codepoint2))    return 0;

    return (codepoint1 << 10) + codepoint2 - 0x35fdc00u;
}

} // namespace choc::text

#endif
