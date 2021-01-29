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

//==============================================================================
SourceCodeText::SourceCodeText (std::string file, std::string text, bool internal)
   : filename (std::move (file)),
     content (std::move (text)),
     utf8 (content.c_str()),
     isInternal (internal)
{}

SourceCodeText::Ptr SourceCodeText::createForFile (std::string file, std::string text)
{
    return Ptr (*new SourceCodeText (std::move (file), std::move (text), false));
}

SourceCodeText::Ptr SourceCodeText::createInternal (std::string name, std::string text)
{
    return Ptr (*new SourceCodeText (std::move (name), std::move (text), true));
}

//==============================================================================
CodeLocation::CodeLocation (SourceCodeText::Ptr code)  : sourceCode (std::move (code)), location (sourceCode->utf8) {}

/** This is the best way to convert a string to a CodeLocation as it'll also validate the UTF8 and throw
    an error if it's dodgy.
*/
CodeLocation CodeLocation::createFromString (std::string filename, std::string text)
{
    CodeLocation code (SourceCodeText::createForFile (std::move (filename), std::move (text)));
    code.validateUTF8();
    return code;
}

CodeLocation CodeLocation::createFromSourceFile (const SourceFile& f)
{
    return createFromString (f.filename, f.content);
}

void CodeLocation::validateUTF8() const
{
    if (auto invalidBytes = location.findInvalidData())
    {
        CodeLocation c (*this);
        c.location = UTF8Reader (invalidBytes);
        c.throwError (Errors::invalidUTF8());
    }
}

bool CodeLocation::isEmpty() const
{
    return sourceCode == nullptr || sourceCode->content.empty();
}

std::string CodeLocation::getFilename() const
{
    if (sourceCode != nullptr)
        return sourceCode->filename;

    return {};
}

size_t CodeLocation::getByteOffsetInFile() const
{
    auto diff = location.getAddress() - sourceCode->utf8.getAddress();
    SOUL_ASSERT (diff >= 0);
    return static_cast<size_t> (diff);
}

CodeLocation::LineAndColumn CodeLocation::getLineAndColumn() const
{
    if (sourceCode == nullptr)
        return { 0, 0 };

    LineAndColumn lc = { 1, 1 };

    for (auto i = sourceCode->utf8; i < location && ! i.isEmpty(); ++i)
    {
        ++lc.column;
        if (*i == '\n')  { lc.column = 1; lc.line++; }
    }

    return lc;
}

CodeLocation CodeLocation::getOffset (uint32_t linesToAdd, uint32_t columnsToAdd) const
{
    auto l = *this;
    LineAndColumn lc;

    for (;;)
    {
        if (lc.line == linesToAdd && lc.column == columnsToAdd)
            return l;

        if (l.location.isEmpty())
            return {};

        ++lc.column;
        if (*(l.location) == '\n')  { lc.column = 0; lc.line++; }
        ++(l.location);
    }
}

CodeLocation CodeLocation::getStartOfLine() const
{
    if (location.getAddress() == nullptr)
        return {};

    auto l = *this;
    auto start = sourceCode->utf8;

    while (l.location > start)
    {
        auto prev = l.location;
        auto c = *--prev;

        if (c == '\r' || c == '\n')
            break;

        l.location = prev;
    }

    return l;
}

CodeLocation CodeLocation::getEndOfLine() const
{
    if (location.getAddress() == nullptr)
        return {};

    auto l = *this;

    while (! l.location.isEmpty())
    {
        auto c = l.location.getAndAdvance();

        if (c == '\r' || c == '\n')
            break;
    }

    return l;
}

CodeLocation CodeLocation::getStartOfNextLine() const
{
    for (auto l = *this;;)
    {
        if (l.location.isEmpty())
            return {};

        if (l.location.getAndAdvance() == '\n')
            return l;
    }
}

CodeLocation CodeLocation::getStartOfPreviousLine() const
{
    auto l = getStartOfLine();

    if (l.location.getAddress() == nullptr)
        return {};

    if (l.location <= sourceCode->utf8)
        return {};

    --(l.location);
    return l.getStartOfLine();
}

std::string CodeLocation::getSourceLine() const
{
    if (location.getAddress() == nullptr)
        return {};

    auto s = getStartOfLine();
    auto e = getEndOfLine();

    return { s.location.getAddress(),
             e.location.getAddress() };
}

void CodeLocation::emitMessage (CompileMessage message) const
{
    soul::emitMessage (message.withLocation (*this));
}

[[noreturn]] void CodeLocation::throwError (CompileMessage message) const
{
    soul::throwError (message.withLocation (*this));
}

bool CodeLocationRange::isEmpty() const
{
    return start.sourceCode == nullptr || start.location.getAddress() == end.location.getAddress();
}

std::string CodeLocationRange::toString() const
{
    if (start.sourceCode == nullptr)
    {
        SOUL_ASSERT (end.sourceCode == nullptr);
        return {};
    }

    SOUL_ASSERT (end.sourceCode != nullptr && end.location.getAddress() >= start.location.getAddress());
    return std::string (start.location.getAddress(), end.location.getAddress());
}

} // namespace soul
