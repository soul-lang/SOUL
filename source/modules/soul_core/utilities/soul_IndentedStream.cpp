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

IndentedStream::ScopedIndent::ScopedIndent (ScopedIndent&& other) : owner (other.owner), amount (other.amount) { other.amount = 0; }

IndentedStream::ScopedIndent::ScopedIndent (IndentedStream& s, int numChars, bool braced)
    : owner (s), amount (numChars), isBraced (braced)
{
    if (isBraced)
        owner << "{" << newLine;

    owner.indent (numChars);
}

IndentedStream::ScopedIndent::~ScopedIndent()
{
    owner.indent (-amount);

    if (isBraced)
        owner << "}";
}

IndentedStream::IndentedStream() = default;

std::string IndentedStream::toString() const                                    { return content.str(); }
void IndentedStream::setTotalIndent (int numChars)                              { indentSize = numChars; }
int IndentedStream::getTotalIndent() const noexcept                             { return currentIndent; }
IndentedStream::ScopedIndent IndentedStream::createIndent()                     { return createIndent (indentSize); }
IndentedStream::ScopedIndent IndentedStream::createIndent (int numChars)        { return ScopedIndent (*this, numChars, false); }
IndentedStream::ScopedIndent IndentedStream::createBracedIndent()               { return ScopedIndent (*this, indentSize, true); }
IndentedStream::ScopedIndent IndentedStream::createBracedIndent (int numChars)  { return ScopedIndent (*this, numChars, true); }
static constexpr size_t maxLineLength = 150;

void IndentedStream::write (const std::string& text)
{
    if (! text.empty())
    {
        if (! startsWith (text, "}"))
            printSectionBreakIfNeeded();

        writeIndentIfNeeded();
        lastLineWasBlank = false;
        currentLineIsEmpty = false;
        content << text;
    }
}

void IndentedStream::writeLines (ArrayView<std::string> lines)
{
    bool first = true;

    for (auto line : lines)
    {
        if (first)
            first = false;
        else
            *this << newLine;

        write (trimStart (line));
    }
}

IndentedStream& IndentedStream::operator<< (const std::string& text)
{
    if (contains (text, '\n'))
    {
        writeLines (splitAtDelimiter (text, '\n'));
        return *this;
    }

    if (text.length() > maxLineLength)
        writeLines (splitLinesOfCode (text, maxLineLength));
    else
        write (text);

    return *this;
}

IndentedStream& IndentedStream::operator<< (const char* text)
{
    SOUL_ASSERT (text != nullptr);
    SOUL_ASSERT (! contains (ArrayView<char> (text, strlen (text)), '\n'));

    if (*text != 0)
        *this << std::string (text);

    return *this;
}

IndentedStream& IndentedStream::operator<< (double value)   { return *this << doubleToAccurateString (value); }
IndentedStream& IndentedStream::operator<< (float value)    { return *this << floatToAccurateString (value); }
IndentedStream& IndentedStream::operator<< (size_t value)   { return *this << std::to_string (value); }

IndentedStream& IndentedStream::operator<< (const NewLine&)
{
    content << std::endl;
    indentNeeded = true;
    lastLineWasBlank = currentLineIsEmpty;
    currentLineIsEmpty = true;
    return *this;
}

IndentedStream& IndentedStream::operator<< (const BlankLine&)
{
    while (! lastLineWasBlank)
        *this << newLine;

    return *this;
}

IndentedStream& IndentedStream::operator<< (char c)
{
    char n[2] = { c, 0 };
    return operator<< ((const char*) n);
}

void IndentedStream::writeMultipleLines (const std::string& text)
{
    writeMultipleLines (text.c_str());
}

void IndentedStream::writeMultipleLines (const char* text)
{
    auto lineStart = text;

    while (*text != 0)
    {
        if (*text == '\r')
        {
            ++text;
            continue;
        }

        if (*text == '\n')
        {
            *this << trimEnd (std::string (lineStart, text)) << newLine;
            lineStart = ++text;
            continue;
        }

        ++text;
    }
}

void IndentedStream::indent (int amount)
{
    currentIndent += amount;
    SOUL_ASSERT (currentIndent >= 0);
}

void IndentedStream::writeIndentIfNeeded()
{
    if (indentNeeded)
    {
        indentNeeded = false;
        *this << std::string ((size_t) currentIndent, ' ');
    }
}

void IndentedStream::insertSectionBreak()
{
    sectionBreakNeeded = true;
}

void IndentedStream::printSectionBreakIfNeeded()
{
    if (sectionBreakNeeded)
    {
        sectionBreakNeeded = false;

        *this << blankLine
              << "//==============================================================================" << newLine;
    }
}


} // namespace soul
