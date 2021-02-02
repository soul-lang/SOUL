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

Compiler::Compiler (bool i) : includeStandardLibrary (i)
{
    reset();
}

void Compiler::reset()
{
    topLevelNamespace.reset();
    allocator.clear();
}

bool Compiler::addCode (CompileMessageList& messageList, CodeLocation code)
{
    if (messageList.hasErrors())
        return false;

    if (topLevelNamespace == nullptr)
    {
        topLevelNamespace = AST::createRootNamespace (allocator);

        if (includeStandardLibrary)
            addDefaultBuiltInLibrary();
    }

    try
    {
        if (code.isEmpty())
            code.throwError (Errors::emptyProgram());

        SOUL_LOG_TIME_OF_SCOPE ("initial resolution pass: " + code.getFilename());
        soul::CompileMessageHandler handler (messageList);
        compile (std::move (code));
        return true;
    }
    catch (soul::AbortCompilationException) {}

    return false;
}

void Compiler::addDefaultBuiltInLibrary()
{
    CompileMessageList list;

    try
    {
        soul::CompileMessageHandler handler (list);
        compile (getDefaultLibraryCode());

        // TODO: when we have import & module support, these will no longer be hard-coded here
        compile (getSystemModule ("soul.audio.utils"));
        compile (getSystemModule ("soul.midi"));
        compile (getSystemModule ("soul.notes"));
        compile (getSystemModule ("soul.frequency"));
        compile (getSystemModule ("soul.mixing"));
        compile (getSystemModule ("soul.oscillators"));
        compile (getSystemModule ("soul.noise"));
        compile (getSystemModule ("soul.timeline"));
        compile (getSystemModule ("soul.filters"));
    }
    catch (soul::AbortCompilationException)
    {
        soul::throwInternalCompilerError ("Error in built-in code: " + list.toString());
    }
}

static void sanityCheckBuildSettings (const BuildSettings& settings,
                                      uint32_t minBlockSize = 1, uint32_t maxBlockSize = 65536)
{
    if (settings.maxBlockSize != 0 && (settings.maxBlockSize < minBlockSize || settings.maxBlockSize > maxBlockSize))
        CodeLocation().throwError (Errors::unsupportedBlockSize());

    constexpr double maxSampleRate = 48000.0 * 100;

    if (settings.sampleRate <= 0 || settings.sampleRate > maxSampleRate)
        CodeLocation().throwError (Errors::unsupportedSampleRate());

    if (settings.optimisationLevel < -1 || settings.optimisationLevel > 3)
        CodeLocation().throwError (Errors::unsupportedOptimisationLevel());
}

static ArrayWithPreallocation<CodeLocation, 4> getHEARTFiles (const BuildBundle& bundle)
{
    ArrayWithPreallocation<CodeLocation, 4> result;

    for (auto& file : bundle.sourceFiles)
    {
        auto code = CodeLocation::createFromSourceFile (file);

        if (code.location.findEndOfWhitespace().startsWith ("#SOUL"))
            result.push_back (code);
    }

    return result;
}

static Program buildHEART (CompileMessageList& messageList, CodeLocation code)
{
    try
    {
        CompileMessageHandler handler (messageList);
        auto program = heart::Parser::parse (code);
        heart::Checker::sanityCheck (program);
        return program;
    }
    catch (AbortCompilationException) {}

    return {};
}


//==============================================================================
Program Compiler::build (CompileMessageList& messageList, const BuildBundle& bundle)
{
    sanityCheckBuildSettings (bundle.settings);

    auto heartFiles = getHEARTFiles (bundle);

    if (! heartFiles.empty())
    {
        if (heartFiles.size() > 1 || heartFiles.size() < bundle.sourceFiles.size())
            CodeLocation().throwError (Errors::onlyOneHeartFileAllowed());

        return buildHEART (messageList, heartFiles.front());
    }

    Compiler c (bundle.settings.overrideStandardLibrary.empty());

    if (! bundle.settings.overrideStandardLibrary.empty())
        for (auto& file : bundle.settings.overrideStandardLibrary)
            if (! c.addCode (messageList, CodeLocation::createFromSourceFile (file)))
                return {};

    for (auto& file : bundle.sourceFiles)
        if (! c.addCode (messageList, CodeLocation::createFromSourceFile (file)))
            return {};

    return c.link (messageList, bundle.settings);
}

std::vector<pool_ref<AST::ModuleBase>> Compiler::parseTopLevelDeclarations (AST::Allocator& allocator, CodeLocation code,
                                                                            AST::Namespace& parentNamespace)
{
    return StructuralParser::parseTopLevelDeclarations (allocator, code, parentNamespace);
}

