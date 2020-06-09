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

CodeLocation CodeLocation::createFromSourceFile (const BuildBundle::SourceFile& f)
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

std::string CodeLocation::getSourceLine() const
{
    if (location.getAddress() == nullptr)
        return {};

    auto start = location;
    auto end = location;

    while (! end.isEmpty())
    {
        auto c = end.getAndAdvance();

        if (c == '\r' || c == '\n')
            break;
    }

    while (start > sourceCode->utf8)
    {
        auto prev = start;
        auto c = *--prev;

        if (c == '\r' || c == '\n')
            break;

        start = prev;
    }

    return { start.getAddress(), end.getAddress() };
}

void CodeLocation::emitMessage (CompileMessage message) const
{
    soul::emitMessage (message.withLocation (*this));
}

[[noreturn]] void CodeLocation::throwError (CompileMessage message) const
{
    soul::throwError (message.withLocation (*this));
}


} // namespace soul
