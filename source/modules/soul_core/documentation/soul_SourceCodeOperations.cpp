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

//==============================================================================
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
    source = code;

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

SourceCodeOperations::ModuleDeclaration SourceCodeOperations::createDecl (AST::ModuleBase& m)
{
    ModuleDeclaration d { m, allocator };

    d.moduleKeyword = m.processorKeywordLocation;
    SOUL_ASSERT (d.moduleKeyword.location.startsWith (d.getType().c_str()));

    d.startIncludingPreamble = SourceCodeUtilities::findStartOfPrecedingComment (d.moduleKeyword);
    d.openBrace = SimpleTokeniser::findNext (d.moduleKeyword, Operator::openBrace);
    d.endOfClosingBrace = SourceCodeUtilities::findEndOfMatchingBrace (d.openBrace);
    d.fileComment = SourceCodeUtilities::parseComment (d.startIncludingPreamble);
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

void SourceCodeOperations::insertText (CodeLocation location, std::string newText)
{
    SOUL_ASSERT (applyModification != nullptr);
    applyModification ({ location.getByteOffsetInFile(), 0, std::move (newText) });
}

void SourceCodeOperations::replaceText (CodeLocation start, CodeLocation end, std::string newText)
{
    auto s = start.getByteOffsetInFile();
    auto e = end.getByteOffsetInFile();
    SOUL_ASSERT (e >= s);
    SOUL_ASSERT (applyModification != nullptr);
    applyModification ({ s, e - s, std::move (newText) });
}

void SourceCodeOperations::deleteText (CodeLocation start, CodeLocation end)
{
    replaceText (start, end, {});
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

} // namespace soul