//==============================================================================
void Compiler::compile (CodeLocation code)
{
    SOUL_LOG_TIME_OF_SCOPE ("compile: " + code.getFilename());

    for (auto& m : StructuralParser::parseTopLevelDeclarations (allocator, code, *topLevelNamespace))
        SanityCheckPass::runPreResolution (m);

    ResolutionPass::run (allocator, *topLevelNamespace, true);

    ASTUtilities::mergeDuplicateNamespaces (*topLevelNamespace);
    SanityCheckPass::runDuplicateNameChecker (*topLevelNamespace);
}

//==============================================================================
AST::ProcessorBase& Compiler::findMainProcessor (const BuildSettings& settings)
{
    if (! settings.mainProcessor.empty())
    {
        auto path = IdentifierPath::fromString (allocator.identifiers, settings.mainProcessor);

        if (path.isValid())
            for (auto& m : topLevelNamespace->getMatchingSubModules (path))
                if (auto pb = cast<AST::ProcessorBase> (m))
                    return *pb;

        CodeLocation().throwError (Errors::cannotFindMainProcessorWithName (settings.mainProcessor));
    }

    std::vector<pool_ref<AST::ProcessorBase>> mainProcessors;
    ASTUtilities::findAllMainProcessors (*topLevelNamespace, mainProcessors);

    if (mainProcessors.size() > 1)
    {
        CompileMessageGroup group;

        for (auto& p : mainProcessors)
            group.messages.push_back (Errors::multipleProcessorsMarkedAsMain().withLocation (p->context.location));

        throwError (group);
    }

    if (mainProcessors.size() == 1)
        return mainProcessors.front();

    auto main = ASTUtilities::scanForProcessorToUseAsMain (*topLevelNamespace);

    if (main == nullptr)
        topLevelNamespace->context.throwError (Errors::cannotFindMainProcessor());

    return *main;
}

Program Compiler::link (CompileMessageList& messageList, const BuildSettings& settings)
{
    if (messageList.hasErrors())
        return {};

    try
    {
        CompileMessageHandler handler (messageList);
        sanityCheckBuildSettings (settings);
        return link (messageList, settings, findMainProcessor (settings));
    }
    catch (AbortCompilationException) {}

    return {};
}

Program Compiler::link (CompileMessageList& messageList, const BuildSettings& settings, AST::ProcessorBase& processorToRun)
{
    try
    {
        SOUL_LOG_TIME_OF_SCOPE ("link time");
        CompileMessageHandler handler (messageList);
        ASTUtilities::resolveHoistedEndpoints (allocator, *topLevelNamespace);
        ASTUtilities::mergeDuplicateNamespaces (*topLevelNamespace);
        ASTUtilities::removeModulesWithSpecialisationParams (*topLevelNamespace);
        ResolutionPass::run (allocator, *topLevelNamespace, false);

        compile (getSystemModule ("soul.complex"));
        ConvertComplexPass::run (allocator, *topLevelNamespace);

        ASTUtilities::connectAnyChildEndpointsNeedingToBeExposed (allocator, processorToRun);

        Program program;
        program.getStringDictionary() = allocator.stringDictionary;  // Bring the existing string dictionary along so that the handles match
        compileAllModules (*topLevelNamespace, program, processorToRun);
        heart::Utilities::inlineFunctionsThatUseAdvanceOrStreams<Optimisations> (program);
        heart::Checker::sanityCheck (program, settings);

        if (settings.optimisationLevel != 0)
        {
            Optimisations::removeUnusedObjects (program);
        }

        reset();

        SOUL_LOG (program.getMainProcessor().originalFullName + ": linked HEART",
                  [&] { return program.toHEART(); });

        heart::Checker::testHEARTRoundTrip (program);
        Optimisations::optimiseFunctionBlocks (program);
        Optimisations::removeUnusedVariables (program);

        return program;
    }
    catch (AbortCompilationException) {}

    return {};
}

static Module& createHEARTModule (Program& p, pool_ptr<AST::ModuleBase> module, bool isMainProcessor)
{
    int index = isMainProcessor ? 0 : -1;

    if (module->isNamespace())   return p.addNamespace (index);
    if (module->isGraph())       return p.addGraph (index);

    SOUL_ASSERT (module->isProcessor());
    return p.addProcessor (index);
}

void Compiler::compileAllModules (const AST::Namespace& parentNamespace, Program& program,
                                  AST::ProcessorBase& processorToRun)
{
    std::vector<pool_ref<AST::ModuleBase>> soulModules;
    ASTUtilities::findAllModulesToCompile (parentNamespace, soulModules);

    std::vector<pool_ref<Module>> heartModules;
    heartModules.reserve (soulModules.size());

    for (auto& m : soulModules)
        heartModules.push_back (createHEARTModule (program, m, m == processorToRun));

    HEARTGenerator::build (soulModules, heartModules);
}

} // namespace soul
