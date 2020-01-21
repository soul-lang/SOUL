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

Compiler::Compiler()
{
    reset();
}

void Compiler::reset()
{
    topLevelNamespace.reset();
    allocator.clear();
    auto rootNamespaceName = allocator.get (Program::getRootNamespaceName());
    topLevelNamespace = allocator.allocate<AST::Namespace> (AST::Context(), rootNamespaceName);
    addDefaultBuiltInLibrary();
}

bool Compiler::addCode (CompileMessageList& messageList, CodeLocation code)
{
    if (messageList.hasErrors())
        return false;

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
        compile (getSystemModule ("soul.noise"));
    }
    catch (soul::AbortCompilationException)
    {
        soul::throwInternalCompilerError ("Error in built-in code: " + list.toString());
    }
}


//==============================================================================
Program Compiler::build (CompileMessageList& messageList, CodeLocation code, const LinkOptions& linkOptions)
{
    Compiler c;

    if (! messageList.hasErrors())
        if (c.addCode (messageList, std::move (code)))
            return c.link (messageList, linkOptions);

    return {};
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
AST::ProcessorBase& Compiler::findMainProcessor (const LinkOptions& linkOptions)
{
    auto nameOfProcessorToRun = linkOptions.getMainProcessor();

    if (! nameOfProcessorToRun.empty())
    {
        auto path = IdentifierPath::fromString (allocator.identifiers, nameOfProcessorToRun);

        if (path.isValid())
            for (auto& m : topLevelNamespace->getMatchingSubModules (path))
                if (auto pb = cast<AST::ProcessorBase> (m))
                    return *pb;

        CodeLocation().throwError (Errors::cannotFindMainProcessorWithName (nameOfProcessorToRun));
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

Program Compiler::link (CompileMessageList& messageList, const LinkOptions& linkOptions)
{
    if (messageList.hasErrors())
        return {};

    try
    {
        CompileMessageHandler handler (messageList);
        return link (messageList, linkOptions, findMainProcessor (linkOptions));
    }
    catch (AbortCompilationException) {}

    return {};
}

Program Compiler::link (CompileMessageList& messageList, const LinkOptions& linkOptions, AST::ProcessorBase& processorToRun)
{
    try
    {
        SOUL_LOG_TIME_OF_SCOPE ("link time");
        ignoreUnused (linkOptions);
        CompileMessageHandler handler (messageList);
        resolveProcessorInstances (processorToRun);
        ASTUtilities::resolveHoistedEndpoints (allocator, *topLevelNamespace);
        ASTUtilities::mergeDuplicateNamespaces (*topLevelNamespace);
        ASTUtilities::removeModulesWithSpecialisationParams (*topLevelNamespace);
        ResolutionPass::run (allocator, *topLevelNamespace, true);
        ResolutionPass::run (allocator, *topLevelNamespace, false);
        createImplicitProcessorInstances (*topLevelNamespace);
        ASTUtilities::connectAnyChildEndpointsNeedingToBeExposed (allocator, processorToRun);

        Program program;
        program.getStringDictionary() = allocator.stringDictionary;  // Bring the existing string dictionary along so that the handles match
        compileAllModules (*topLevelNamespace, program, processorToRun);
        heart::Utilities::inlineFunctionsThatUseAdvanceOrStreams<Optimisations> (program);
        heart::Checker::sanityCheck (program);
        reset();

        SOUL_LOG (program.getMainProcessorOrThrowError().getNameWithoutRootNamespace() + ": linked HEART",
                  [&] { return program.toHEART(); });

        heart::Checker::testHEARTRoundTrip (program);
        optimise (program);
        return program;
    }
    catch (AbortCompilationException) {}

    return {};
}

void Compiler::optimise (Program& program)
{
    Optimisations::optimiseFunctionBlocks (program);
    Optimisations::removeUnusedVariables (program);
}

void Compiler::resolveProcessorInstances (AST::ProcessorBase& processor)
{
    createImplicitProcessorInstances (*topLevelNamespace);

    std::vector<pool_ref<AST::ProcessorBase>> usedProcessorInstances;
    recursivelyResolveProcessorInstances (processor, usedProcessorInstances);
    ASTUtilities::removeUnusedGraphs (*topLevelNamespace, usedProcessorInstances);
}

void Compiler::recursivelyResolveProcessorInstances (pool_ref<AST::ProcessorBase> processor,
                                                     std::vector<pool_ref<AST::ProcessorBase>>& usedProcessorInstances)
{
    usedProcessorInstances.push_back (processor);

    if (auto graph = cast<AST::Graph> (processor))
    {
        AST::Graph::RecursiveGraphDetector::check (*graph);

        {
            DuplicateNameChecker duplicateNameChecker;

            for (auto& i : graph->processorInstances)
                duplicateNameChecker.check (i->instanceName->path.toString(), i->instanceName->context);
        }

        std::vector<pool_ref<AST::ProcessorBase>> resolvedTargets;

        for (auto& i : graph->processorInstances)
        {
            if (! i->wasCreatedImplicitly)
                if (! graph->getMatchingSubModules (i->instanceName->path).empty())
                    i->context.throwError (Errors::alreadyProcessorWithName (i->instanceName->path));

            auto& target = graph->findSingleMatchingProcessor (i);
            resolvedTargets.push_back (target);
            bool alreadyUsed = contains (usedProcessorInstances, target);
            auto resolvedProcessor = createSpecialisedInstance (*graph, i, target, alreadyUsed);
            recursivelyResolveProcessorInstances (resolvedProcessor, usedProcessorInstances);
        }

        SOUL_ASSERT (resolvedTargets.size() == graph->processorInstances.size());

        for (size_t i = 0; i < resolvedTargets.size(); ++i)
        {
            for (size_t j = i + 1; j < resolvedTargets.size(); ++j)
            {
                if (resolvedTargets[i] == resolvedTargets[j])
                {
                    auto pi1 = graph->processorInstances[i];
                    auto pi2 = graph->processorInstances[j];

                    if (pi1->wasCreatedImplicitly != pi2->wasCreatedImplicitly)
                        pi1->context.throwError (Errors::cannotUseProcessorInLet (resolvedTargets[i]->name));
                }
            }
        }
    }
}

void Compiler::createImplicitProcessorInstanceIfNeeded (AST::Graph& graph, AST::QualifiedIdentifier& path)
{
    if (path.path.empty())
        return;

    for (auto i : graph.processorInstances)
        if (path == *i->instanceName)
            return;

    auto& target = graph.findSingleMatchingProcessor (path);
    auto& i = allocator.allocate<AST::ProcessorInstance> (path.context);
    graph.processorInstances.push_back (i);
    i.instanceName  = allocator.allocate<AST::QualifiedIdentifier> (path.context, path.path);
    i.targetProcessor = allocator.allocate<AST::ProcessorRef> (path.context, target);
    i.wasCreatedImplicitly = true;
}

void Compiler::createImplicitProcessorInstances (AST::ModuleBase& module)
{
    for (auto& m : module.getSubModules())
        createImplicitProcessorInstances (m);

    if (auto graph = cast<AST::Graph> (module))
    {
        for (auto conn : graph->connections)
        {
            createImplicitProcessorInstanceIfNeeded (*graph, *conn->source.processorName);
            createImplicitProcessorInstanceIfNeeded (*graph, *conn->dest.processorName);
        }
    }
}

static std::string createSpecialisationNameSuffix (AST::Graph& graph, const AST::ProcessorInstance& processorInstance)
{
    SOUL_ASSERT (! processorInstance.specialisationArgs.empty());
    return "_for_" + makeSafeIdentifierName (replaceSubString (graph.getFullyQualifiedPath().toString(), ":", "_")
                                               + "_" + processorInstance.instanceName->toString());
}

pool_ref<AST::ProcessorBase> Compiler::createSpecialisedInstance (AST::Graph& graph,
                                                                  AST::ProcessorInstance& processorInstance,
                                                                  AST::ProcessorBase& target, bool mustCreateClone)
{
    auto numParams = processorInstance.specialisationArgs.size();

    if (target.getSpecialisationParameters().size() != numParams)
        processorInstance.context.throwError (Errors::wrongNumArgsForProcessor (target.getFullyQualifiedPath()));

    pool_ref<AST::ProcessorBase> specialised (target);

    if (numParams != 0)
        specialised = addClone (target, TokenisedPathString::join (target.name.toString(),
                                                                   createSpecialisationNameSuffix (graph, processorInstance)));
    else if (mustCreateClone)
        specialised = addClone (target, target.name.toString());

    auto params = specialised->getSpecialisationParameters();

    // Apply any type arguments first - we'll go back and do the values afterwards
    // so that they can use these types if needed..
    for (size_t i = 0; i < numParams; ++i)
    {
        auto& arg = processorInstance.specialisationArgs[i];
        auto& param = params[i];

        if (auto u = cast<AST::UsingDeclaration> (param))
        {
            if (! AST::isResolvedAsType (arg))
                arg->context.throwError (Errors::expectedType());

            u->targetType = arg;
        }

        if (auto pa = cast<AST::ProcessorAliasDeclaration> (param))
        {
            if (auto p = cast<AST::ProcessorRef> (arg))
                pa->targetProcessor = p->processor.get();
            else
                arg->context.throwError (Errors::expectedProcessorName());
        }
    }

    for (size_t i = 0; i < numParams; ++i)
    {
        auto& arg = *processorInstance.specialisationArgs[i];
        auto& param = params[i];

        if (auto v = cast<AST::VariableDeclaration> (param))
        {
            if (! v->isResolved())
            {
                ResolutionPass::run (allocator, specialised, true);

                if (! v->isResolved())
                    ResolutionPass::run (allocator, specialised, false);
            }

            if (arg.isResolved() && ! AST::isResolvedAsValue (arg))
                arg.context.throwError (Errors::expectedConstant());

            SOUL_ASSERT (v->isConstant && v->declaredType != nullptr);

            auto targetType = v->getType();

            if (auto value = arg.getAsConstant())
            {
                if (TypeRules::canSilentlyCastTo (targetType, value->value))
                {
                    auto constValue = value->value.castToTypeWithError (targetType, value->context);
                    v->initialValue = allocator.allocate<AST::Constant> (arg.context, constValue);
                    continue;
                }

                if (targetType.isUnsizedArray() && TypeRules::canBeConvertedAllowingFixedToUnsizedArrays (targetType, value->value.getType()))
                {
                    v->initialValue = allocator.allocate<AST::Constant> (arg.context, value->value);
                    continue;
                }
            }

            if (auto variableRef = cast<AST::VariableRef> (arg))
            {
                if (variableRef->variable->isExternal && TypeRules::canSilentlyCastTo (targetType, variableRef->variable->getType()))
                {
                    v->initialValue = allocator.allocate<AST::QualifiedIdentifier> (variableRef->variable->context,
                                                                                    IdentifierPath (variableRef->variable->context.parentScope->getFullyQualifiedPath(),
                                                                                                    allocator.get (variableRef->variable->name)).fromSecondPart());
                    continue;
                }
            }

            arg.context.throwError (Errors::expectedExpressionOfType (targetType.getDescription()));
        }
    }

    specialised->specialisationParams.clear();
    processorInstance.targetProcessor = allocator.allocate<AST::ProcessorRef> (processorInstance.context, specialised);
    processorInstance.specialisationArgs.clear();

    // Since this clone isn't resolved, do this now
    ResolutionPass::run (allocator, specialised, true);

    return specialised;
}

pool_ref<AST::ProcessorBase> Compiler::addClone (const AST::ProcessorBase& m, const std::string& nameRoot)
{
    auto& ns = m.getNamespace();
    return *StructuralParser::cloneProcessorWithNewName (allocator, ns, m, ns.makeUniqueName (nameRoot));
}

static pool_ptr<Module> createHEARTModule (Program& p, pool_ptr<AST::ModuleBase> module, bool isMainProcessor)
{
    int index = isMainProcessor ? 0 : -1;

    if (module->isNamespace())   return p.addNamespace (index);
    if (module->isGraph())       return p.addGraph (index);
    if (module->isProcessor())   return p.addProcessor (index);

    SOUL_ASSERT_FALSE;
    return {};
}

void Compiler::compileAllModules (const AST::Namespace& parentNamespace, Program& program,
                                  AST::ProcessorBase& processorToRun)
{
    std::vector<pool_ref<AST::ModuleBase>> modulesToCompile;
    ASTUtilities::findAllModulesToCompile (parentNamespace, modulesToCompile);

    HEARTGenerator::UnresolvedFunctionCallList unresolvedCalls;

    for (auto& m : modulesToCompile)
    {
        auto newModule = createHEARTModule (program, m, m == processorToRun);
        HEARTGenerator::run (m, *newModule, unresolvedCalls);
    }

    for (auto& c : unresolvedCalls)
        c.resolve();
}

} // namespace soul
