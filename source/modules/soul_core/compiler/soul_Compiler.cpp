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

std::vector<AST::ModuleBasePtr> Compiler::parseTopLevelDeclarations (AST::Allocator& allocator,
                                                                     CodeLocation code,
                                                                     AST::Namespace& parentNamespace)
{
    return StructuralParser::parseTopLevelDeclarations (allocator, code, parentNamespace);
}

static void mergeNamespaces (AST::Namespace& target, AST::Namespace& source)
{
    auto newParentScope = std::addressof (target);

    for (auto& f : source.functions)
    {
        target.functions.push_back (f);
        f->context.parentScope = newParentScope;
    }

    for (auto& s : source.structures)
    {
        target.structures.push_back (s);
        s->context.parentScope = newParentScope;
    }

    for (auto& u : source.usings)
    {
        target.usings.push_back (u);
        u->context.parentScope = newParentScope;
    }

    for (auto& m : source.subModules)
    {
        target.subModules.push_back (m);
        m->context.parentScope = newParentScope;
    }

    for (auto& c : source.constants)
    {
        target.constants.push_back (c);
        c->context.parentScope = newParentScope;
    }
}

static bool mergeFirstPairOfDuplicateNamespaces (AST::Namespace& ns)
{
    bool anyDone = false;

    for (size_t i = 0; i < ns.subModules.size(); ++i)
    {
        if (auto ns1 = cast<AST::Namespace> (ns.subModules[i]))
        {
            anyDone = mergeFirstPairOfDuplicateNamespaces (*ns1) || anyDone;

            for (size_t j = i + 1; j < ns.subModules.size(); ++j)
            {
                if (auto ns2 = cast<AST::Namespace> (ns.subModules[j]))
                {
                    if (ns1->name == ns2->name
                         && ns1->getSpecialisationParameters().empty()
                         && ns2->getSpecialisationParameters().empty())
                    {
                        mergeNamespaces (*ns1, *ns2);
                        ns.subModules.erase (ns.subModules.begin() + (ssize_t) j);
                        return true;
                    }
                }
            }
        }
    }

    return anyDone;
}

static void mergeDuplicateNamespaces (AST::Namespace& ns)
{
    while (mergeFirstPairOfDuplicateNamespaces (ns))
    {}
}

//==============================================================================
void Compiler::compile (CodeLocation code)
{
    SOUL_LOG_TIME_OF_SCOPE ("compile: " + code.getFilename());

    for (auto& m : StructuralParser::parseTopLevelDeclarations (allocator, code, *topLevelNamespace))
        SanityCheckPass::runPreResolution (*m);

    ResolutionPass::run (allocator, *topLevelNamespace, true);

    mergeDuplicateNamespaces (*topLevelNamespace);
    SanityCheckPass::runDuplicateNameChecker (*topLevelNamespace);
}

//==============================================================================
static void findAllMainProcessors (AST::ModuleBase& module, std::vector<pool_ptr<AST::ProcessorBase>>& found)
{
    for (auto& m : module.getSubModules())
    {
        if (auto pb = cast<AST::ProcessorBase> (m))
            if (auto main = pb->annotation.findProperty ("main"))
                if (auto c = main->value->getAsConstant())
                    if (c->value.getAsBool())
                        found.push_back (pb);

        findAllMainProcessors (*m, found);
    }
}

static pool_ptr<AST::ProcessorBase> scanForProcessorToUseAsMain (AST::ModuleBase& module)
{
    pool_ptr<AST::ProcessorBase> lastProcessor;

    for (auto& m : module.getSubModules())
    {
        auto p1 = cast<AST::ProcessorBase> (m);

        if (p1 != nullptr && ! p1->annotation.findProperty ("main"))
            lastProcessor = p1;
        else if (auto p2 = scanForProcessorToUseAsMain (*m))
            lastProcessor = p2;
    }

    return lastProcessor;
}

