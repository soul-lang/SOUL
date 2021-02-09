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

struct SimpleTokeniser  : public SOULTokeniser
{
    SimpleTokeniser (const CodeLocation& start, bool ignoreComments)
    {
        shouldIgnoreComments = ignoreComments;
        initialise (start);
    }

    [[noreturn]] void throwError (const CompileMessage& message) const override
    {
        location.throwError (message);
    }

    bool skipPastMatchingCloseDelimiter (TokenType openDelim, TokenType closeDelim)
    {
        int depth = 0;

        for (;;)
        {
            if (matches (Token::eof))
                return false;

            auto token = skip();

            if (token == openDelim)
            {
                ++depth;
            }
            else if (token == closeDelim)
            {
                if (--depth == 0)
                    return true;
            }
        }
    }

    static CodeLocation findNext (CodeLocation start, TokenType target)
    {
        try
        {
            SimpleTokeniser tokeniser (start, true);

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
            SimpleTokeniser tokeniser (start, true);
            SOUL_ASSERT (tokeniser.matches (openDelim));

            if (tokeniser.skipPastMatchingCloseDelimiter (openDelim, closeDelim))
                return tokeniser.location;
        }
        catch (const AbortCompilationException&) {}

        return {};
    }
};

static bool isFollowedByBlankLine (CodeLocation pos)
{
    return choc::text::trimEnd (pos.getSourceLine()).empty()
        || choc::text::trimEnd (pos.getStartOfNextLine().getSourceLine()).empty();
}

CodeLocation SourceCodeUtilities::findNextOccurrence (CodeLocation start, char character)
{
    for (auto pos = start;; ++(pos.location))
    {
        auto c = *(pos.location);

        if (c == static_cast<decltype(c)> (character))
            return pos;

        if (c == 0)
            return {};
    }
}

pool_ptr<AST::ASTObject> SourceCodeUtilities::findASTObjectAt (ArrayView<pool_ref<AST::ModuleBase>> modulesToSearch,
                                                               CodeLocation targetLocation)
{
    struct FindLocationVisitor  : public ASTVisitor
    {
        void visitObject (AST::ASTObject& o) override
        {
            ASTVisitor::visitObject (o);

            auto distance = target - o.context.location.location.getAddress();

            if (distance == 0
                 || (distance > 0
                      && result != nullptr
                      && distance < target - result->context.location.location.getAddress()))
                result = o;
        }

        const char* target = nullptr;
        pool_ptr<AST::ASTObject> result;
    };

    FindLocationVisitor v;
    v.target = targetLocation.location.getAddress();

    for (auto& m : modulesToSearch)
    {
        v.ASTVisitor::visitObject (m.get());

        if (v.result != nullptr)
            break;
    }

    return v.result;
}

CodeLocationRange SourceCodeUtilities::findRangeOfASTObject (AST::ASTObject& object)
{
    struct FindLexicalRangeVisitor  : public ASTVisitor
    {
        void visitObject (AST::ASTObject& o) override
        {
            ASTVisitor::visitObject (o);

            auto loc = o.context.location;

            if (loc.location < range.start.location)  range.start = loc;
            if (loc.location > range.end.location)    range.end = loc;
        }

        CodeLocationRange range;
    };

    FindLexicalRangeVisitor v;
    v.range.start = v.range.end = object.context.location;
    v.visitObject (object);
    return v.range;
}

CodeLocation SourceCodeUtilities::findEndOfExpression (CodeLocation start)
{
    try
    {
        SimpleTokeniser tokeniser (start, true);

        while (! tokeniser.matches (Token::eof))
        {
            if (tokeniser.matchesAny (Operator::comma, Operator::semicolon, Operator::closeParen, Operator::closeBrace))
                return tokeniser.location;

            if (tokeniser.matches (Operator::openParen))
            {
                if (! tokeniser.skipPastMatchingCloseDelimiter (Operator::openParen, Operator::closeParen))
                    break;
            }
            else if (tokeniser.matches (Operator::openBrace))
            {
                if (! tokeniser.skipPastMatchingCloseDelimiter (Operator::openBrace, Operator::closeBrace))
                    break;
            }
            else
            {
                tokeniser.skip();
            }
        }
    }
    catch (const AbortCompilationException&) {}

    return {};
}

