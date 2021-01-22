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

static bool isFollowedByBlankLine (CodeLocation pos)
{
    return choc::text::trimEnd (pos.getSourceLine()).empty()
        || choc::text::trimEnd (pos.getStartOfNextLine().getSourceLine()).empty();
}

struct SimpleTokeniser  : public SOULTokeniser
{
    SimpleTokeniser (const CodeLocation& start) { initialise (start); }

    [[noreturn]] void throwError (const CompileMessage& message) const override
    {
        location.throwError (message);
    }

    static CodeLocation findNext (CodeLocation start, TokenType target)
    {
        try
        {
            SimpleTokeniser tokeniser (start);

            while (! tokeniser.matches (Token::eof))
            {
                if (tokeniser.matches (target))
                    return tokeniser.location;

                tokeniser.skip();
            }
        }
        catch (const AbortCompilationException&) {}

        return {};
    }

    static CodeLocation findEndOfMatchingDelimiter (const CodeLocation& start, TokenType openDelim, TokenType closeDelim)
    {
        try
        {
            SimpleTokeniser tokeniser (start);
            SOUL_ASSERT (tokeniser.matches (openDelim));
            int depth = 0;

            for (;;)
            {
                auto token = tokeniser.skip();

                if (token == openDelim)
                {
                    ++depth;
                }
                else if (token == closeDelim)
                {
                    if (--depth == 0)
                        return tokeniser.location;
                }
                else if (token == Token::eof)
                {
                    break;
                }
            }
        }
        catch (const AbortCompilationException&) {}

        return {};
    }
};

CodeLocation SourceCodeUtilities::findEndOfMatchingBrace (CodeLocation start) { return SimpleTokeniser::findEndOfMatchingDelimiter (start, Operator::openBrace, Operator::closeBrace); }
CodeLocation SourceCodeUtilities::findEndOfMatchingParen (CodeLocation start) { return SimpleTokeniser::findEndOfMatchingDelimiter (start, Operator::openParen, Operator::closeParen); }

SourceCodeUtilities::Comment SourceCodeUtilities::getFileSummaryComment (CodeLocation file)
{
    auto firstComment = parseComment (file);

    if (firstComment.isDoxygenStyle && isFollowedByBlankLine (firstComment.end))
        return firstComment;

    if (firstComment.valid)
    {
        auto secondComment = parseComment (firstComment.end);

        if (secondComment.isDoxygenStyle && isFollowedByBlankLine (secondComment.end))
            return secondComment;
    }

    return {};
}

std::string SourceCodeUtilities::getFileSummaryTitle (const Comment& summary)
{
    if (summary.valid && ! summary.lines.empty())
    {
        auto firstLine = choc::text::trim (summary.lines[0]);

        if (choc::text::startsWith (toLowerCase (firstLine), "title:"))
        {
            auto title = choc::text::trim (firstLine.substr (6));

            if (choc::text::endsWith (title, "."))
                title = title.substr (title.length() - 1);

            return title;
        }
    }

    return {};
}

std::string SourceCodeUtilities::getFileSummaryBody (const Comment& summary)
{
    if (summary.valid && ! summary.lines.empty())
    {
        auto firstLine = choc::text::trim (summary.lines[0]);

        if (choc::text::startsWith (toLowerCase (firstLine), "title:"))
        {
            auto copy = summary;
            copy.lines.erase (copy.lines.begin());

            while (! copy.lines.empty() && copy.lines.front().empty())
                copy.lines.erase (copy.lines.begin());

            return  copy.getText();
        }
    }

    return summary.getText();
}