pool_ptr<AST::ProcessorBase> Compiler::findMainProcessor (const LinkOptions& linkOptions)
{
    auto nameOfProcessorToRun = linkOptions.getMainProcessor();

    if (! nameOfProcessorToRun.empty())
    {
        auto path = IdentifierPath::fromString (allocator.identifiers, nameOfProcessorToRun);

        if (path.isValid())
            for (auto& m : topLevelNamespace->getMatchingSubModules (path))
                if (auto pb = cast<AST::ProcessorBase> (m))
                    return pb;

        CodeLocation().throwError (Errors::cannotFindMainProcessorWithName (nameOfProcessorToRun));
    }

    std::vector<pool_ptr<AST::ProcessorBase>> mainProcessors;
    findAllMainProcessors (*topLevelNamespace, mainProcessors);

    if (mainProcessors.size() > 1)
    {
        CompileMessageGroup group;

        for (auto& p : mainProcessors)
            group.messages.push_back (Errors::multipleProcessorsMarkedAsMain().withLocation (p->context.location));

        throwError (group);
    }

    if (mainProcessors.size() == 1)
        return mainProcessors.front();

    auto main = scanForProcessorToUseAsMain (*topLevelNamespace);

    if (main == nullptr)
        topLevelNamespace->context.throwError (Errors::cannotFindMainProcessor());

    return main;
}

Program Compiler::link (CompileMessageList& messageList, const LinkOptions& linkOptions)
{
    if (messageList.hasErrors())
        return {};

    try
    {
        CompileMessageHandler handler (messageList);
        auto main = findMainProcessor (linkOptions);
        return link (messageList, linkOptions, main);
    }
    catch (AbortCompilationException) {}

    return {};
}

static void testHEARTRoundTrip (const Program& program)
{
    ignoreUnused (program);

   #if SOUL_ENABLE_ASSERTIONS && (SOUL_TEST_HEART_ROUNDTRIP || (SOUL_DEBUG && ! defined (SOUL_TEST_HEART_ROUNDTRIP)))
    auto dump = program.toHEART();
    SOUL_ASSERT (dump == program.clone().toHEART());
    SOUL_ASSERT (dump == heart::Parser::parse (CodeLocation::createFromString ("internal test dump", dump)).toHEART());
   #endif
}

Program Compiler::link (CompileMessageList& messageList, const LinkOptions& linkOptions,
                        pool_ptr<AST::ProcessorBase> processorToRun)
{
    try
    {
        SOUL_LOG_TIME_OF_SCOPE ("link time");
        ignoreUnused (linkOptions);
        CompileMessageHandler handler (messageList);
        resolveProcessorInstances (processorToRun);
        mergeDuplicateNamespaces (*topLevelNamespace);
        removeModulesWithSpecialisationParams (topLevelNamespace);
        ResolutionPass::run (allocator, *topLevelNamespace, true);
        ResolutionPass::run (allocator, *topLevelNamespace, false);
        createImplicitProcessorInstances (topLevelNamespace);

        Program program;
        program.getStringDictionary() = allocator.stringDictionary;  // Bring the existing string dictionary along so that the handles match
        compileAllModules (*topLevelNamespace, program, processorToRun);
        sanityCheck (program);
        reset();

        SOUL_LOG (program.getMainProcessorOrThrowError().getNameWithoutRootNamespace() + ": linked HEART",
                  [&] { return program.toHEART(); });

        testHEARTRoundTrip (program);
        optimise (program);
        return program;
    }
    catch (AbortCompilationException) {}

    return {};
}

static void removeUnusedGraphs (AST::Namespace& ns, ArrayView<pool_ptr<AST::ProcessorBase>> graphsToKeep)
{
    for (auto& m : ns.subModules)
        if (auto subNamespace = cast<AST::Namespace> (m))
            removeUnusedGraphs (*subNamespace, graphsToKeep);

    removeIf (ns.subModules, [=] (AST::ModuleBasePtr m)
    {
        if (auto graph = cast<AST::Graph> (m))
            return ! contains (graphsToKeep, graph);

        return false;
    });
}

void Compiler::resolveProcessorInstances (pool_ptr<AST::ProcessorBase> processor)
{
    createImplicitProcessorInstances (topLevelNamespace);

    std::vector<pool_ptr<AST::ProcessorBase>> usedProcessorInstances;
    recursivelyResolveProcessorInstances (processor, usedProcessorInstances);
    removeUnusedGraphs (*topLevelNamespace, usedProcessorInstances);
}

