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

#ifndef CHOC_CODE_PRINTER_HEADER_INCLUDED
#define CHOC_CODE_PRINTER_HEADER_INCLUDED

#include <string>
#include <vector>
#include "choc_FloatToString.h"
#include "choc_StringUtilities.h"

namespace choc::text
{

/**
    A special stream for creating indented source code text.
*/
struct CodePrinter
{
    CodePrinter() = default;
    ~CodePrinter() = default;
    CodePrinter (CodePrinter&&) = default;
    CodePrinter (const CodePrinter&) = default;

    /** Returns the finished contents of the stream as a string. */
    std::string toString() const;

    //==============================================================================
    CodePrinter& operator<< (const char*);
    CodePrinter& operator<< (const std::string&);
    CodePrinter& operator<< (std::string_view);
    CodePrinter& operator<< (char);
    CodePrinter& operator<< (double);
    CodePrinter& operator<< (float);

    /** Prints any kind of integer type. */
    template <typename IntegerType>
    CodePrinter& operator<< (IntegerType);

    /** This class is used as a sentinel which when passed to the `<<` operator, writes a new-line. */
    struct NewLine {};
    /** This class is used as a sentinel which when passed to the `<<` operator, writes a blank line. */
    struct BlankLine {};
    /** This class is used as a sentinel which when passed to the `<<` operator, writes a section break. */
    struct SectionBreak {};

    /** Emits a new-line */
    CodePrinter& operator<< (const NewLine&);
    /** Emits either one or two new-lines, in order to leave exactly one blank line */
    CodePrinter& operator<< (const BlankLine&);
    /** Emits the section-break text. @see setSectionBreak() */
    CodePrinter& operator<< (const SectionBreak&);

    //==============================================================================
    /** Sets the current tab size. Note that this won't modify any previously-written lines,
        it just affects the subsequent printing.
    */
    void setTabSize (size_t numSpaces);
    /** Sets a text string to use as a section break - the default is a commented-out line of dashes. */
    void setSectionBreak (std::string newSectionBreakString);
    /** Modifies the new-line sequence.
        By default this is just a `\n`, but you may want to change it to a `\r\n` sequence.
    */
    void setNewLine (const char* newLineSequence);
    /** This sets a line length limit, so that longer lines will be wrapped where possible.
        Setting this to 0 turns off line-wrap.
    */
    void setLineWrapLength (size_t lineWrapCharacters);
    /** Returns the current line-wrap length, where 0 means no wrapping. */
    size_t getLineWrapLength() const            { return lineWrapLength; }

    //==============================================================================
    /** An RAII class which resets the indent level when it is deleted. */
    struct Indent;

    /** Returns an Indent object which adds an indentation of the default amount. */
    [[nodiscard]] Indent createIndent();
    /** Returns an Indent object which adds an indentation of the given amount. */
    [[nodiscard]] Indent createIndent (size_t numSpaces);
    /** Returns an Indent object which modifies the indent level and adds a pair of brace characters around the block. */
    [[nodiscard]] Indent createIndent (char openBrace, char closeBrace);
    /** Returns an Indent object which modifies the indent level and adds a pair of brace characters around the block. */
    [[nodiscard]] Indent createIndent (size_t numSpaces, char openBrace, char closeBrace);
    /** Returns an Indent object which modifies the indent level and adds default curly-braces around the block. */
    [[nodiscard]] Indent createIndentWithBraces();
    /** Returns an Indent object which modifies the indent level and adds default curly-braces around the block. */
    [[nodiscard]] Indent createIndentWithBraces (size_t numSpaces);

    /** Returns the total number of spaces that will be used for indenting the current line. */
    size_t getTotalIndent() const;
    /** Adds or removes some indentation to the current level. (The Indent class provides a better
        way to indent blocks than manually changing this value). @see createIndent()
    */
    void addIndent (int spacesToAdd);
    /** Modifies the current total indent level. @see createIndent() */
    void setTotalIndent (size_t newTotalNumSpaces);

private:
    struct Line
    {
        size_t indent;
        std::string line;
    };

    std::vector<Line> lines;
    int indent = 0, tabSize = 4;
    size_t lineWrapLength = 0;
    std::string newLineString = "\n",
                sectionBreakString = "//==============================================================================";