CodeLocation SourceCodeUtilities::findStartOfPrecedingComment (CodeLocation location)
{
    auto prevLineStart = location.getStartOfPreviousLine();

    if (prevLineStart.isEmpty())
        return location;

    auto prevLine = prevLineStart.getSourceLine();

    if (choc::text::startsWith (choc::text::trimStart (prevLine), "//"))
    {
        for (auto start = prevLineStart;;)
        {
            auto next = start.getStartOfPreviousLine();

            if (next.isEmpty() || ! choc::text::startsWith (choc::text::trimStart (next.getSourceLine()), "//"))
                return start;

            start = next;
        }
    }

    if (choc::text::endsWith (choc::text::trimEnd (prevLine), "*/"))
    {
        auto fileStart = prevLineStart.sourceCode->utf8;
        auto start = prevLineStart;
        start.location += static_cast<int> (choc::text::trimEnd (prevLine).length() - 2);

        if (start.location > fileStart + 1)
        {
            --(start.location);
            --(start.location);

            for (;;)
            {
                if (start.location.startsWith ("/*"))
                    return start;

                if (start.location > fileStart)
                    --(start.location);
                else
                    break;
            }
        }
    }

    return location;
}

SourceCodeUtilities::Comment SourceCodeUtilities::parseComment (CodeLocation pos)
{
    if (pos.isEmpty())
        return {};

    Comment result;
    pos.location = pos.location.findEndOfWhitespace();
    result.start = pos;

    if (pos.location.advanceIfStartsWith ("/*"))
    {
        result.valid = true;
        result.isStarSlash = true;

        while (*pos.location == '*')
        {
            result.isDoxygenStyle = true;
            ++(pos.location);
        }
    }
    else if (pos.location.advanceIfStartsWith ("//"))
    {
        result.valid = true;
        result.isStarSlash = false;

        while (*pos.location == '/')
        {
            result.isDoxygenStyle = true;
            ++(pos.location);
        }
    }
    else
    {
        return {};
    }

    if (pos.location.advanceIfStartsWith ("<"))
        result.isReferringBackwards = true;

    while (*pos.location == ' ')
        ++(pos.location);

    if (result.isStarSlash)
    {
        auto closeComment = pos.location.find ("*/");

        if (closeComment.isEmpty())
            return {};

        result.lines = choc::text::splitIntoLines (std::string (pos.location.getAddress(),
                                                                closeComment.getAddress()),
                                                   false);

        auto firstLineIndent = pos.location.getAddress() - pos.getStartOfLine().location.getAddress();

        for (auto& l : result.lines)
        {
            l = choc::text::trimEnd (l);
            auto leadingSpacesOnLine = l.length() - choc::text::trimStart (l).length();

            if (firstLineIndent > 0 && leadingSpacesOnLine >= (size_t) firstLineIndent)
                l = l.substr ((size_t) firstLineIndent);
        }

        result.end = pos;
        result.end.location = closeComment;
        result.end.location += 2;
    }
    else
    {
        for (;;)
        {
            auto line = choc::text::trim (pos.getSourceLine());

            if (! choc::text::startsWith (line, "//"))
                break;

            line = line.substr (2);

            while (! line.empty() && line[0] == '/')
                line = line.substr (1);

            result.lines.push_back (line);
            pos = pos.getStartOfNextLine();
        }

        result.end = pos;

        if (! result.lines.empty())
        {
            auto countLeadingSpaces = [] (std::string_view s) -> size_t
            {
                for (size_t i = 0; i < s.length(); ++i)
                    if (s[i] != ' ')
                        return i;

                return 0;
            };

            size_t leastLeadingSpace = 1000;

            for (auto& l : result.lines)
                leastLeadingSpace = std::min (leastLeadingSpace, countLeadingSpaces (l));

            if (leastLeadingSpace != 0)
                for (auto& l : result.lines)
                    l = l.substr (leastLeadingSpace);
        }
    }

    removeIf (result.lines, [] (const std::string& s) { return choc::text::contains (s, "================")
                                                            || choc::text::contains (s, "****************"); });

    while (! result.lines.empty() && result.lines.back().empty())
        result.lines.erase (result.lines.end() - 1);

    while (! result.lines.empty() && result.lines.front().empty())
        result.lines.erase (result.lines.begin());

    return result;
}

std::string SourceCodeUtilities::Comment::getText() const
{
    return joinStrings (lines, "\n");
}

} // namespace soul