void Compiler::recursivelyResolveProcessorInstances (pool_ptr<AST::ProcessorBase> processor,
                                                     std::vector<pool_ptr<AST::ProcessorBase>>& usedProcessorInstances)
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

        std::vector<pool_ptr<AST::ProcessorBase>> resolvedTargets;

        for (auto& i : graph->processorInstances)
        {
            if (! i->wasCreatedImplicitly)
                if (! graph->getMatchingSubModules (i->instanceName->path).empty())
                    i->context.throwError (Errors::alreadyProcessorWithName (i->instanceName->path));

            auto target = graph->findSingleMatchingProcessor (*i);
            resolvedTargets.push_back (target);
            bool alreadyUsed = contains (usedProcessorInstances, target);
            auto resolvedProcessor = createSpecialisedInstance (*graph, *i, *target, alreadyUsed);
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

    if (auto target = graph.findSingleMatchingProcessor (path))
    {
        auto i = allocator.allocate<AST::ProcessorInstance> (path.context);
        graph.processorInstances.push_back (i);
        i->instanceName  = allocator.allocate<AST::QualifiedIdentifier> (path.context, path.path);
        i->targetProcessor = allocator.allocate<AST::ProcessorRef> (path.context, *target);
        i->wasCreatedImplicitly = true;
    }
}