    void append (std::string);
    void writeBlock (std::string_view);
    void startNewLine();
    bool isLastLineEmpty() const;
    bool isLastLineActive() const;
    bool lastLineIsSectionBreak() const;
};


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

inline CodePrinter& CodePrinter::operator<< (const char* s)            { writeBlock (s); return *this; }
inline CodePrinter& CodePrinter::operator<< (const std::string& s)     { writeBlock (s); return *this; }
inline CodePrinter& CodePrinter::operator<< (std::string_view s)       { writeBlock (s); return *this; }
inline CodePrinter& CodePrinter::operator<< (char c)                   { char s[2] = { c, 0 }; append (s); return *this; }
inline CodePrinter& CodePrinter::operator<< (double v)                 { append (choc::text::floatToString (v)); return *this; }
inline CodePrinter& CodePrinter::operator<< (float v)                  { append (choc::text::floatToString (v)); return *this; }
inline CodePrinter& CodePrinter::operator<< (const NewLine&)           { startNewLine(); return *this; }
inline CodePrinter& CodePrinter::operator<< (const BlankLine&)         { if (! isLastLineEmpty()) { if (isLastLineActive()) startNewLine(); startNewLine(); } return *this; }
inline CodePrinter& CodePrinter::operator<< (const SectionBreak&)      { if (! lastLineIsSectionBreak()) { *this << BlankLine(); append (sectionBreakString + newLineString); } return *this; }

template <typename IntegerType>
CodePrinter& CodePrinter::operator<< (IntegerType v)
{
    static_assert (std::is_integral<IntegerType>::value, "You're probably trying to write a type that's not supported");
    append (std::to_string (v));
    return *this;
}

static inline size_t getLengthWithTrimmedEnd (const std::string& s)
{
    auto len = s.length();

    while (len != 0 && choc::text::isWhitespace (s[len - 1]))
        --len;

    return len;
}

inline void CodePrinter::setTabSize (size_t newSize)                   { tabSize = static_cast<int> (newSize); }
inline void CodePrinter::setNewLine (const char* newLineBreak)         { newLineString = newLineBreak; }
inline void CodePrinter::setLineWrapLength (size_t len)                { lineWrapLength = len; }
inline void CodePrinter::setSectionBreak (std::string newBreakString)  { sectionBreakString = std::move (newBreakString); }
inline void CodePrinter::startNewLine()                                { append ("\n"); }
inline bool CodePrinter::isLastLineEmpty() const                       { return lines.empty() || getLengthWithTrimmedEnd (lines.back().line) == 0; }
inline bool CodePrinter::isLastLineActive() const                      { return ! lines.empty() && lines.back().line.back() != '\n'; }
inline bool CodePrinter::lastLineIsSectionBreak() const                { return ! lines.empty() && lines.back().line == sectionBreakString + newLineString; }

inline size_t CodePrinter::getTotalIndent() const                      { return static_cast<size_t> (indent); }
inline void CodePrinter::setTotalIndent (size_t newIndent)             { indent = static_cast<int> (newIndent); }
inline void CodePrinter::addIndent (int spacesToAdd)                   { indent += spacesToAdd; CHOC_ASSERT (indent >= 0); }

struct CodePrinter::Indent
{
    Indent (Indent&& other)
      : owner (other.owner), amount (other.amount),
        openBrace (other.openBrace), closeBrace (other.closeBrace)
    {
        other.amount = 0;
        other.closeBrace = 0;
    }

    Indent (const Indent&) = delete;

    ~Indent()
    {
        owner.addIndent (-amount);

        if (closeBrace != 0)
            owner << closeBrace;
    }

private:
    friend struct CodePrinter;

    CodePrinter& owner;
    int amount;
    char openBrace = 0, closeBrace = 0;

    Indent (CodePrinter& p, int size, char ob, char cb) : owner (p), amount (size), openBrace (ob), closeBrace (cb)
    {
        if (openBrace != 0)
            owner << openBrace << NewLine();

        owner.addIndent (size);
    }
};

inline CodePrinter::Indent CodePrinter::createIndent()                                { return createIndent (0, 0); }
inline CodePrinter::Indent CodePrinter::createIndent (size_t size)                    { return createIndent (size, 0, 0); }
inline CodePrinter::Indent CodePrinter::createIndent (char ob, char cb)               { return createIndent (static_cast<size_t> (tabSize), ob, cb); }
inline CodePrinter::Indent CodePrinter::createIndent (size_t size, char ob, char cb)  { return Indent (*this, static_cast<int> (size), ob, cb); }
inline CodePrinter::Indent CodePrinter::createIndentWithBraces()                      { return createIndent ('{', '}'); }
inline CodePrinter::Indent CodePrinter::createIndentWithBraces (size_t size)          { return createIndent (size, '{', '}'); }

inline std::string CodePrinter::toString() const
{
    std::string s;
    auto totalLen = lines.size() * newLineString.length() + 1;

    for (auto& l : lines)
        if (auto contentLen = getLengthWithTrimmedEnd (l.line))
            totalLen += l.indent + contentLen;

    s.reserve (totalLen);

    for (auto& l : lines)
    {
        if (auto contentLen = getLengthWithTrimmedEnd (l.line))
        {
            s.append (l.indent, ' ');
            s.append (l.line.begin(), l.line.begin() + static_cast<std::string::difference_type> (contentLen));
        }

        s.append (newLineString);
    }

    return s;
}

static inline size_t findLineSplitPoint (std::string_view text, size_t targetLength)
{
    size_t pos = 0;
    char currentQuote = 0;
    auto canBreakAfterChar = [] (char c)  { return c == ' ' || c == '\t' || c == ',' || c == ';' || c == '\n'; };

    for (;;)
    {
        if (pos == text.length())
            return pos;

        auto c = text[pos++];

        if (pos >= targetLength && currentQuote == 0 && canBreakAfterChar (c))
            return pos;

        if (c == '"' || c == '\'')
        {
            if (currentQuote == 0)      currentQuote = c;
            else if (currentQuote == c) currentQuote = 0;
        }
    }
}

inline void CodePrinter::append (std::string s)
{
    if (! s.empty())
    {
        if (isLastLineActive())
            lines.back().line += std::move (s);
        else
            lines.push_back ({ static_cast<size_t> (indent), std::move (s) });

        while (lineWrapLength != 0 && lines.back().line.length() > lineWrapLength)
        {
            auto& last = lines.back().line;
            auto splitPoint = findLineSplitPoint (last, lineWrapLength);

            if (splitPoint >= last.size())
                break;

            auto newLastLine = last.substr (splitPoint);
            last = last.substr (0, splitPoint);
            lines.push_back ({ static_cast<size_t> (indent), std::move (newLastLine) });
        }
    }
}

inline void CodePrinter::writeBlock (std::string_view text)
{
    auto lineStart = text.begin();
    auto end = text.end();

    for (auto i = lineStart; i != end; ++i)
    {
        if (*i == '\n')
        {
            auto nextLine = i + 1;
            append ({ lineStart, nextLine });
            lineStart = nextLine;
        }
    }

    append ({ lineStart, end });
}

} // namespace choc::text

#endif
