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

#ifndef CHOC_STRING_UTILS_HEADER_INCLUDED
#define CHOC_STRING_UTILS_HEADER_INCLUDED

#include <string>
#include <cmath>
#include <assert.h>

#ifndef CHOC_ASSERT
 #define CHOC_ASSERT(x)  assert(x);
#endif

namespace choc::text
{


//==============================================================================
/** Converts a hex character to a number 0-15, or -1 if it's not a valid hex digit. */
int hexDigitToInt (uint32_t unicodeChar);

inline bool isWhitespace (char c)                               { return c == ' ' || (c <= 13 && c >= 9); }

/** If the given character is at the start and end of the string, it trims it away. */
std::string removeOuterCharacter (std::string text, char outerChar);

inline std::string removeDoubleQuotes (std::string text)       { return removeOuterCharacter (std::move (text), '"'); }
inline std::string removeSingleQuotes (std::string text)       { return removeOuterCharacter (std::move (text), '\''); }

inline std::string addDoubleQuotes (std::string text)          { return "\"" + std::move (text) + "\""; }
inline std::string addSingleQuotes (std::string text)          { return "'" + std::move (text) + "'"; }

template <typename IsDelimiterChar>
std::vector<std::string> splitString (std::string_view textToSplit,
                                      IsDelimiterChar&& isDelimiterChar,
                                      bool includeDelimitersInResult);

template <typename CharStartsDelimiter, typename CharIsInDelimiterBody>
std::vector<std::string> splitString (std::string_view textToSplit,
                                      CharStartsDelimiter&& isDelimiterStart,
                                      CharIsInDelimiterBody&& isDelimiterBody,
                                      bool includeDelimitersInResult);

std::vector<std::string> splitString (std::string_view textToSplit,
                                      char delimiterCharacter,
                                      bool includeDelimitersInResult);

std::vector<std::string> splitAtWhitespace (std::string_view text, bool keepDelimiters = false);

/** Splits a string at newline characters, returning an array of strings. */
std::vector<std::string> splitIntoLines (std::string_view text, bool includeNewLinesInResult);

/** Calculates the Levenstein distance between two strings. */
template <typename StringType>
size_t getLevenshteinDistance (const StringType& string1,
                               const StringType& string2);




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

inline int hexDigitToInt (uint32_t c)
{
    auto d1 = c -  static_cast<uint32_t> ('0');         if (d1 < 10u)  return static_cast<int> (d1);
    auto d2 = d1 + static_cast<uint32_t> ('0' - 'a');   if (d2 < 6u)   return static_cast<int> (d2 + 10);
    auto d3 = d2 + static_cast<uint32_t> ('a' - 'A');   if (d3 < 6u)   return static_cast<int> (d3 + 10);
    return -1;
}

inline std::string removeOuterCharacter (std::string t, char outerChar)
{
    if (t.length() >= 2 && t.front() == outerChar && t.back() == outerChar)
        return t.substr (1, t.length() - 2);

    return t;
}

template <typename CharStartsDelimiter, typename CharIsInDelimiterBody>
std::vector<std::string> splitString (std::string_view source,
                                      CharStartsDelimiter&& isDelimiterStart,
                                      CharIsInDelimiterBody&& isDelimiterBody,
                                      bool keepDelimiters)
{
    std::vector<std::string> tokens;
    auto tokenStart = source.begin();
    auto pos = tokenStart;

    while (pos != source.end())
    {
        if (isDelimiterStart (*pos))
        {
            auto delimiterStart = pos++;

            while (pos != source.end() && isDelimiterBody (*pos))
                ++pos;

            if (pos != source.begin())
                tokens.push_back ({ tokenStart, keepDelimiters ? pos : delimiterStart });

            tokenStart = pos;
        }
        else
        {
            ++pos;
        }
    }

    if (pos != source.begin())
        tokens.push_back ({ tokenStart, pos });

    return tokens;
}

template <typename IsDelimiterChar>
std::vector<std::string> splitString (std::string_view source, IsDelimiterChar&& isDelimiterChar, bool keepDelimiters)
{
    std::vector<std::string> tokens;
    auto tokenStart = source.begin();
    auto pos = tokenStart;

    while (pos != source.end())
    {
        if (isDelimiterChar (*pos))
        {
            tokens.push_back ({ tokenStart, keepDelimiters ? pos + 1 : pos });
            tokenStart = ++pos;
        }
        else
        {
            ++pos;
        }
    }

    if (pos != source.begin())
        tokens.push_back ({ tokenStart, pos });

    return tokens;
}

inline std::vector<std::string> splitString (std::string_view text, char delimiterCharacter, bool keepDelimiters)
{
    return splitString (text, [=] (char c) { return c == delimiterCharacter; }, keepDelimiters);
}

inline std::vector<std::string> splitAtWhitespace (std::string_view text, bool keepDelimiters)
{
    return splitString (text,
                        [] (char c) { return isWhitespace (c); },
                        [] (char c) { return isWhitespace (c); },
                        keepDelimiters);
}

inline std::vector<std::string> splitIntoLines (std::string_view text, bool includeNewLinesInResult)
{
    return splitString (text, '\n', includeNewLinesInResult);
}


template <typename StringType>
size_t getLevenshteinDistance (const StringType& string1, const StringType& string2)
{
    if (string1.empty())  return string2.length();
    if (string2.empty())  return string1.length();

    auto calculate = [] (size_t* costs, size_t numCosts, const StringType& s1, const StringType& s2) -> size_t
    {
        for (size_t i = 0; i < numCosts; ++i)
            costs[i] = i;

        size_t p1 = 0;

        for (auto c1 : s1)
        {
            auto corner = p1;
            *costs = p1 + 1;
            size_t p2 = 0;

            for (auto c2 : s2)
            {
                auto upper = costs[p2 + 1];
                costs[p2 + 1] = c1 == c2 ? corner : (std::min (costs[p2], std::min (upper, corner)) + 1);
                ++p2;
                corner = upper;
            }

            ++p1;
        }

        return costs[numCosts - 1];
    };

    auto sizeNeeded = string2.length() + 1;
    constexpr size_t maxStackSize = 96;

    if (sizeNeeded <= maxStackSize)
    {
        size_t costs[maxStackSize];
        return calculate (costs, sizeNeeded, string1, string2);
    }

    std::unique_ptr<size_t[]> costs (new size_t[sizeNeeded]);
    return calculate (costs.get(), sizeNeeded, string1, string2);
}


} // namespace choc::text

#endif