void Compiler::createImplicitProcessorInstances (AST::ModuleBasePtr module)
{
    for (auto& m : module->getSubModules())
        createImplicitProcessorInstances (*m);

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

pool_ptr<AST::ProcessorBase> Compiler::createSpecialisedInstance (AST::Graph& graph,
                                                                  AST::ProcessorInstance& processorInstance,
                                                                  AST::ProcessorBase& target, bool mustCreateClone)
{
    auto numParams = processorInstance.specialisationArgs.size();

    if (target.getSpecialisationParameters().size() != numParams)
        processorInstance.context.throwError (Errors::wrongNumArgsForProcessor (target.getFullyQualifiedPath()));

    pool_ptr<AST::ProcessorBase> specialised (target);

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
                pa->targetProcessor = p->processor;
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
                ResolutionPass::run (allocator, *specialised, true);

                if (! v->isResolved())
                    ResolutionPass::run (allocator, *specialised, false);
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
    processorInstance.targetProcessor = allocator.allocate<AST::ProcessorRef> (processorInstance.context, *specialised);
    processorInstance.specialisationArgs.clear();

    // Since this clone isn't resolved, do this now
    ResolutionPass::run (allocator, *specialised, true);

    return specialised;
}

pool_ptr<AST::ProcessorBase> Compiler::addClone (const AST::ProcessorBase& m, const std::string& nameRoot)
{
    auto& ns = m.getNamespace();
    return StructuralParser::cloneProcessorWithNewName (allocator, ns, m, ns.makeUniqueName (nameRoot));
}

void Compiler::removeModulesWithSpecialisationParams (pool_ptr<AST::Namespace> ns)
{
    for (auto& m : ns->getSubModules())
        if (auto sub = cast<AST::Namespace> (m))
            removeModulesWithSpecialisationParams (sub);

    removeIf (ns->subModules,
              [] (AST::ModuleBasePtr& m) { return ! m->getSpecialisationParameters().empty(); });
}

static pool_ptr<Module> createModuleFor (Program& p, pool_ptr<AST::ModuleBase> module, bool isMainProcessor)
{
    int index = isMainProcessor ? 0 : -1;

    if (module->isNamespace())   return p.addNamespace (index);
    if (module->isGraph())       return p.addGraph (index);
    if (module->isProcessor())   return p.addProcessor (index);

    SOUL_ASSERT_FALSE;
    return {};
}

static void findAllModulesToCompile (const AST::Namespace& parentNamespace, std::vector<pool_ptr<AST::ModuleBase>>& modulesToCompile)
{
    for (auto& m : parentNamespace.subModules)
    {
        SOUL_ASSERT (m->getSpecialisationParameters().empty());
        modulesToCompile.push_back (m);

        if (auto ns = cast<AST::Namespace> (m))
            findAllModulesToCompile (*ns, modulesToCompile);
    }
}

void Compiler::compileAllModules (const AST::Namespace& parentNamespace, Program& program,
                                  pool_ptr<AST::ProcessorBase> processorToRun)
{
    std::vector<pool_ptr<AST::ModuleBase>> modulesToCompile;
    findAllModulesToCompile (parentNamespace, modulesToCompile);

    HEARTGenerator::UnresolvedFunctionCallList unresolvedCalls;

    for (auto& m : modulesToCompile)
    {
        auto newModule = createModuleFor (program, *m, processorToRun == m);
        HEARTGenerator::run (*m, *newModule, unresolvedCalls);
    }

    for (auto& c : unresolvedCalls)
        c.resolve();
}

void Compiler::sanityCheck (Program& program)
{
    program.getMainProcessorOrThrowError();
    sanityCheckInputsAndOutputs (program);
    sanityCheckRunFunctions (program);
    checkForRecursiveFunctions (program);
    checkForInfiniteLoops (program);
}

void Compiler::optimise (Program& program)
{
    Optimisations::optimiseFunctionBlocks (program);
    Optimisations::removeUnusedVariables (program);
}

void Compiler::sanityCheckInputsAndOutputs (Program& program)
{
    auto& mainProcessor = program.getMainProcessorOrThrowError();

    for (auto& input : mainProcessor.inputs)
    {
        if (input->arraySize != 1)
            input->location.throwError (Errors::notYetImplemented ("top-level arrays of inputs"));

        if (input->sampleTypes.size() != 1)
            input->location.throwError (Errors::onlyOneTypeInTopLevelInputs());
    }

    for (auto& output : mainProcessor.outputs)
    {
        if (output->arraySize != 1)
            output->location.throwError (Errors::notYetImplemented ("top-level arrays of outputs"));

        if (output->sampleTypes.size() != 1)
            output->location.throwError (Errors::onlyOneTypeInTopLevelOutputs());
    }
}

void Compiler::sanityCheckRunFunctions (Program& program)
{
    for (auto& m : program.getModules())
        if (m->isProcessor())
            if (auto runFn = m->getRunFunction())
                if (! CallFlowGraph::doesFunctionCallAdvance (*runFn))
                    throwErrorForFunction (runFn, Errors::runFunctionMustCallAdvance());
}

void Compiler::checkForInfiniteLoops (Program& program)
{
    for (auto& m : program.getModules())
        for (auto& f : m->functions)
            if (CallFlowGraph::doesFunctionContainInfiniteLoops (*f))
                throwErrorForFunction (f, Errors::functionContainsAnInfiniteLoop (getFunctionName (*f)));
}

void Compiler::throwErrorForFunction (heart::FunctionPtr f, const CompileMessage& error)
{
    if (auto originalFn = findASTFunction (f))
        originalFn->context.throwError (error);
    else
        f->location.throwError (error);
}

std::string Compiler::getFunctionName (heart::Function& f)
{
    if (auto af = findASTFunction (f))
        return af->name;

    return f.name;
}

AST::FunctionPtr Compiler::findASTFunction (heart::FunctionPtr f)
{
    struct FunctionFinder  : public ASTVisitor
    {
        void visit (AST::Function& fn) override
        {
            if (targetFn == fn.generatedFunction)
                result = fn;
        }

        heart::FunctionPtr targetFn;
        AST::FunctionPtr result;
    };

    FunctionFinder finder;
    finder.targetFn = f;
    finder.visitObject (*topLevelNamespace);
    return finder.result;
}

static std::string sanitiseFunctionName (const std::string& name)
{
    if (startsWith (name, "_"))
    {
        auto i = name.find ("_specialised_");

        if (i != std::string::npos && i > 0)
            return quoteName (name.substr (1, i - 1));
    }

    return quoteName (name);
}

void Compiler::checkForRecursiveFunctions (Program& program)
{
    auto recursiveCallSequence = CallFlowGraph::findRecursiveFunctionCallSequences (program);

    if (! recursiveCallSequence.empty())
    {
        std::vector<std::string> functionNames;
        AST::FunctionPtr astFunction;

        for (auto& fn : recursiveCallSequence)
        {
            functionNames.push_back (sanitiseFunctionName (fn->name));

            if (astFunction == nullptr)
                astFunction = findASTFunction (fn);
        }

        auto location = astFunction != nullptr ? astFunction->context : AST::Context();

        if (functionNames.size() == 1)
            location.throwError (Errors::functionCallsItselfRecursively (functionNames.front()));

        if (functionNames.size() == 2)
            location.throwError (Errors::functionsCallEachOtherRecursively (functionNames[0], functionNames[1]));

        if (functionNames.size() > 2)
            location.throwError (Errors::recursiveFunctionCallSequence (joinStrings (functionNames, ", ")));
    }
}

} // namespace soul