CodeLocation SourceCodeUtilities::findEndOfMatchingBrace (CodeLocation start) { return SimpleTokeniser::findEndOfMatchingDelimiter (start, Operator::openBrace, Operator::closeBrace); }
CodeLocation SourceCodeUtilities::findEndOfMatchingParen (CodeLocation start) { return SimpleTokeniser::findEndOfMatchingDelimiter (start, Operator::openParen, Operator::closeParen); }

SourceCodeUtilities::Comment SourceCodeUtilities::getFileSummaryComment (CodeLocation file)
{
    auto firstComment = parseComment (file);

    if (firstComment.isDoxygenStyle && isFollowedByBlankLine (firstComment.range.end))
        return firstComment;

    if (firstComment.valid)
    {
        auto secondComment = parseComment (firstComment.range.end);

        if (secondComment.isDoxygenStyle && isFollowedByBlankLine (secondComment.range.end))
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
            auto lines = summary.lines;
            lines.erase (lines.begin());

            while (! lines.empty() && lines.front().empty())
                lines.erase (lines.begin());

            return joinStrings (lines, "\n");
        }
    }

    return summary.range.toString();
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
    result.range.start = pos;

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

        result.range.end = pos;
        result.range.end.location = closeComment;
        result.range.end.location += 2;
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

        result.range.end = pos;

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

SourceCodeUtilities::Comment SourceCodeUtilities::findPrecedingComment (CodeLocation location)
{
    return parseComment (findStartOfPrecedingComment (location.getStartOfLine()));
}

//==============================================================================
void SourceCodeUtilities::iterateSyntaxTokens (CodeLocation start,
                                               const std::function<bool(std::string_view, std::string_view)>& handleToken)
{
    auto getTokenType = [] (TokenType t) -> std::string_view
    {
       #define SOUL_COMPARE_KEYWORD(name, str) if (t == Keyword::name) return "hljs-keyword";
        SOUL_KEYWORDS (SOUL_COMPARE_KEYWORD)
       #undef SOUL_COMPARE_KEYWORD

       #define SOUL_COMPARE_OPERATOR(name, str) if (t == Operator::name) return "hljs-operator";
        SOUL_OPERATORS (SOUL_COMPARE_OPERATOR)
       #undef SOUL_COMPARE_OPERATOR

        if (t == Token::identifier)      return "hljs-name";
        if (t == Token::literalInt32)    return "hljs-number";
        if (t == Token::literalInt64)    return "hljs-number";
        if (t == Token::literalFloat32)  return "hljs-number";
        if (t == Token::literalFloat64)  return "hljs-number";
        if (t == Token::literalImag32)   return "hljs-number";
        if (t == Token::literalImag64)   return "hljs-number";
        if (t == Token::literalString)   return "hljs-string";
        if (t == Token::comment)         return "hljs-comment";

        return "hljs";
    };

    SimpleTokeniser tokeniser (start, false);
    auto currentSectionStart = start.location;
    std::string_view currentTokenType;

    try
    {
        while (! tokeniser.matches (Token::eof))
        {
            auto newPos = tokeniser.location.location;
            auto newType = getTokenType (tokeniser.currentType);

            if (newType != currentTokenType)
            {
                std::string_view tokenText (currentSectionStart.getAddress(),
                                            static_cast<size_t> (newPos.getAddress() - currentSectionStart.getAddress()));

                if (! tokenText.empty())
                    if (! handleToken (tokenText, currentTokenType))
                        break;

                currentSectionStart = newPos;
                currentTokenType = newType;
            }

            tokeniser.skip();
        }
    }
    catch (const AbortCompilationException&) {}

    if (currentSectionStart.isNotEmpty())
        handleToken (currentSectionStart.getAddress(), currentTokenType);
}


} // namespace soul
