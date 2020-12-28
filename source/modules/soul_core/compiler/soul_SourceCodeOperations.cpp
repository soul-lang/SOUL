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

SourceCodeOperations::SourceCodeOperations() = default;
SourceCodeOperations::~SourceCodeOperations() = default;

void SourceCodeOperations::clear()
{
    topLevelNamespace.reset();
    allModules.clear();
    processors.clear();
    graphs.clear();
    namespaces.clear();
    source = {};
    allocator.clear();
}

bool SourceCodeOperations::reload (CompileMessageList& messageList, CodeLocation code, ApplyModificationFn applyMod)
{
    applyModification = std::move (applyMod);
    return reparse (messageList, std::move (code));
}

void SourceCodeOperations::removeProcessor (AST::ProcessorBase& p)
{
    auto d = findDeclaration (p);
    SOUL_ASSERT (d != nullptr);
    deleteText (d->startIncludingPreamble, d->endOfClosingBrace);
    reparse();
}

void SourceCodeOperations::addProcessor (AST::ProcessorBase&)
{
    // TODO
}

void SourceCodeOperations::recurseFindingModules (soul::AST::ModuleBase& m)
{
    if (m.originalModule != nullptr)
        return;

    // if there's no keyword then it's an outer namespace that was parsed indirectly
    if (! m.processorKeywordLocation.isEmpty())
    {
        allModules.push_back (createDecl (m));

        if (m.isGraph())
            graphs.push_back (createDecl (m));
        else if (m.isProcessor())
            processors.push_back (createDecl (m));
        else if (m.isNamespace())
            namespaces.push_back (createDecl (m));
    }

    for (auto& sub : m.getSubModules())
        recurseFindingModules (sub);
}

bool SourceCodeOperations::reparse (CompileMessageList& messageList, CodeLocation code)
{
    clear();

    try
    {
        CompileMessageHandler handler (messageList);
        topLevelNamespace = AST::createRootNamespace (allocator);

        for (auto& m : Compiler::parseTopLevelDeclarations (allocator, std::move (code), *topLevelNamespace))
            recurseFindingModules (m);
    }
    catch (AbortCompilationException)
    {
        clear();
    }

    return ! messageList.hasErrors();
}

void SourceCodeOperations::reparse()
{
    SOUL_ASSERT (topLevelNamespace != nullptr);
    CompileMessageList errors;
    auto ok = reparse (errors, std::move (source));
    SOUL_ASSERT (ok); ignoreUnused (ok);
}

static CodeLocation findStartOfPrecedingComment (CodeLocation location)
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

CodeLocation SourceCodeOperations::findEndOfMatchingBrace (CodeLocation start) { return SimpleTokeniser::findEndOfMatchingDelimiter (start, Operator::openBrace, Operator::closeBrace); }
CodeLocation SourceCodeOperations::findEndOfMatchingParen (CodeLocation start) { return SimpleTokeniser::findEndOfMatchingDelimiter (start, Operator::openParen, Operator::closeParen); }

SourceCodeOperations::ModuleDeclaration SourceCodeOperations::createDecl (AST::ModuleBase& m)
{
    ModuleDeclaration d { m };

    d.moduleKeyword = m.processorKeywordLocation;
    SOUL_ASSERT (d.moduleKeyword.location.startsWith (d.getType().c_str()));

    d.startIncludingPreamble = findStartOfPrecedingComment (d.moduleKeyword);
    d.openBrace = SimpleTokeniser::findNext (d.moduleKeyword, Operator::openBrace);
    d.endOfClosingBrace = findEndOfMatchingBrace (d.openBrace);
    return d;
}

SourceCodeOperations::ModuleDeclaration* SourceCodeOperations::findDeclaration (AST::ModuleBase& target)
{
    for (auto& m : allModules)
        if (std::addressof (m.module) == std::addressof (target))
            return std::addressof (m);

    SOUL_ASSERT_FALSE;
    return {};
}

size_t SourceCodeOperations::getFileOffset (const CodeLocation& location) const
{
    auto diff = location.location.getAddress() - source.location.getAddress();
    SOUL_ASSERT (diff >= 0);
    return static_cast<size_t> (diff);
}

void SourceCodeOperations::insertText (CodeLocation location, std::string newText)
{
    SOUL_ASSERT (applyModification != nullptr);
    applyModification ({ getFileOffset (location), 0, std::move (newText) });
}

void SourceCodeOperations::replaceText (CodeLocation start, CodeLocation end, std::string newText)
{
    auto s = getFileOffset (start);
    auto e = getFileOffset (end);
    SOUL_ASSERT (e >= s);
    SOUL_ASSERT (applyModification != nullptr);
    applyModification ({ s, e - s, std::move (newText) });
}

void SourceCodeOperations::deleteText (CodeLocation start, CodeLocation end)
{
    replaceText (start, end, {});
}

static std::vector<std::string> extractComment (CodeLocation pos)
{
    pos.location = pos.location.findEndOfWhitespace();

    if (pos.location.advanceIfStartsWith ("/**"))
    {
        auto closeComment = pos.location.find ("*/");

        if (closeComment.isEmpty())
            return {};

        auto firstLine = pos.getSourceLine();
        auto leadingSpaces = firstLine.find ("/**");
        SOUL_ASSERT (leadingSpaces != std::string::npos);
        leadingSpaces += 4;

        if (pos.location.isWhitespace())
            ++(pos.location);

        auto lines = choc::text::splitIntoLines (std::string (pos.location.getAddress(),
                                                              closeComment.getAddress()),
                                                 false);

        for (auto& l : lines)
        {
            l = choc::text::trimEnd (l);
            auto leadingSpacesOnLine = l.length() - choc::text::trimStart (l).length();

            if (leadingSpacesOnLine >= leadingSpaces)
                l = l.substr (leadingSpaces);
        }

        return lines;
    }

    if (pos.location.startsWith ("///"))
    {
        std::vector<std::string> lines;

        for (;;)
        {
            auto line = choc::text::trim (pos.getSourceLine());

            if (! choc::text::startsWith (line, "///"))
                break;

            lines.push_back (line.substr (3));
            pos = pos.getStartOfNextLine();
        }

        return lines;
    }

    return {};
}

std::string SourceCodeOperations::ModuleDeclaration::getType() const
{
    return module.isNamespace() ? "namespace"
                                : (module.isGraph() ? "graph" : "processor");
}

std::string SourceCodeOperations::ModuleDeclaration::getName() const
{
    return module.name.toString();
}

std::string SourceCodeOperations::ModuleDeclaration::getFullyQualifiedName() const
{
    return Program::stripRootNamespaceFromQualifiedPath (module.getFullyQualifiedDisplayPath().toString());
}

std::string SourceCodeOperations::ModuleDeclaration::getComment() const
{
    auto lines = extractComment (startIncludingPreamble);

    return joinStrings (lines, "\n");
}


} // namespace soul
