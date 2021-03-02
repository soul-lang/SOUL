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
/**
    Runs multiple passes over the raw AST to attempt to resolve names into references
    to functions, variables, types, etc. and also does some constant and type folding
*/
struct ResolutionPass  final
{
    static void run (AST::Allocator& a, AST::ModuleBase& m, bool ignoreTypeAndConstantErrors)
    {
        ResolutionPass (a, m).run (ignoreTypeAndConstantErrors);
    }

private:
    ResolutionPass (AST::Allocator& a, AST::ModuleBase& m) : allocator (a), module (m)
    {
        intrinsicsNamespacePath = IdentifierPath::fromString (allocator.identifiers, getIntrinsicsNamespaceName());
    }

    AST::Allocator& allocator;
    AST::ModuleBase& module;
    IdentifierPath intrinsicsNamespacePath;

    struct RunStats
    {
        size_t numFailures = 0, numReplaced = 0;

        void clear()
        {
            numFailures = 0;
            numReplaced = 0;
        }

        void add (const RunStats& rhs)
        {
            numFailures += rhs.numFailures;
            numReplaced += rhs.numReplaced;
        }
    };

    RunStats run (bool ignoreTypeAndConstantErrors)
    {
        RunStats runStats;

        if (module.isFullyResolved)
            return runStats;

        if (module.isTemplateModule())
        {
            module.isFullyResolved = true;
            return runStats;
        }

        for (;;)
        {
            runStats.clear();

            tryPass<QualifiedIdentifierResolver> (runStats, true);
            tryPass<TypeResolver> (runStats, true);
            tryPass<ProcessorInstanceResolver> (runStats, true);
            tryPass<NamespaceAliasResolver> (runStats, true);
            tryPass<OperatorResolver> (runStats, true);
            rebuildVariableUseCounts (module);
            tryPass<FunctionResolver> (runStats, true);
            tryPass<ConstantFolder> (runStats, true);

            rebuildVariableUseCounts (module);

            if (runStats.numReplaced == 0)
                tryPass<GenericFunctionResolver> (runStats, true);

            // Can't use a range-based-for here because the array will change during the loop
            for (size_t i = 0; i < module.getSubModules().size(); ++i)
                runStats.add (ResolutionPass (allocator, module.getSubModules()[i])
                                .run (ignoreTypeAndConstantErrors));

            if (runStats.numFailures == 0)
                break;

            if (runStats.numReplaced == 0)
            {
                // failed to resolve anything new, so can't get any further..
                if (ignoreTypeAndConstantErrors)
                    return runStats;

                tryPass<FunctionResolver> (runStats, false);
                tryPass<QualifiedIdentifierResolver> (runStats, false);
                tryPass<TypeResolver> (runStats, false);
                tryPass<ProcessorInstanceResolver> (runStats, false);
                tryPass<NamespaceAliasResolver> (runStats, false);
                tryPass<OperatorResolver> (runStats, false);
                tryPass<GenericFunctionResolver> (runStats, false);
                break;
            }
        }

        SanityCheckPass::runPostResolutionChecks (module);

        module.isFullyResolved = true;
        return runStats;
    }

    template <typename PassType>
    void tryPass (RunStats& runStats, bool ignoreErrors)
    {
        PassType pass (*this, ignoreErrors);
        pass.performPass();
        runStats.numFailures += pass.numFails;
        runStats.numReplaced += pass.itemsReplaced;
    }

    //==============================================================================
    struct ErrorIgnoringRewritingASTVisitor  : public RewritingASTVisitor
    {
        ErrorIgnoringRewritingASTVisitor (ResolutionPass& rp, bool shouldIgnoreErrors)
            : owner (rp), allocator (rp.allocator), module (rp.module), ignoreErrors (shouldIgnoreErrors) {}

        void performPass()   { this->visitObject (module); }

        using super = ErrorIgnoringRewritingASTVisitor;

        using RewritingASTVisitor::visit;

        AST::StaticAssertion& visit (AST::StaticAssertion& a) override
        {
            RewritingASTVisitor::visit (a);
            a.testAndThrowErrorOnFailure();
            return a;
        }

        AST::Statement& visit (AST::IfStatement& i) override
        {
            if (i.isConstIf)
            {
                replaceExpression (i.condition);

                if (auto constant = i.condition->getAsConstant())
                {
                    if (constant->value.getAsBool())
                        replaceStatement (i.trueBranch);
                    else if (i.falseBranch != nullptr)
                        replaceStatement (i.falseBranch);
                }
                else
                {
                    ++numFails;

                    if (ignoreErrors)
                    {
                        catchParseErrors ([this, &i]
                        {
                            replaceStatement (i.trueBranch);
                            replaceStatement (i.falseBranch);
                        });
                    }
                }

                return i;
            }

            return RewritingASTVisitor::visit (i);
        }

        bool failIfNotResolved (AST::Expression& e)
        {
            if (e.isResolved())
                return false;

            ++numFails;
            return true;
        }

        ResolutionPass& owner;
        AST::Allocator& allocator;
        AST::ModuleBase& module;
        size_t numFails = 0;
        const bool ignoreErrors;
    };

    //==============================================================================
    static void rebuildVariableUseCounts (AST::ModuleBase& module)
    {
        struct UseCountResetter  : public ASTVisitor
        {
            void visit (AST::VariableDeclaration& v) override
            {
                ASTVisitor::visit (v);
                v.numReads = 0;
                v.numWrites = 0;
            }
        };

        struct UseCounter  : public ASTVisitor
        {
            void visit (AST::Assignment& a) override
            {
                auto oldWriting = isWriting;
                auto oldReading = isReading;
                isReading = false;
                isWriting = true;
                visitObject (a.target);
                isWriting = oldWriting;
                isReading = oldReading;
                visitObject (a.newValue);
            }

            void visit (AST::PreOrPostIncOrDec& p) override
            {
                auto oldWriting = isWriting;
                auto oldReading = isReading;
                isReading = true;
                isWriting = true;
                ASTVisitor::visit (p);
                isWriting = oldWriting;
                isReading = oldReading;
            }

            void visit (AST::InPlaceOperator& o) override
            {
                auto oldWriting = isWriting;
                auto oldReading = isReading;
                isReading = true;
                isWriting = true;
                visitObject (o.target);
                isWriting = oldWriting;
                isReading = oldReading;
                visitObject (o.source);
            }

            void visit (AST::VariableRef& v) override
            {
                ASTVisitor::visit (v);

                if (isWriting)
                    v.variable->numWrites++;
                else
                    v.variable->numReads++;
            }

            void visit (AST::CallOrCast& c) override
            {
                if (c.arguments != nullptr)
                {
                    // Since we don't know if this might be a function with all pass-by-ref args, we need
                    // to mark all the args as possibly being written..
                    auto oldWriting = isWriting;
                    isWriting = true;
                    ASTVisitor::visit (c);
                    isWriting = oldWriting;
                }
            }

            void visit (AST::FunctionCall& c) override
            {
                if (c.arguments != nullptr)
                {
                    SOUL_ASSERT (c.arguments->items.size() == c.targetFunction.parameters.size());

                    // Visit the function arguments, marking them as writing if the function parameter is pass by reference
                    for (size_t i = 0; i < c.arguments->items.size(); ++i)
                    {
                        auto param = c.targetFunction.parameters[i];
                        auto oldWriting = isWriting;
                        isWriting = param->isResolved() ? param->getType().isReference() : true;
                        visitObject (c.arguments->items[i]);
                        isWriting = oldWriting;
                    }
                }
            }

            bool isReading = true, isWriting = false;
        };

        UseCountResetter resetter;
        UseCounter counter;
        resetter.visitObject (module);
        counter.visitObject (module);
    }

    static AST::TypeCast& convertToCast (AST::Allocator& a, AST::CallOrCast& call, Type destType)
    {
        SOUL_ASSERT (call.arguments != nullptr && ! call.isMethodCall);

        if (auto list = cast<AST::CommaSeparatedList> (call.arguments))
            if (list->items.size() == 1)
                return a.allocate<AST::TypeCast> (call.context, std::move (destType), list->items.front());

        return a.allocate<AST::TypeCast> (call.context, std::move (destType), *call.arguments);
    }

    //==============================================================================
    struct OperatorResolver  : public ErrorIgnoringRewritingASTVisitor
    {
        static inline constexpr const char* getPassName()  { return "OperatorResolver"; }
        using super::visit;

        OperatorResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
            : super (rp, shouldIgnoreErrors) {}

        AST::Expression& visit (AST::BinaryOperator& b) override
        {
            super::visit (b);

            if (b.isOutputEndpoint())
            {
                auto& w = allocator.allocate<AST::WriteToEndpoint> (b.context, b.lhs, b.rhs);
                visitObject (w);
                return w;
            }

            if (! b.isResolved())
                ++numFails;

            return b;
        }

        AST::Expression& visit (AST::InPlaceOperator& o) override
        {
            super::visit (o);

            if (! o.isResolved())
            {
                ++numFails;
                return o;
            }

            if (! o.target->isAssignable())
                o.context.throwError (Errors::operatorNeedsAssignableTarget (BinaryOp::getSymbol (o.operation)));

            SanityCheckPass::throwErrorIfNotReadableValue (o.source);

            auto destType    = o.target->getResultType();
            auto sourceType  = o.source->getResultType();

            auto opTypes = BinaryOp::getTypes (o.operation, destType, sourceType);

            if (! opTypes.resultType.isValid())
                o.context.throwError (Errors::illegalTypesForBinaryOperator (BinaryOp::getSymbol (o.operation),
                                                                             sourceType.getDescription(),
                                                                             destType.getDescription()));

            SanityCheckPass::expectSilentCastPossible (o.context, opTypes.operandType, o.target);
            SanityCheckPass::expectSilentCastPossible (o.context, opTypes.operandType, o.source);

            auto& binaryOp = allocator.allocate<AST::BinaryOperator> (o.context, o.target, o.source, o.operation);

            // special-case handling for addition of an int to a wrap or clamp type, as we want this to
            // work without the user needing to write it out long-hand with a cast
            if (destType.isBoundedInt() && sourceType.isInteger()
                 && (o.operation == BinaryOp::Op::add || o.operation == BinaryOp::Op::subtract))
            {
                auto& resultCast = allocator.allocate<AST::TypeCast> (o.source->context, destType, binaryOp);
                return allocator.allocate<AST::Assignment> (o.context, o.target, resultCast);
            }

            return allocator.allocate<AST::Assignment> (o.context, o.target, binaryOp);
        }

        AST::Expression& visit (AST::WriteToEndpoint& w) override
        {
            super::visit (w);

            if (! w.isResolved())
            {
                ++numFails;
                return w;
            }

            auto& topLevelWrite = ASTUtilities::getTopLevelWriteToEndpoint (w);

            // Either an OutputEndpointRef, or an ArrayElementRef of an OutputEndpointRef
            if (auto outputEndpoint = cast<AST::OutputEndpointRef> (topLevelWrite.target))
                if (ASTUtilities::isConsoleEndpoint (outputEndpoint->output))
                    ASTUtilities::ensureEventEndpointSupportsType (allocator, outputEndpoint->output, w.value->getResultType());

            return w;
        }
    };

    //==============================================================================
    struct ModuleInstanceResolver  : public ErrorIgnoringRewritingASTVisitor
    {
        using super::visit;

        ModuleInstanceResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
        : super (rp, shouldIgnoreErrors) {}

        using ArgList = decltype(AST::CommaSeparatedList::items);
        using ParamList = decltype(AST::ModuleBase::specialisationParams);

        const size_t maxNamespaceInstanceCount = 100;

        template<class M>
        M& visitModule (M& m)
        {
            if (! m.isTemplateModule())
                return super::visit (m);

            visitArray (m.specialisationParams);

            for (auto& p : m.specialisationParams)
                validateSpecialisationParam (p, ignoreErrors);

            return m;
        }

        AST::Graph& visit (AST::Graph& g) override          { return visitModule (g); }
        AST::Processor& visit (AST::Processor& p) override  { return visitModule (p); }
        AST::Namespace& visit (AST::Namespace& n) override  { return visitModule (n); }

        AST::Namespace& getOrAddNamespaceSpecialisation (AST::Namespace& namespaceToClone, const ArgList& specialisationArgs)
        {
            SOUL_ASSERT (specialisationArgs.size() <= namespaceToClone.specialisationParams.size());

            // No parameters, just use the existing namespace
            if (namespaceToClone.specialisationParams.empty())
                return namespaceToClone;

            auto instanceKey = ASTUtilities::getSpecialisationSignature (namespaceToClone.specialisationParams, specialisationArgs);

            for (auto i : namespaceToClone.namespaceInstances)
                if (i.key == instanceKey)
                    return *i.instance;

            auto& parentNamespace = namespaceToClone.getNamespace();
            auto newName = parentNamespace.makeUniqueName ("_" + namespaceToClone.name.toString());
            auto& target = *cast<AST::Namespace> (namespaceToClone.createClone (allocator, parentNamespace, newName));
            namespaceToClone.namespaceInstances.push_back ({ instanceKey, target });

            if (namespaceToClone.namespaceInstances.size() > maxNamespaceInstanceCount)
                namespaceToClone.context.throwError(Errors::tooManyNamespaceInstances (std::to_string (maxNamespaceInstanceCount)));

            resolveAllSpecialisationArgs (specialisationArgs, target.specialisationParams);
            return target;
        }

        bool validateSpecialisationParam (AST::ASTObject& param, bool shouldIgnoreErrors) const
        {
            if (auto u = cast<AST::UsingDeclaration> (param))
            {
                if (u->targetType == nullptr)
                    return true;

                if (AST::isResolvedAsType (*u->targetType))
                    return true;

                if (! shouldIgnoreErrors)
                    u->targetType->context.throwError (Errors::expectedType());

                return false;
            }
            else if (auto pa = cast<AST::ProcessorAliasDeclaration> (param))
            {
                if (pa->targetProcessor == nullptr)
                    return true;

                pa->resolvedProcessor = pa->targetProcessor->getAsProcessor();

                if (pa->resolvedProcessor != nullptr)
                    return true;

                if (! shouldIgnoreErrors)
                    pa->targetProcessor->context.throwError (Errors::expectedProcessorName());

                return false;
            }
            else if (auto v = cast<AST::VariableDeclaration> (param))
            {
                if (v->initialValue == nullptr)
                    return true;

                if (AST::isResolvedAsValue (*v->initialValue))
                    return true;

                if (! shouldIgnoreErrors)
                    v->initialValue->context.throwError (Errors::expectedValue());

                return false;
            }
            else if (auto n = cast<AST::NamespaceAliasDeclaration> (param))
            {
                if (n->targetNamespace == nullptr)
                    return true;

                n->resolvedNamespace = n->targetNamespace->getAsNamespace();

                if (n->resolvedNamespace != nullptr)
                    return true;

                if (! shouldIgnoreErrors)
                    n->targetNamespace->context.throwError (Errors::expectedNamespaceName());

                return false;
            }

            return false;
        }

        bool canResolveSpecialisationArg (AST::Expression& arg, AST::ASTObject& param) const
        {
            if (auto u = cast<AST::UsingDeclaration> (param))
            {
                if (AST::isResolvedAsType (arg))
                    return true;

                if (! ignoreErrors && arg.isResolved())
                    arg.context.throwError (Errors::expectedType());

                return false;
            }
            else if (auto pa = cast<AST::ProcessorAliasDeclaration> (param))
            {
                if (auto prf = cast<AST::ProcessorInstanceRef> (arg))
                    return prf->processorInstance.specialisationArgs == nullptr
                    && prf->getAsProcessor()->specialisationParams.empty();

                if (auto pr = arg.getAsProcessor())
                    return true;

                if (! ignoreErrors && arg.isResolved())
                    arg.context.throwError (Errors::expectedProcessorName());

                return false;
            }
            else if (auto v = cast<AST::VariableDeclaration> (param))
            {
                if (AST::isResolvedAsValue (arg))
                {
                    if (auto variableRef = cast<AST::VariableRef> (arg))
                        if (variableRef->variable->isExternal)
                            return true;

                    if (arg.getAsConstant() == nullptr)
                        return false;

                    SOUL_ASSERT (v->isConstant);
                    return true;
                }

                if (! ignoreErrors && arg.isResolved())
                    arg.context.throwError (Errors::expectedValue());

                return false;
            }
            else if (auto n = cast<AST::NamespaceAliasDeclaration> (param))
            {
                if (arg.isResolved())
                {
                    if (arg.getAsNamespace() != nullptr)
                        return true;

                    if (! ignoreErrors)
                        arg.context.throwError (Errors::expectedNamespaceName());
                }

                return false;
            }

            return false;
        }

        bool validateSpecialisationArgs (const ArgList& args, const ParamList& params, bool shouldIgnoreErrors) const
        {
            for (size_t i = 0; i < params.size(); ++i)
                if (! validateSpecialisationParam (params[i], shouldIgnoreErrors))
                    return false;

            if (args.size() == params.size())
                return true;

            if (args.size() > params.size())
                return false;

            for (auto i = args.size(); i < params.size(); i++)
            {
                if (auto x = cast<AST::UsingDeclaration> (params[i]))
                {
                    if (x->targetType == nullptr)
                        return false;
                }
                else if (auto n = cast<AST::NamespaceAliasDeclaration> (params[i]))
                {
                    if (n->resolvedNamespace == nullptr)
                        return false;
                }
                else if (auto p = cast<AST::ProcessorAliasDeclaration> (params[i]))
                {
                    if (p->resolvedProcessor == nullptr)
                        return false;
                }
                else if (auto v = cast<AST::VariableDeclaration> (params[i]))
                {
                    if (v->initialValue == nullptr)
                        return false;
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

        bool canResolveAllSpecialisationArgs (const ArgList& args, const ParamList& params) const
        {
            SOUL_ASSERT (args.size() <= params.size());

            for (size_t i = 0; i < args.size(); ++i)
                if (! canResolveSpecialisationArg (args[i].get(), params[i]))
                    return false;

            return true;
        }

        static void resolveSpecialisationArg (AST::Expression& arg, AST::ASTObject& param)
        {
            if (auto u = cast<AST::UsingDeclaration> (param))
            {
                SOUL_ASSERT (AST::isResolvedAsType (arg));
                u->targetType = arg;
                return;
            }

            if (auto pa = cast<AST::ProcessorAliasDeclaration> (param))
            {
                auto pr = arg.getAsProcessor();
                pa->resolvedProcessor = *pr;
                return;
            }

            if (auto v = cast<AST::VariableDeclaration> (param))
            {
                SOUL_ASSERT (AST::isResolvedAsValue (arg));

                if (v->isResolved())
                    SanityCheckPass::expectSilentCastPossible (arg.context, v->getType(), arg);

                v->initialValue = arg;
                return;
            }

            if (auto n = cast<AST::NamespaceAliasDeclaration> (param))
            {
                n->resolvedNamespace = arg.getAsNamespace();
                return;
            }

            SOUL_ASSERT_FALSE;
        }

        static void resolveAllSpecialisationArgs (const ArgList& args, ParamList& params)
        {
            SOUL_ASSERT (args.size() <= params.size());

            for (size_t i = 0; i < args.size(); ++i)
                resolveSpecialisationArg (args[i], params[i]);

            params.clear();
        }
    };


    //==============================================================================
    struct QualifiedIdentifierResolver  : public ModuleInstanceResolver
    {
        static inline constexpr const char* getPassName()  { return "QualifiedIdentifierResolver"; }
        using super::visit;

        QualifiedIdentifierResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
            : ModuleInstanceResolver (rp, shouldIgnoreErrors) {}

        void performPass()
        {
            super::performPass();

            if (numVariablesResolved > 0)
            {
                struct RecursiveVariableInitialiserCheck  : public ASTVisitor
                {
                    void visit (AST::VariableDeclaration& v) override
                    {
                        if (contains (stack, pool_ptr<AST::VariableDeclaration> (v)))
                            v.context.throwError (Errors::initialiserRefersToTarget (v.name));

                        if (v.initialValue != nullptr)
                            stack.push_back (v);

                        ASTVisitor::visit (v);

                        if (v.initialValue != nullptr)
                            stack.pop_back();
                    }

                    void visit (AST::VariableRef& vr) override
                    {
                        visit (vr.variable);
                    }

                    std::vector<pool_ref<AST::VariableDeclaration>> stack;
                };

                RecursiveVariableInitialiserCheck().visitObject (module);
            }
        }

        AST::Block& visit (AST::Block& b) override
        {
            auto oldStatement = currentStatement;

            for (auto& s : b.statements)
            {
                currentStatement = s;
                replaceStatement (s);
            }

            currentStatement = oldStatement;
            return b;
        }

        pool_ptr<AST::Namespace> findParameterisedNamespace (AST::QualifiedIdentifier& qi, int& itemsRemoved)
        {
            itemsRemoved = 1;
            auto path = qi.getPath().getParentPath();
            auto p = path.toString();

            while (! path.empty())
            {
                AST::Scope::NameSearch search;
                search.partiallyQualifiedPath = path;
                search.stopAtFirstScopeWithResults = true;
                search.findVariables = false;
                search.findTypes = false;
                search.findFunctions = false;
                search.findNamespaces = true;
                search.findProcessors = false;
                search.findProcessorInstances = false;
                search.findEndpoints = false;

                if (auto scope = qi.getParentScope())
                    scope->performFullNameSearch (search, currentStatement.get());

                if (search.itemsFound.size() != 0)
                {
                    auto item = search.itemsFound.front();

                    if (auto n = cast<AST::Namespace> (item))
                    {
                        if (n->isTemplateModule())
                            return n;

                        return {};
                    }
                }

                path = path.getParentPath();
                itemsRemoved++;
            }

            return {};
        }

        void updateQualifiedIdentifierPrefix (AST::QualifiedIdentifier& qi, AST::Namespace& resolvedNamespace)
        {
            auto index = currentModule->namespaceAliases.size() + 1;
            auto aliasName = allocator.get ("_ns_" + std::to_string (index));

            qi.pathPrefix = IdentifierPath (aliasName);

            auto& aliasDeclaration = allocator.allocate<AST::NamespaceAliasDeclaration> (qi.context, aliasName);
            aliasDeclaration.resolvedNamespace = resolvedNamespace;
            currentModule->namespaceAliases.push_back (aliasDeclaration);
        }

        AST::Expression& visit (AST::QualifiedIdentifier& qi) override
        {
            super::visit (qi);

            auto path = qi.getPath().toString();

            AST::Scope::NameSearch search;
            search.partiallyQualifiedPath = qi.getPath();
            search.stopAtFirstScopeWithResults = true;
            search.findVariables = true;
            search.findTypes = true;
            search.findFunctions = false;
            search.findNamespaces = true;
            search.findProcessors = true;
            search.findProcessorInstances = (parsingProcessorInstance == 0);
            search.findEndpoints = true;

            if (auto scope = qi.getParentScope())
                scope->performFullNameSearch (search, currentStatement.get());

            if (search.itemsFound.size() == 0)
            {
                if (qi.getPath().isQualified())
                {
                    int itemsRemoved = 0;
                    auto targetNamespace = findParameterisedNamespace (qi, itemsRemoved);

                    if (targetNamespace != nullptr)
                    {
                        auto specialisationArgs = AST::CommaSeparatedList::getAsExpressionList (qi.pathSections[0].specialisationArgs);

                        if (! validateSpecialisationArgs (specialisationArgs, targetNamespace->specialisationParams, false))
                            qi.context.throwError (Errors::wrongNumArgsForNamespace (targetNamespace->getFullyQualifiedDisplayPath()));

                        if (canResolveAllSpecialisationArgs (specialisationArgs, targetNamespace->specialisationParams))
                        {
                            auto& resolvedNamespace = getOrAddNamespaceSpecialisation (*targetNamespace, specialisationArgs);
                            updateQualifiedIdentifierPrefix (qi, resolvedNamespace);
                            auto itemsToRemove = static_cast<size_t> (static_cast<int> (qi.pathSections[0].path.size()) - itemsRemoved);
                            qi.pathSections[0].path.removeFirst (itemsToRemove);
                            ++itemsReplaced;
                            return qi;
                        }
                    }
                }
            }
            else if (search.itemsFound.size() == 1)
            {
                auto item = search.itemsFound.front();

                if (qi.isSimplePath())
                {
                    if (auto s = cast<AST::StructDeclaration> (item))
                        return allocator.allocate<AST::StructDeclarationRef> (qi.context, *s);

                    if (auto e = cast<AST::Expression> (item))
                        return *e;

                    if (auto v = cast<AST::VariableDeclaration> (item))
                    {
                        ++numVariablesResolved;
                        return allocator.allocate<AST::VariableRef> (qi.context, *v);
                    }

                    if (auto p = cast<AST::ProcessorBase> (item))
                    {
                        if (currentConnectionEndpoint != nullptr)
                        {
                            auto specialisationArgs = AST::CommaSeparatedList::getAsExpressionList (qi.pathSections[0].specialisationArgs);

                            if (! validateSpecialisationArgs (specialisationArgs, p->specialisationParams, false))
                                return qi;

                            return getOrCreateImplicitProcessorInstance (qi.context, *p, {});
                        }

                        return allocator.allocate<AST::ProcessorRef> (qi.context, *p);
                    }

                    if (auto n = cast<AST::Namespace> (item))
                        return allocator.allocate<AST::NamespaceRef> (qi.context, *n);

                    if (auto na = cast<AST::NamespaceAliasDeclaration> (item))
                    {
                        if (na->isResolved())
                            return allocator.allocate<AST::NamespaceRef> (qi.context, *na->resolvedNamespace);

                        if (na->targetNamespace == &qi)
                            qi.context.throwError (Errors::circularNamespaceAlias (qi.getPath()));
                    }

                    if (auto p = cast<AST::ProcessorInstance> (item))
                        return allocator.allocate<AST::ProcessorInstanceRef> (qi.context, *p);

                    if (auto pa = cast<AST::ProcessorAliasDeclaration> (item))
                    {
                        if (pa->isResolved())
                        {
                            if (currentConnectionEndpoint != nullptr)
                                return getOrCreateImplicitProcessorInstance (qi.context, *pa->resolvedProcessor, {});

                            return allocator.allocate<AST::ProcessorRef> (qi.context, *pa->resolvedProcessor);
                        }
                    }

                    if (auto e = cast<AST::EndpointDeclaration> (item))
                        if (! e->isUnresolvedChildReference())
                            return ASTUtilities::createEndpointRef (allocator, qi.context, *e);
                }
                else
                {
                    if (auto targetNamespace = cast<AST::Namespace> (item))
                    {
                        auto specialisationArgs = AST::CommaSeparatedList::getAsExpressionList (qi.pathSections[0].specialisationArgs);

                        if (! validateSpecialisationArgs (specialisationArgs, targetNamespace->specialisationParams, false))
                            qi.context.throwError (Errors::wrongNumArgsForNamespace (targetNamespace->getFullyQualifiedDisplayPath()));

                        if (canResolveAllSpecialisationArgs (specialisationArgs, targetNamespace->specialisationParams))
                        {
                            auto& resolvedNamespace = getOrAddNamespaceSpecialisation (*targetNamespace, specialisationArgs);
                            updateQualifiedIdentifierPrefix (qi, resolvedNamespace);

                            qi.pathSections.erase (qi.pathSections.begin());
                            ++itemsReplaced;
                            return qi;
                        }

                    }
                }
            }


            if (auto builtInConstant = getBuiltInConstant (qi))
                return *builtInConstant;

            if (auto consoleEndpoint = ASTUtilities::createConsoleEndpoint (allocator, qi))
                return *consoleEndpoint;

            if (ignoreErrors)
            {
                ++numFails;
            }
            else
            {
                if (qi.getPath().isUnqualifiedName ("wrap") || qi.getPath().isUnqualifiedName ("clamp"))
                    return qi;

                if (search.itemsFound.size() > 1)
                    qi.context.throwError (Errors::ambiguousSymbol (qi.getPath()));

                qi.context.throwError (Errors::unresolvedSymbol (qi.getPath()));
            }

            return qi;
        }

        AST::Function& visit (AST::Function& f) override
        {
            if (! f.isGeneric())
                return super::visit (f);

            return f;
        }

        AST::Expression& visit (AST::CallOrCast& call) override
        {
            if (call.arguments != nullptr)
                visitObject (*call.arguments);

            if (call.areAllArgumentsResolved())
            {
                if (AST::isResolvedAsType (call.nameOrType.get()))
                    return convertToCast (allocator, call, call.nameOrType->resolveAsType());

                if (auto name = cast<AST::QualifiedIdentifier> (call.nameOrType))
                {
                    if (! name->isSimplePath())
                    {
                        replaceExpression (call.nameOrType);
                        return call;
                    }

                    if (name->getPath().isQualified())
                        replaceExpression (call.nameOrType);

                    bool canResolveProcessorInstance = (parsingProcessorInstance != 0 || currentConnectionEndpoint != nullptr);

                    AST::Scope::NameSearch search;
                    search.partiallyQualifiedPath = name->getPath();
                    search.stopAtFirstScopeWithResults = true;
                    search.findVariables = false;
                    search.findTypes = true;
                    search.findFunctions = false;
                    search.findNamespaces = canResolveProcessorInstance;
                    search.findProcessors = canResolveProcessorInstance;
                    search.findProcessorInstances = false;
                    search.findEndpoints = false;

                    if (auto scope = name->getParentScope())
                        scope->performFullNameSearch (search, currentStatement.get());

                    if (search.itemsFound.size() == 1)
                    {
                        if (auto e = cast<AST::Expression> (search.itemsFound.front()))
                        {
                            if (AST::isResolvedAsType (e))
                                return allocator.allocate<AST::TypeCast> (call.context, e->resolveAsType(), *call.arguments);

                            if (canResolveProcessorInstance)
                                if (auto p = e->getAsProcessor())
                                    return resolveProcessorInstance (call, *p);
                        }

                        if (canResolveProcessorInstance)
                            if (auto p = cast<AST::ProcessorBase> (search.itemsFound.front()))
                                return resolveProcessorInstance (call, *p);
                    }
                }
                else
                {
                    replaceExpression (call.nameOrType);
                }
            }

            return call;
        }

        AST::ProcessorInstanceRef& getOrCreateImplicitProcessorInstance (AST::Context& c,
                                                                         AST::ProcessorBase& processor,
                                                                         pool_ptr<AST::Expression> arguments)
        {
            auto signature = ASTUtilities::getSpecialisationSignature (processor.specialisationParams,
                                                                       AST::CommaSeparatedList::getAsExpressionList (arguments));

            auto currentGraph = cast<AST::Graph> (currentModule);
            SOUL_ASSERT (currentGraph != nullptr);

            for (auto i : currentGraph->processorInstances)
            {
                if (i->targetProcessor->getAsProcessor() == processor
                     && signature == ASTUtilities::getSpecialisationSignature (processor.specialisationParams,
                                                                               AST::CommaSeparatedList::getAsExpressionList (i->specialisationArgs)))
                {
                    if (i->implicitInstanceSource == nullptr)
                        c.throwError (Errors::cannotUseProcessorInLet (processor.name));

                    if (i->implicitInstanceSource != currentConnectionEndpoint)
                        c.throwError (Errors::cannotReuseImplicitProcessorInstance());

                    return allocator.allocate<AST::ProcessorInstanceRef> (c, i);
                }
            }

            auto& i = allocator.allocate<AST::ProcessorInstance> (c);

            if (arguments == nullptr)
            {
                i.instanceName = allocator.allocate<AST::UnqualifiedName> (c, processor.name);
            }
            else
            {
                auto name = currentGraph->makeUniqueName ("_instance_" + processor.name.toString());
                i.instanceName = allocator.allocate<AST::UnqualifiedName> (c, allocator.get (name));
            }

            i.targetProcessor = allocator.allocate<AST::ProcessorRef> (c, processor);
            i.specialisationArgs = arguments;
            i.implicitInstanceSource = currentConnectionEndpoint;
            currentGraph->addProcessorInstance (i);
            return allocator.allocate<AST::ProcessorInstanceRef> (c, i);
        }

        AST::Expression& resolveProcessorInstance (AST::CallOrCast& call, AST::ProcessorBase& p)
        {
            if (p.specialisationParams.size() == call.getNumArguments())
                return getOrCreateImplicitProcessorInstance (call.context, p, call.arguments);

            return call;
        }

        AST::Expression& visit (AST::ArrayElementRef& s) override
        {
            auto& result = super::visit (s);

            if (s.isResolved())
                SanityCheckPass::checkArraySubscript (s);

            return result;
        }

        pool_ptr<AST::Expression> createSizeForType (const AST::Context& c, const Type& type)
        {
            uint64_t size = 0;

            if (type.isFixedSizeArray() || type.isVector())
                size = type.getArrayOrVectorSize();
            else if (type.isBoundedInt())
                size = (uint64_t) type.getBoundedIntLimit();

            if (size == 0)
            {
                if (! ignoreErrors)
                    c.throwError (Errors::cannotTakeSizeOfType());

                return {};
            }

            return allocator.allocate<AST::Constant> (c, size > 0x7fffffffu ? Value::createInt64 (size)
                                                                            : Value::createInt32 (size));
        }

        pool_ptr<AST::Expression> createTypeMetaFunction (AST::UnqualifiedName& name, AST::Expression& arg)
        {
            auto op = AST::TypeMetaFunction::getOperationForName (name.identifier);

            if (op != AST::TypeMetaFunction::Op::none)
                return allocator.allocate<AST::TypeMetaFunction> (name.context, arg, op);

            return {};
        }

        void visitConnectionEndpoint (AST::Connection::SharedEndpoint& endpoint)
        {
            auto oldEndpoint = currentConnectionEndpoint;
            currentConnectionEndpoint = endpoint;
            replaceExpression (endpoint.endpoint);

            if (is_type<AST::ProcessorRef> (endpoint.endpoint) || is_type<AST::ProcessorBase> (endpoint.endpoint))
            {
                ++itemsReplaced;
                endpoint.endpoint = getOrCreateImplicitProcessorInstance (endpoint.endpoint->context,
                                                                          *endpoint.endpoint->getAsProcessor(), {});
            }

            currentConnectionEndpoint = oldEndpoint;
        }

        AST::Connection& visit (AST::Connection& c) override
        {
            visitConnectionEndpoint (c.source);
            visitConnectionEndpoint (c.dest);
            replaceExpression (c.delayLength);
            return c;
        }

        AST::Expression& visit (AST::DotOperator& d) override
        {
            auto& result = super::visit (d);

            if (std::addressof (result) != std::addressof (d))
                return result;

            if (currentConnectionEndpoint != nullptr)
            {
                if (auto processorInstance = cast<AST::ProcessorInstanceRef> (d.lhs))
                {
                    if (auto processor = processorInstance->getAsProcessor())
                    {
                        AST::Scope::NameSearch search;
                        search.partiallyQualifiedPath = d.rhs.getIdentifierPath();
                        search.stopAtFirstScopeWithResults = true;
                        search.findVariables = false;
                        search.findTypes = false;
                        search.findFunctions = false;
                        search.findNamespaces = false;
                        search.findProcessors = false;
                        search.findProcessorInstances = false;
                        search.findEndpoints = true;

                        processor->performFullNameSearch (search, nullptr);

                        if (search.itemsFound.size() == 1)
                            if (is_type<AST::EndpointDeclaration> (search.itemsFound.front()))
                                return allocator.allocate<AST::ConnectionEndpointRef> (d.context,
                                                                                       processorInstance,
                                                                                       allocator.allocate<AST::UnqualifiedName> (d.context, d.rhs.identifier));
                    }
                }

                if (auto nestedDots = cast<AST::DotOperator> (d.lhs))
                {
                    if (ignoreErrors)
                        ++numFails;
                    else
                        d.context.throwError (Errors::invalidEndpointSpecifier());
                }
            }

            if (failIfNotResolved (d.lhs))
                return result;

            if (AST::isResolvedAsType (d.lhs.get()))
            {
                if (auto metaFunction = createTypeMetaFunction (d.rhs, d.lhs))
                    return *metaFunction;
            }
            else if (AST::isResolvedAsValue (d.lhs.get()))
            {
                auto lhsType = d.lhs->getResultType();

                if (lhsType.isStruct())
                {
                    auto& s = lhsType.getStructRef();
                    auto name = d.rhs.toString();

                    if (s.hasMemberWithName (name))
                        return allocator.allocate<AST::StructMemberRef> (d.context, d.lhs, s, std::move (name));

                    if (! ignoreErrors)
                        d.rhs.context.throwError (Errors::unknownMemberInStruct (d.rhs.toString(), lhsType.getDescription()));
                }
                else if (lhsType.isComplex())
                {
                    auto name = d.rhs.toString();

                    if (name == "real" || name == "imag")
                        return allocator.allocate<AST::ComplexMemberRef> (d.context, d.lhs, lhsType, std::move (name));

                    d.rhs.context.throwError (Errors::unknownMemberInComplex (d.rhs.toString(), lhsType.getDescription()));
                }

                if (auto metaFunction = createTypeMetaFunction (d.rhs, d.lhs))
                    return *metaFunction;
            }
            else if (d.lhs->isOutputEndpoint())
            {
                if (currentConnectionEndpoint != nullptr || d.rhs.toString() == "type")
                    return d;

                d.context.throwError (Errors::noSuchOperationOnEndpoint());
            }
            else if (AST::isResolvedAsProcessor (d.lhs.get()))
            {
                if (currentConnectionEndpoint != nullptr)
                    return d;

                d.context.throwError (Errors::noSuchOperationOnProcessor());
            }

            if (ignoreErrors)
                ++numFails;
            else
                d.context.throwError (Errors::invalidDotArguments());

            return d;
        }

        pool_ptr<AST::Constant> getBuiltInConstant (AST::QualifiedIdentifier& u)
        {
            pool_ptr<AST::Constant> result;

            if (u.getPath().isUnqualified())
                matchBuiltInConstant (u.getPath().getFirstPart(),
                                      [&] (Value&& value)
                                      {
                                          result = allocator.allocate<AST::Constant> (u.context, std::move (value));
                                      });

            return result;
        }

        AST::ProcessorInstance& visit (AST::ProcessorInstance& i) override
        {
            ++parsingProcessorInstance;
            auto& result = super::visit (i);
            --parsingProcessorInstance;
            return result;
        }

        AST::Graph& visit (AST::Graph& g) override
        {
            auto lastModule = currentModule;
            currentModule = g;
            auto& result = ModuleInstanceResolver::visit (g);
            currentModule = lastModule;
            return result;
        }

        AST::Namespace& visit (AST::Namespace& n) override
        {
            auto lastModule = currentModule;
            currentModule = n;
            auto& result = ModuleInstanceResolver::visit (n);
            currentModule = lastModule;
            return result;
        }

        AST::Processor& visit (AST::Processor& p) override
        {
            auto lastModule = currentModule;
            currentModule = p;
            auto& result = ModuleInstanceResolver::visit (p);
            currentModule = lastModule;
            return result;
        }

        pool_ptr<AST::Statement> currentStatement;
        pool_ptr<AST::Connection::SharedEndpoint> currentConnectionEndpoint;
        pool_ptr<AST::ModuleBase> currentModule;
        int parsingProcessorInstance = 0;
        uint32_t numVariablesResolved = 0;
    };

    //==============================================================================
    struct ConstantFolder  : public ErrorIgnoringRewritingASTVisitor
    {
        static inline constexpr const char* getPassName()  { return "ConstantFolder"; }
        using super::visit;

        ConstantFolder (ResolutionPass& rp, bool shouldIgnoreErrors)
            : super (rp, shouldIgnoreErrors) { SOUL_ASSERT (shouldIgnoreErrors); }

        bool isUsedAsReference = false;

        AST::Expression& createConstant (const AST::Context& c, Value v)
        {
            return allocator.allocate<AST::Constant> (c, std::move (v));
        }

        pool_ref<AST::Expression> visitExpression (pool_ref<AST::Expression> e) override
        {
            e = super::visitExpression (e);

            if (e->isResolved())
            {
                if (isUsedAsReference)
                    return e;

                if (auto c = e->getAsConstant())
                    if (c != e)
                        return createConstant (e->context, c->value);

                return e;
            }

            ++numFails;
            return e;
        }

        AST::Expression& visit (AST::VariableRef& v) override
        {
            auto& e = super::visit (v);

            if (failIfNotResolved (e))
                return e;

            if (v.variable->numWrites == 0
                 && v.variable->initialValue != nullptr
                 && ! v.variable->doNotConstantFold)
            {
                if (failIfNotResolved (*v.variable->initialValue))
                    return e;

                if (auto c = visitExpression (*v.variable->initialValue)->getAsConstant())
                {
                    auto t = c->getResultType();

                    if (! t.isArray())   // arrays don't work as constants in LLVM
                    {
                        auto variableResolvedType = v.getResultType();

                        if (t.isIdentical (variableResolvedType))
                            return createConstant (v.context, c->value);

                        if (c->canSilentlyCastTo (variableResolvedType))
                            return createConstant (v.context, c->value.castToTypeExpectingSuccess (variableResolvedType));
                    }
                }
            }

            return e;
        }

        AST::Expression& visit (AST::TernaryOp& t) override
        {
            super::visit (t);

            if (failIfNotResolved (t))
                return t;

            if (AST::isResolvedAsValue (t.condition.get())
                 && AST::isResolvedAsValue (t.trueBranch.get())
                 && AST::isResolvedAsValue (t.falseBranch.get()))
            {
                SanityCheckPass::expectSilentCastPossible (t.context, Type (PrimitiveType::bool_), t.condition);

                auto trueType  = t.trueBranch->getResultType();
                auto falseType = t.falseBranch->getResultType();

                if (trueType.isVoid() || falseType.isVoid())
                    t.context.throwError (Errors::ternaryCannotBeVoid());

                if (! trueType.isIdentical (falseType))
                {
                    bool castToTrue  = TypeRules::canSilentlyCastTo (trueType, falseType);
                    bool castToFalse = TypeRules::canSilentlyCastTo (falseType, trueType);

                    if (! (castToTrue || castToFalse))
                        t.context.throwError (Errors::ternaryTypesMustMatch (trueType.getDescription(),
                                                                             falseType.getDescription()));

                    if (castToTrue)
                    {
                        t.falseBranch = allocator.allocate<AST::TypeCast> (t.falseBranch->context, trueType, t.falseBranch);
                        ++itemsReplaced;
                    }
                    else
                    {
                        t.trueBranch = allocator.allocate<AST::TypeCast> (t.trueBranch->context, falseType, t.trueBranch);
                        ++itemsReplaced;
                    }
                }

                if (auto constant = t.condition->getAsConstant())
                    return constant->value.getAsBool() ? t.trueBranch : t.falseBranch;
            }

            return t;
        }

        AST::Expression& visit (AST::FunctionCall& c) override
        {
            if (c.getNumArguments() != 0)
            {
                auto parameters = c.targetFunction.parameters.begin();
                auto savedIsUsedAsReference = isUsedAsReference;

                for (auto& a : c.arguments->items)
                {
                    auto param = *parameters++;

                    if (param->isResolved())
                    {
                        auto paramType = param->getType();
                        isUsedAsReference = paramType.isReference();

                        if (isUsedAsReference && paramType.isNonConstReference()
                             && AST::isResolvedAsValue (a.get()) && ! a->isAssignable())
                            a->context.throwError (Errors::cannotPassConstAsNonConstRef());

                        replaceExpression (a);
                    }
                }

                isUsedAsReference = savedIsUsedAsReference;

                if (c.targetFunction.isIntrinsic())
                {
                    ArrayWithPreallocation<Value, 4> constantArgs;

                    if (c.arguments != nullptr)
                    {
                        for (auto& arg : c.arguments->items)
                        {
                            if (auto constant = arg->getAsConstant())
                                constantArgs.emplace_back (constant->value);
                            else
                                break;
                        }
                    }

                    if (constantArgs.size() == c.arguments->items.size())
                    {
                        auto result = performIntrinsic (c.targetFunction.intrinsic, constantArgs);

                        if (result.isValid())
                            return createConstant (c.context, std::move (result));
                    }
                }
            }

            failIfNotResolved (c);
            return c;
        }

        AST::Expression& visit (AST::TypeCast& c) override
        {
            super::visit (c);

            if (failIfNotResolved (c))
                return c;

            if (c.getNumArguments() == 0)
                return createConstant (c.context, Value::zeroInitialiser (c.targetType));

            if (auto list = cast<AST::CommaSeparatedList> (c.source))
                return convertExpressionListToConstant (c, c.targetType, *list);

            if (AST::isResolvedAsValue (c.source.get()) && c.source->getResultType().isIdentical (c.targetType))
                return c.source;

            if (auto cv = c.source->getAsConstant())
            {
                auto castValue = cv->value.tryCastToType (c.targetType);

                if (castValue.isValid())
                    return allocator.allocate<AST::Constant> (c.context, castValue);
            }

            return c;
        }

        AST::Expression& visit (AST::UnaryOperator& o) override
        {
            super::visit (o);

            if (failIfNotResolved (o))
                return o;

            if (auto constant = o.source->getAsConstant())
            {
                auto result = constant->value;

                if (UnaryOp::apply (result, o.operation))
                    return createConstant (o.source->context, std::move (result));
            }

            return o;
        }

        AST::Expression& visit (AST::BinaryOperator& b) override
        {
            super::visit (b);

            if (failIfNotResolved (b))
                return b;

            SanityCheckPass::throwErrorIfNotReadableValue (b.rhs);

            if (b.isOutputEndpoint())
            {
                ++numFails;
                return b;
            }

            SanityCheckPass::throwErrorIfNotReadableValue (b.lhs);
            auto resultType = b.getOperandType();

            if (resultType.isValid())
            {
                if (auto lhsConst = b.lhs->getAsConstant())
                {
                    if (auto rhsConst = b.rhs->getAsConstant())
                    {
                        auto result = lhsConst->value;

                        if (BinaryOp::apply (result, rhsConst->value, b.operation,
                                             [&] (CompileMessage message) { b.context.throwError (message); }))
                            return createConstant (b.context, std::move (result));
                    }
                }
            }

            return b;
        }

        AST::Expression& convertExpressionListToConstant (AST::Expression& expr, soul::Type& targetType, AST::CommaSeparatedList& list) const
        {
            auto numArgs = TypeRules::checkArraySizeAndThrowErrorIfIllegal (expr.context, list.items.size());

            if (targetType.isStruct())
            {
                SanityCheckPass::throwErrorIfWrongNumberOfElements (expr.context, targetType, numArgs);

                auto& s = targetType.getStructRef();

                ArrayWithPreallocation<Value, 8> memberValues;
                memberValues.reserve (s.getNumMembers());

                for (size_t i = 0; i < numArgs; ++i)
                {
                    auto memberType = s.getMemberType (i);

                    if (auto constant = list.items[i]->getAsConstant())
                    {
                        if (constant->canSilentlyCastTo (memberType))
                        {
                            memberValues.push_back (constant->value.castToTypeExpectingSuccess (memberType));
                            continue;
                        }

                        if (! ignoreErrors)
                            SanityCheckPass::expectSilentCastPossible (constant->context, memberType, *constant);
                    }

                    return expr;
                }

                return allocator.allocate<AST::Constant> (expr.context, Value::createStruct (s, memberValues));
            }
            else if (targetType.isArrayOrVector())
            {
                if (numArgs == 1)
                {
                    if (auto constant = list.items.front()->getAsConstant())
                        if (TypeRules::canCastTo (targetType, constant->value.getType()))
                            return allocator.allocate<AST::Constant> (expr.context, constant->value.castToTypeExpectingSuccess (targetType));

                    return expr;
                }

                SanityCheckPass::throwErrorIfWrongNumberOfElements (expr.context, targetType, numArgs);

                auto elementType = targetType.getElementType();

                ArrayWithPreallocation<Value, 8> elementValues;
                elementValues.reserve (numArgs);

                for (size_t i = 0; i < numArgs; ++i)
                {
                    if (auto itemList = cast<AST::CommaSeparatedList> (list.items[i]))
                    {
                        auto& e = convertExpressionListToConstant (list.items[i], elementType, *itemList);

                        if (auto constant = cast<AST::Constant> (e))
                        {
                            elementValues.push_back (constant->value.castToTypeExpectingSuccess (elementType));
                            continue;
                        }
                    }

                    if (auto constant = list.items[i]->getAsConstant())
                    {
                        if (TypeRules::canCastTo (elementType, constant->value.getType()))
                        {
                            elementValues.push_back (constant->value.castToTypeExpectingSuccess (elementType));
                            continue;
                        }
                    }

                    return expr;
                }

                if (targetType.isUnsizedArray())
                    return allocator.allocate<AST::Constant> (expr.context, Value::createArrayOrVector (targetType.createCopyWithNewArraySize (numArgs),
                                                                                                     elementValues));

                if (numArgs > 1)
                    SanityCheckPass::throwErrorIfWrongNumberOfElements (expr.context, targetType, numArgs);

                return allocator.allocate<AST::Constant> (expr.context, Value::createArrayOrVector (targetType, elementValues));
            }
            else if (targetType.isComplex())
            {
                if (numArgs != 2)
                    expr.context.throwError (Errors::wrongNumberOfComplexInitialisers());

                auto real = list.items[0]->getAsConstant();
                auto imag = list.items[1]->getAsConstant();

                if (real == nullptr || imag == nullptr)
                    return expr;

                auto attributeType = Type (targetType.isComplex32() ? PrimitiveType::float32 : PrimitiveType::float64);

                SanityCheckPass::expectSilentCastPossible (real->context, attributeType, *real);
                SanityCheckPass::expectSilentCastPossible (imag->context, attributeType, *imag);

                auto realValue = real->value.castToTypeExpectingSuccess (attributeType);
                auto imagValue = imag->value.castToTypeExpectingSuccess (attributeType);

                auto value = targetType.isComplex32() ? Value (std::complex<float> (realValue.getAsFloat(), imagValue.getAsFloat()))
                                                      : Value (std::complex<double> (realValue.getAsDouble(), imagValue.getAsDouble()));

                return allocator.allocate<AST::Constant> (expr.context, value);
            }

            if (numArgs > 1)
                expr.context.throwError (Errors::wrongTypeForInitialiseList());

            if (auto constant = list.items.front()->getAsConstant())
                if (TypeRules::canCastTo (targetType, constant->value.getType()))
                    return allocator.allocate<AST::Constant> (expr.context, constant->value.castToTypeExpectingSuccess (targetType));

            return expr;
        }

        AST::Statement& visit (AST::IfStatement& i) override
        {
            if (i.isConstIf)
            {
                replaceExpression (i.condition);
            }
            else
            {
                auto& result = super::visit (i);
                SOUL_ASSERT (std::addressof (result) == std::addressof (i));
            }

            if (auto constant = i.condition->getAsConstant())
            {
                if (constant->value.getAsBool())
                {
                    replaceStatement (i.trueBranch);
                    return i.trueBranch;
                }

                if (i.falseBranch != nullptr)
                {
                    replaceStatement (i.falseBranch);
                    return *i.falseBranch;
                }

                return allocator.allocate<AST::NoopStatement> (i.context);
            }

            if (i.isConstIf)
            {
                if (! ignoreErrors)
                    i.condition->context.throwError (Errors::expectedConstant());
                else
                    ++numFails;
            }

            return i;
        }
    };

    //==============================================================================
    struct TypeResolver  : public ErrorIgnoringRewritingASTVisitor
    {
        static inline constexpr const char* getPassName()  { return "TypeResolver"; }
        using super::visit;

        TypeResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
            : super (rp, shouldIgnoreErrors) {}

        void performPass() { visitObject (module); }

        AST::Expression& visit (AST::TypeCast& c) override
        {
            super::visit (c);

            if (c.targetType.isUnsizedArray())
            {
                auto numArgs = c.getNumArguments();

                if (c.source->isCompileTimeConstant())
                {
                    auto castValue = c.source->getAsConstant()->value
                                        .tryCastToType (c.targetType.createCopyWithNewArraySize (1));

                    if (castValue.isValid())
                        return allocator.allocate<AST::Constant> (c.source->context, castValue);
                }

                if (numArgs > 1)
                {
                    c.targetType.resolveUnsizedArraySize (numArgs);
                    ++itemsReplaced;
                }
            }

            return c;
        }

        AST::Expression& visit (AST::SubscriptWithBrackets& s) override
        {
            super::visit (s);

            if (AST::isResolvedAsValue (s.lhs.get()))
                return allocator.allocate<AST::ArrayElementRef> (s.context, s.lhs, s.rhs, nullptr, false);

            if (AST::isResolvedAsType (s.lhs.get()))
            {
                if (s.rhs == nullptr)
                    return allocator.allocate<AST::ConcreteType> (s.lhs->context,
                                                                  s.lhs->resolveAsType().createUnsizedArray());

                if (AST::isResolvedAsValue (s.rhs))
                {
                    if (s.rhs->isCompileTimeConstant())
                    {
                        if (auto constant = s.rhs->getAsConstant())
                        {
                            auto size = TypeRules::checkAndGetArraySize (s.rhs->context, constant->value);
                            auto elementType = s.lhs->resolveAsType();

                            if (! elementType.canBeArrayElementType())
                            {
                                if (elementType.isArray())
                                    s.lhs->context.throwError (Errors::notYetImplemented ("Multi-dimensional arrays"));

                                s.lhs->context.throwError (Errors::wrongTypeForArrayElement());
                            }

                            return allocator.allocate<AST::ConcreteType> (s.lhs->context, elementType.createArray (size));
                        }
                    }

                    if (! ignoreErrors)
                        s.context.throwError (Errors::arraySizeMustBeConstant());
                }
            }

            if (AST::isResolvedAsEndpoint (s.lhs.get()))
                return allocator.allocate<AST::ArrayElementRef> (s.context, s.lhs, s.rhs, nullptr, false);

            if (AST::isResolvedAsProcessor (s.lhs.get()))
                s.context.throwError (Errors::notYetImplemented ("Processor Indexes"));

            if (ignoreErrors)
                ++numFails;
            else if (AST::isResolvedAsProcessor (s.lhs.get()))
                s.context.throwError (Errors::arraySuffixOnProcessor());
            else if (s.lhs->isResolved())
                s.context.throwError (Errors::cannotResolveBracketedExp());

            return s;
        }

        AST::Expression& visit (AST::SubscriptWithChevrons& s) override
        {
            super::visit (s);

            if (AST::isResolvedAsType (s.lhs.get()))
            {
                auto type = s.lhs->resolveAsType();

                if (! type.canBeVectorElementType())
                    s.rhs->context.throwError (Errors::wrongTypeForVectorElement());

                if (AST::isResolvedAsValue (s.rhs))
                {
                    if (auto constant = s.rhs->getAsConstant())
                    {
                        auto size = TypeRules::checkAndGetArraySize (s.rhs->context, constant->value);

                        if (! Type::isLegalVectorSize ((int64_t) size))
                            s.rhs->context.throwError (Errors::illegalVectorSize());

                        auto vectorSize = static_cast<Type::ArraySize> (size);
                        return allocator.allocate<AST::ConcreteType> (s.lhs->context, Type::createVector (type.getPrimitiveType(), vectorSize));
                    }
                }
            }

            if (auto name = cast<AST::QualifiedIdentifier> (s.lhs))
            {
                bool isWrap  = name->getPath().isUnqualifiedName ("wrap");
                bool isClamp = name->getPath().isUnqualifiedName ("clamp");

                if (isWrap || isClamp)
                {
                    if (AST::isResolvedAsValue (s.rhs))
                    {
                        if (auto constant = s.rhs->getAsConstant())
                        {
                            auto size = TypeRules::checkAndGetArraySize (s.rhs->context, constant->value);

                            if (! Type::isLegalBoundedIntSize (size))
                                s.rhs->context.throwError (Errors::illegalSize());

                            auto boundingSize = static_cast<Type::BoundedIntSize> (size);

                            return allocator.allocate<AST::ConcreteType> (s.lhs->context,
                                                                          isWrap ? Type::createWrappedInt (boundingSize)
                                                                                 : Type::createClampedInt (boundingSize));
                        }
                        else
                        {
                            if (! ignoreErrors)
                                s.rhs->context.throwError (Errors::wrapOrClampSizeMustBeConstant());
                        }
                    }
                }
            }

            if (ignoreErrors)
                ++numFails;
            else
                s.context.throwError (Errors::cannotResolveVectorSize());

            return s;
        }

        AST::Expression& visit (AST::TypeMetaFunction& c) override
        {
            super::visit (c);

            if (AST::isResolvedAsType (c))
                return allocator.allocate<AST::ConcreteType> (c.context, c.resolveAsType());

            if (AST::isResolvedAsValue (c))
                return allocator.allocate<AST::Constant> (c.context, c.getResultValue());

            if (c.isSizeOfUnsizedType())
            {
                auto& argList = allocator.allocate<AST::CommaSeparatedList> (c.context);
                argList.items.push_back (c.source);

                auto name = allocator.identifiers.get ("get_array_size");
                auto& qi = allocator.allocate<AST::QualifiedIdentifier> (c.context, IdentifierPath (name));
                return allocator.allocate<AST::CallOrCast> (qi, argList, true);
            }

            if (ignoreErrors)
            {
                ++numFails;
            }
            else
            {
                c.throwErrorIfUnresolved();
                c.context.throwError (Errors::cannotResolveSourceType());
            }

            return c;
        }

        AST::Expression& visit (AST::ArrayElementRef& s) override
        {
            super::visit (s);

            if (! ignoreErrors)
                SanityCheckPass::checkArraySubscript (s);

            return s;
        }

        AST::Function& visit (AST::Function& f) override
        {
            if (f.isGeneric())
                return f;

            return super::visit (f);
        }

        AST::StructDeclaration& visit (AST::StructDeclaration& s) override
        {
            recursiveTypeDeclVisitStack.push (s);
            auto& e = super::visit (s);
            recursiveTypeDeclVisitStack.pop();
            return e;
        }

        AST::UsingDeclaration& visit (AST::UsingDeclaration& u) override
        {
            recursiveTypeDeclVisitStack.push (u);
            auto& e = super::visit (u);
            recursiveTypeDeclVisitStack.pop();
            return e;
        }

        AST::Connection& visit (AST::Connection& c) override
        {
            auto oldParentConn = currentConnection;
            currentConnection = c;
            super::visit (c);
            currentConnection = oldParentConn;
            return c;
        }

        AST::Statement& visit (AST::VariableDeclaration& v) override
        {
            super::visit (v);

            if (v.initialValue != nullptr && ! v.isResolved())
            {
                if (AST::isResolvedAsType (v.declaredType))
                {
                    auto destType = v.declaredType->resolveAsType();

                    if (destType.isUnsizedArray())
                    {
                        if (auto size = findSizeOfArray (v.initialValue))
                            resolveVariableDeclarationInitialValue (v, destType.createCopyWithNewArraySize (size));
                    }
                    else
                    {
                        resolveVariableDeclarationInitialValue (v, destType);
                    }
                }
                else if (v.declaredType == nullptr)
                {
                    if (AST::isResolvedAsValue (v.initialValue))
                    {
                        auto type = v.initialValue->getResultType();

                        if (type.isUnsizedArray())
                        {
                            if (auto size = findSizeOfArray (v.initialValue))
                                resolveVariableDeclarationInitialValue (v, type.createCopyWithNewArraySize (size));
                            else
                                resolveVariableDeclarationInitialValue (v, type.createCopyWithNewArraySize (1));
                        }
                    }
                    else if (AST::isResolvedAsType (v.initialValue))
                    {
                        v.initialValue->context.throwError (Errors::expectedValue());
                    }
                }
            }

            return v;
        }

        AST::Expression& visit (AST::BinaryOperator& b) override
        {
            super::visit (b);

            if (b.isResolved())
            {
                SanityCheckPass::throwErrorIfNotReadableValue (b.rhs);

                if (b.isOutputEndpoint())
                {
                    ++numFails;
                    return b;
                }

                SanityCheckPass::throwErrorIfNotReadableValue (b.lhs);
                auto resultType = b.getOperandType();

                if (! resultType.isValid() && ! ignoreErrors)
                    SanityCheckPass::throwErrorForBinaryOperatorTypes (b);
            }

            return b;
        }

        Type::ArraySize findSizeOfArray (pool_ptr<AST::Expression> value)
        {
            if (value != nullptr)
            {
                if (AST::isResolvedAsValue (value))
                {
                    auto type = value->getResultType();

                    if (type.isFixedSizeArray())
                        return type.getArraySize();
                }

                if (auto list = cast<AST::CommaSeparatedList> (value))
                    return TypeRules::checkArraySizeAndThrowErrorIfIllegal (value->context, list->items.size());

                if (auto c = cast<AST::TypeCast> (value))
                {
                    if (c->targetType.isFixedSizeArray())
                        return c->targetType.getArraySize();

                    if (c->targetType.isUnsizedArray())
                        return findSizeOfArray (c->source);
                }

                if (auto call = cast<AST::CallOrCast> (value))
                {
                    if (AST::isResolvedAsType (call->nameOrType.get()))
                    {
                        auto type = call->nameOrType->resolveAsType();

                        if (type.isFixedSizeArray())
                            return type.getArraySize();
                    }
                }
            }

            return 0;
        }

        void resolveVariableDeclarationInitialValue (AST::VariableDeclaration& v, const Type& type)
        {
            if (AST::isResolvedAsValue (v.initialValue))
            {
                if (! v.initialValue->getResultType().isIdentical (type))
                {
                    SanityCheckPass::expectSilentCastPossible (v.initialValue->context, type, *v.initialValue);
                    v.initialValue = allocator.allocate<AST::TypeCast> (v.initialValue->context, type, *v.initialValue);
                }

                v.declaredType = {};
                ++itemsReplaced;
            }
            else if (is_type<AST::CommaSeparatedList> (v.initialValue))
            {
                v.initialValue = allocator.allocate<AST::TypeCast> (v.initialValue->context, type, *v.initialValue);
                v.declaredType = {};
                ++itemsReplaced;
            }
        }

        SanityCheckPass::RecursiveTypeDeclVisitStack recursiveTypeDeclVisitStack;
        pool_ptr<AST::Connection> currentConnection;
    };


    //==============================================================================
    struct ProcessorInstanceResolver  : public ModuleInstanceResolver
    {
        static inline constexpr const char* getPassName()  { return "ProcessorInstanceResolver"; }
        using super = ModuleInstanceResolver;
        using super::visit;

        ProcessorInstanceResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
            : super (rp, shouldIgnoreErrors) {}

        AST::ProcessorInstance& visit (AST::ProcessorInstance& instance) override
        {
            super::visit (instance);

            if (auto p = instance.targetProcessor->getAsProcessor())
            {
                if (p->owningInstance == instance)
                    return instance;

                auto specialisationArgs = AST::CommaSeparatedList::getAsExpressionList (instance.specialisationArgs);
                auto target = pool_ref<AST::ProcessorBase> (*p);

                if (! validateSpecialisationArgs (specialisationArgs, target->specialisationParams, ignoreErrors))
                {
                    if (ignoreErrors)
                    {
                        ++numFails;
                        return instance;
                    }

                    instance.context.throwError (Errors::wrongNumArgsForNamespace (target->getFullyQualifiedDisplayPath()));
                }

                auto& graph = *cast<AST::Graph> (instance.getParentScope()->findProcessor());
                SanityCheckPass::RecursiveGraphDetector::check (graph);

                if (! instance.isImplicitlyCreated())
                    if (! graph.getMatchingSubModules (instance.instanceName->getIdentifierPath()).empty())
                        instance.context.throwError (Errors::alreadyProcessorWithName (instance.getReadableName()));

                if (! canResolveAllSpecialisationArgs (specialisationArgs, target->specialisationParams))
                {
                    ++numFails;
                    return instance;
                }

                bool requiresSpecialisation = ! target->specialisationParams.empty();

                if (target->owningInstance != nullptr || requiresSpecialisation)
                {
                    auto nameRoot = target->name.toString();

                    if (requiresSpecialisation)
                        nameRoot = TokenisedPathString::join (nameRoot,
                                                              "_for_" + makeSafeIdentifierName (choc::text::replace (graph.getFullyQualifiedPath().toString(), ":", "_")
                                                                  + "_" + instance.instanceName->toString()));
                    auto& ns = target->getNamespace();
                    target = *cast<AST::ProcessorBase> (target->createClone (allocator, ns, ns.makeUniqueName (nameRoot)));

                    if (requiresSpecialisation)
                    {
                        auto oldCloneFn = target->createClone;

                        target->createClone = [=] (AST::Allocator& a, AST::Namespace& parentNS, const std::string& newName) -> AST::ModuleBase&
                        {
                            auto& m = oldCloneFn (a, parentNS, newName);
                            resolveAllSpecialisationArgs (specialisationArgs, m.specialisationParams);
                            return m;
                        };

                        resolveAllSpecialisationArgs (specialisationArgs, target->specialisationParams);
                    }
                }

                target->owningInstance = instance;
                target->originalBeforeSpecialisation = p;
                instance.targetProcessor = allocator.allocate<AST::ProcessorRef> (instance.context, target);
                instance.specialisationArgs = nullptr;
                ++itemsReplaced;
                return instance;
            }

            if (! ignoreErrors)
                instance.targetProcessor->context.throwError (Errors::expectedProcessorName());

            ++numFails;
            return instance;
        }
    };

    //==============================================================================
    struct NamespaceAliasResolver  : public ModuleInstanceResolver
    {
        static inline constexpr const char* getPassName()  { return "NamespaceInstanceResolver"; }
        using super = ModuleInstanceResolver;
        using super::visit;

        NamespaceAliasResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
            : super (rp, shouldIgnoreErrors) {}

        AST::NamespaceAliasDeclaration& visit (AST::NamespaceAliasDeclaration& instance) override
        {
            super::visit (instance);

            if (instance.isResolved())
                return instance;

            if (instance.targetNamespace == nullptr)
                return instance;

            auto specialisationArgs = AST::CommaSeparatedList::getAsExpressionList (instance.specialisationArgs);

            if (auto target = instance.targetNamespace->getAsNamespace())
            {
                if (! validateSpecialisationArgs (specialisationArgs, target->specialisationParams, false))
                    instance.context.throwError (Errors::wrongNumArgsForNamespace (target->getFullyQualifiedDisplayPath()));

                if (canResolveAllSpecialisationArgs (specialisationArgs, target->specialisationParams))
                {
                    instance.resolvedNamespace = getOrAddNamespaceSpecialisation (*target, specialisationArgs);
                    ++itemsReplaced;
                    return instance;
                }
            }
            else
            {
                if (! ignoreErrors)
                    instance.targetNamespace->context.throwError (Errors::expectedNamespaceName());
            }

            ++numFails;
            return instance;
        }
    };

    //==============================================================================
    struct FunctionResolver  : public ErrorIgnoringRewritingASTVisitor
    {
        static inline constexpr const char* getPassName()  { return "FunctionResolver"; }
        using super::visit;

        FunctionResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
            : super (rp, shouldIgnoreErrors) {}

        AST::Expression& visit (AST::CallOrCast& call) override
        {
            super::visit (call);

            if (AST::isResolvedAsType (call.nameOrType.get()))
                return convertToCast (allocator, call, call.nameOrType->resolveAsType());

            if (call.areAllArgumentsResolved())
            {
                if (auto name = cast<AST::QualifiedIdentifier> (call.nameOrType))
                {
                    if (name->getPath().isUnqualifiedName ("advance"))
                        return createAdvanceCall (call);

                    if (name->getPath().isUnqualifiedName ("static_assert"))
                        return ASTUtilities::createStaticAssertion (call.context, allocator, call.arguments->items);

                    if (name->getPath().isUnqualifiedName ("at"))
                        return createAtCall (call);

                    if (! name->isSimplePath())
                    {
                        numFails++;
                        return call;
                    }

                    if (call.arguments != nullptr)
                    {
                        for (auto& arg : call.arguments->items)
                        {
                            if (! AST::isResolvedAsValue (arg.get()))
                            {
                                if (ignoreErrors)
                                {
                                    ++numFails;
                                    return call;
                                }

                                SanityCheckPass::throwErrorIfNotReadableValue (arg);
                            }
                        }
                    }

                    auto possibles = findAllPossibleFunctions (call, *name);

                    for (auto& f : possibles)
                        if (f.functionIsNotResolved)
                            return call;

                    auto totalMatches = possibles.size();

                    // If there's only one function found, and we can call it (maybe with a cast), then go for it..
                    if (totalMatches == 1 && ! possibles.front().isImpossible)
                    {
                        if (auto resolved = resolveFunction (possibles.front(), call, ignoreErrors))
                            return *resolved;

                        return call;
                    }

                    auto exactMatches = countNumberOfExactMatches (possibles);

                    // If there's one exact match, then even if there are others requiring casts, we'll ignore them
                    // and go for the one which is a perfect match..
                    if (exactMatches == 1)
                    {
                        for (auto& f : possibles)
                        {
                            if (f.isExactMatch())
                            {
                                if (auto resolved = resolveFunction (f, call, ignoreErrors))
                                    return *resolved;

                                return call;
                            }
                        }

                        SOUL_ASSERT_FALSE;
                    }

                    // If there are any generic functions, see if exactly one of these works
                    ArrayWithPreallocation<pool_ref<AST::Expression>, 4> matchingGenerics;

                    for (auto& f : possibles)
                    {
                        if (! f.isImpossible && f.requiresGeneric)
                        {
                            if (auto e = resolveFunction (f, call, true))
                                matchingGenerics.push_back (*e);
                            else if (! canResolveGenerics())
                                return call;
                        }
                    }

                    if (matchingGenerics.size() == 1)
                        return matchingGenerics.front();

                    if (! ignoreErrors || matchingGenerics.size() > 1)
                    {
                        if (totalMatches == 0)
                            throwErrorForUnknownFunction (call, *name);

                        auto possibleWithCast = countNumberOfMatchesWithCast (possibles);

                        if (exactMatches + possibleWithCast == 0)
                        {
                            if (totalMatches == 1 && ! possibles.front().requiresGeneric)
                            {
                                auto paramTypes = possibles.front().function.getParameterTypes();
                                SOUL_ASSERT (paramTypes.size() == call.getNumArguments());

                                for (size_t i = 0; i < paramTypes.size(); ++i)
                                    if (! TypeRules::canPassAsArgumentTo (paramTypes[i], call.arguments->items[i]->getResultType(), true))
                                        SanityCheckPass::expectSilentCastPossible (call.arguments->items[i]->context,
                                                                                   paramTypes[i], call.arguments->items[i]);
                            }

                            if (totalMatches == 0 || matchingGenerics.size() <= 1)
                                call.context.throwError (Errors::noMatchForFunctionCall (call.getDescription (name->getPath().toString())));
                        }

                        if (totalMatches > 1 || matchingGenerics.size() > 1)
                        {
                            ArrayWithPreallocation<pool_ref<AST::Function>, 4> functions;

                            for (auto& f : possibles)
                                functions.push_back (f.function);

                            SanityCheckPass::checkForDuplicateFunctions (functions);

                            call.context.throwError (Errors::ambiguousFunctionCall (call.getDescription (name->getPath().toString())));
                        }
                    }
                }
            }

            ++numFails;
            return call;
        }

        struct PossibleFunction
        {
            PossibleFunction() = delete;
            PossibleFunction (PossibleFunction&&) = default;

            PossibleFunction (AST::Function& f, choc::span<Type> argTypes,
                              choc::span<pool_ptr<AST::Constant>> constantArgValues)
                : function (f)
            {
                for (size_t i = 0; i < argTypes.size(); ++i)
                {
                    Type targetParamType;

                    if (! function.parameters[i]->isResolved())
                    {
                        if (function.isGeneric())
                            requiresGeneric = true;
                        else
                            functionIsNotResolved = true;

                        continue;
                    }

                    targetParamType = function.parameters[i]->getType();

                    if (TypeRules::canPassAsArgumentTo (targetParamType, argTypes[i], true))
                        continue;

                    if (! TypeRules::canPassAsArgumentTo (targetParamType, argTypes[i], false))
                    {
                        if (constantArgValues[i] == nullptr
                             || ! TypeRules::canSilentlyCastTo (targetParamType, constantArgValues[i]->value))
                            isImpossible = true;
                    }

                    requiresCast = true;
                }
            }

            AST::Function& function;

            bool isImpossible = false;
            bool requiresCast = false;
            bool requiresGeneric = false;
            bool functionIsNotResolved = false;

            bool isExactMatch() const       { return ! (isImpossible || requiresCast || requiresGeneric); }
        };

        pool_ptr<AST::Expression> resolveFunction (const PossibleFunction& f, AST::CallOrCast& call, bool ignoreErrorsInGenerics)
        {
            if (f.function.isRunFunction() || f.function.isUserInitFunction())
                call.context.throwError (Errors::cannotCallFunction (f.function.name));

            if (f.function.isGeneric())
                return createCallToGenericFunction (call, f.function, ignoreErrorsInGenerics);

            return allocator.allocate<AST::FunctionCall> (call.context, f.function, call.arguments, false);
        }

        virtual bool canResolveGenerics() const     { return false; }

        virtual pool_ptr<AST::Expression> createCallToGenericFunction (AST::CallOrCast&, AST::Function&, bool)
        {
            ++numFails;
            return {};
        }

        ArrayWithPreallocation<PossibleFunction, 4> findAllPossibleFunctions (const AST::CallOrCast& call,
                                                                              const AST::QualifiedIdentifier& name)
        {
            auto argTypes = call.getArgumentTypes();

            AST::Scope::NameSearch search;
            search.partiallyQualifiedPath = name.getPath();
            search.stopAtFirstScopeWithResults = false;
            search.requiredNumFunctionArgs = (int) argTypes.size();
            search.findVariables = false;
            search.findTypes = false;
            search.findFunctions = true;
            search.findNamespaces = false;
            search.findProcessors = false;
            search.findProcessorInstances = false;
            search.findEndpoints = false;

            call.getParentScope()->performFullNameSearch (search, nullptr);

            if (name.getPath().isUnqualified())
            {
                // Handle intrinsics with no explicit namespace
                search.partiallyQualifiedPath = owner.intrinsicsNamespacePath.withSuffix (search.partiallyQualifiedPath.getLastPart());
                call.getParentScope()->performFullNameSearch (search, nullptr);

                // Handle "Koenig" lookup for method calls
                if (call.isMethodCall)
                {
                    SOUL_ASSERT (argTypes.size() != 0);

                    if (argTypes.front().isStruct())
                    {
                        if (auto ownerASTObject = argTypes.front().getStructRef().backlinkToASTObject)
                        {
                            auto structDecl = reinterpret_cast<AST::StructDeclaration*> (ownerASTObject);

                            search.partiallyQualifiedPath = name.getPath();
                            structDecl->context.parentScope->performFullNameSearch (search, nullptr);
                        }
                    }
                }
            }

            ArrayWithPreallocation<pool_ptr<AST::Constant>, 4> constantArgs;

            if (call.arguments != nullptr)
            {
                constantArgs.reserve (argTypes.size());

                for (auto& c : call.arguments->items)
                    constantArgs.push_back (c->getAsConstant());
            }
            else
            {
                constantArgs.resize (argTypes.size());
            }

            ArrayWithPreallocation<PossibleFunction, 4> results;

            for (auto& i : search.itemsFound)
                if (auto f = cast<AST::Function> (i))
                    if (f->originalGenericFunction == nullptr)
                        results.push_back (PossibleFunction (*f, argTypes, constantArgs));

            return results;
        }

        static size_t countNumberOfExactMatches (choc::span<PossibleFunction> matches)
        {
            return (size_t) std::count_if (matches.begin(), matches.end(), [=] (const PossibleFunction& f) { return f.isExactMatch(); });
        }

        static size_t countNumberOfMatchesWithCast (choc::span<PossibleFunction> matches)
        {
            return (size_t) std::count_if (matches.begin(), matches.end(), [=] (const PossibleFunction& f) { return f.requiresCast && ! f.isImpossible; });
        }

        void throwErrorForUnknownFunction (AST::CallOrCast& call, AST::QualifiedIdentifier& name)
        {
            AST::Scope::NameSearch search;
            search.partiallyQualifiedPath = name.getPath();
            search.stopAtFirstScopeWithResults = true;
            search.findVariables = true;
            search.findTypes = true;
            search.findFunctions = true;
            search.findNamespaces = true;
            search.findProcessors = true;
            search.findProcessorInstances = false;
            search.findEndpoints = true;

            if (auto scope = name.getParentScope())
                scope->performFullNameSearch (search, nullptr);

            if (name.getPath().isUnqualified())
            {
                search.partiallyQualifiedPath = owner.intrinsicsNamespacePath.withSuffix (search.partiallyQualifiedPath.getLastPart());
                call.getParentScope()->performFullNameSearch (search, nullptr);
            }

            size_t numFunctions = 0;

            for (auto& i : search.itemsFound)
                if (is_type<AST::Function> (i))
                    ++numFunctions;

            if (numFunctions > 0)
                name.context.throwError (Errors::noFunctionWithNumberOfArgs (name.getPath(),
                                                                             std::to_string (call.getNumArguments())));

            if (! search.itemsFound.empty())
            {
                if (is_type<AST::Processor> (search.itemsFound.front()))
                    name.context.throwError (Errors::cannotUseProcessorAsFunction());

                if (auto e = cast<AST::EndpointDeclaration> (search.itemsFound.front()))
                    name.context.throwError (e->isInput ? Errors::cannotUseInputAsFunction()
                                                        : Errors::cannotUseOutputAsFunction());
            }

            auto possibleFunction = findPossibleMisspeltFunction (name.getPath().getLastPart());

            if (! possibleFunction.empty())
                name.context.throwError (Errors::unknownFunctionWithSuggestion (name.getPath(), possibleFunction));

            name.context.throwError (Errors::unknownFunction (name.getPath()));
        }

        std::string findPossibleMisspeltFunction (const std::string& name)
        {
            std::string nearest;
            size_t lowestDistance = 5;

            AST::Scope* topLevelScope = std::addressof (module);

            while (topLevelScope->getParentScope() != nullptr)
                topLevelScope = topLevelScope->getParentScope();

            findLeastMisspeltFunction (*topLevelScope, name, nearest, lowestDistance);

            nearest = Program::stripRootNamespaceFromQualifiedPath (nearest);
            return TokenisedPathString::removeTopLevelNameIfPresent (nearest, getIntrinsicsNamespaceName());
        }

        static void findLeastMisspeltFunction (AST::Scope& scope, const std::string& name, std::string& nearest, size_t& lowestDistance)
        {
            for (auto& f : scope.getFunctions())
            {
                auto functionName = f->name.toString();
                auto distance = choc::text::getLevenshteinDistance (name, functionName);

                if (distance < lowestDistance)
                {
                    lowestDistance = distance;
                    nearest = TokenisedPathString::join (scope.getFullyQualifiedPath().toString(), functionName);
                }
            }

            for (auto& sub : scope.getSubModules())
                findLeastMisspeltFunction (sub, name, nearest, lowestDistance);
        }

        AST::Expression& createAdvanceCall (AST::CallOrCast& c)
        {
            if (c.isMethodCall)             c.context.throwError (Errors::advanceIsNotAMethod());
            if (c.getNumArguments() != 0)   c.context.throwError (Errors::advanceHasNoArgs());

            return allocator.allocate<AST::AdvanceClock> (c.context);
        }

        AST::Expression& createAtCall (AST::CallOrCast& call)
        {
            if (call.getNumArguments() != 2)
                call.context.throwError (Errors::atMethodTakes1Arg());

            auto& array = call.arguments->items[0].get();
            auto& index = call.arguments->items[1].get();

            if (! index.canSilentlyCastTo (PrimitiveType::int64))
                SanityCheckPass::expectSilentCastPossible (call.context, Type (PrimitiveType::int32), index);

            if (array.kind == AST::ExpressionKind::endpoint)
            {
                SOUL_ASSERT (AST::isResolvedAsOutput (array));
                pool_ptr<AST::Expression> endpointArraySize;

                if (auto endpoint = array.getAsEndpoint())
                {
                    if (endpoint->isUnresolvedChildReference())
                        array.context.throwError (Errors::cannotResolveSourceOfAtMethod());

                    endpointArraySize = endpoint->getDetails().arraySize;
                }

                Type::BoundedIntSize arraySize = 0;

                if (endpointArraySize != nullptr)
                {
                    SOUL_ASSERT (AST::isResolvedAsConstant (endpointArraySize));
                    arraySize = static_cast<Type::BoundedIntSize> (TypeRules::checkAndGetArraySize (endpointArraySize->context,
                                                                                                    endpointArraySize->getAsConstant()->value));
                }

                if (arraySize == 0)
                    call.context.throwError (Errors::wrongTypeForAtMethod());
            }
            else if (array.kind == AST::ExpressionKind::value)
            {
                auto arrayType = array.getResultType();

                if (! arrayType.isArrayOrVector())
                    call.context.throwError (Errors::wrongTypeForAtMethod());
            }
            else
            {
                call.context.throwError (Errors::expectedValueOrEndpoint());
            }

            auto& ref = allocator.allocate<AST::ArrayElementRef> (call.context, array, index, nullptr, false);
            ref.suppressWrapWarning = true;
            return ref;
        }

        AST::Function& visit (AST::Function& f) override
        {
            if (! f.isGeneric())
                return super::visit (f);

            return f;
        }

        AST::Expression& visit (AST::ArrayElementRef& s) override
        {
            super::visit (s);

            if (! ignoreErrors)
                SanityCheckPass::checkArraySubscript (s);

            return s;
        }
    };

    //==============================================================================
    struct GenericFunctionResolver  : public FunctionResolver
    {
        static inline constexpr const char* getPassName()  { return "GenericFunctionResolver"; }

        GenericFunctionResolver (ResolutionPass& rp, bool shouldIgnoreErrors)
            : FunctionResolver (rp, shouldIgnoreErrors)
        {
        }

        bool canResolveGenerics() const override        { return true; }

        pool_ptr<AST::Expression> createCallToGenericFunction (AST::CallOrCast& call, AST::Function& genericFunction,
                                                               bool shouldIgnoreErrors) override
        {
            SOUL_ASSERT (genericFunction.isGeneric());

            if (auto newFunction = getOrCreateSpecialisedFunction (call, genericFunction,
                                                                   call.getArgumentTypes(),
                                                                   shouldIgnoreErrors))
            {
                auto& newCall = allocator.allocate<AST::FunctionCall> (call.context, *newFunction, call.arguments, call.isMethodCall);
                newFunction->originalCallLeadingToSpecialisation = newCall;
                return newCall;
            }

            return {};
        }

        std::string getIDStringForFunction (const AST::Function& resolvedGenericFunction)
        {
            AST::TypeArray types;

            for (auto t : resolvedGenericFunction.genericSpecialisations)
                types.push_back (t->resolveAsType());

            return ASTUtilities::getTypeArraySignature (types);
        }

        pool_ptr<AST::Function> getOrCreateSpecialisedFunction (AST::CallOrCast& call,
                                                                AST::Function& genericFunction,
                                                                choc::span<Type> callerArgumentTypes,
                                                                bool shouldIgnoreErrors)
        {
            auto parentScope = genericFunction.getParentScope();
            SOUL_ASSERT (parentScope != nullptr);

            AST::TypeArray resolvedTypes;

            if (findGenericFunctionTypes (call, genericFunction, callerArgumentTypes, resolvedTypes, shouldIgnoreErrors))
            {
                auto callerSignatureID = ASTUtilities::getTypeArraySignature (resolvedTypes);

                for (auto& f : parentScope->getFunctions())
                    if (f->originalGenericFunction == genericFunction && getIDStringForFunction (f) == callerSignatureID)
                        return f;

                auto& newFunction = StructuralParser::cloneFunction (allocator, genericFunction);
                newFunction.name = allocator.get ("_" + genericFunction.name.toString() + heart::getGenericSpecialisationNameTag());
                newFunction.originalGenericFunction = genericFunction;
                applyGenericFunctionTypes (newFunction, resolvedTypes);

                return newFunction;
            }

            return {};
        }

        bool findGenericFunctionTypes (AST::CallOrCast& call, AST::Function& function, choc::span<Type> callerArgumentTypes, AST::TypeArray& resolvedTypes, bool shouldIgnoreErrors)
        {
            for (auto wildcardToResolve : function.genericWildcards)
            {
                auto wildcardName = wildcardToResolve->identifier;
                Type resolvedType;

                for (size_t i = 0; i < function.parameters.size(); ++i)
                {
                    if (auto paramType = function.parameters[i]->declaredType)
                    {
                        bool anyReferencesInvolved = false;
                        auto newMatch = matchParameterAgainstWildcard (*paramType, callerArgumentTypes[i], wildcardName, anyReferencesInvolved);

                        if (newMatch.isValid())
                        {
                            if (! newMatch.isReference())
                                newMatch = newMatch.removeConstIfPresent();

                            if (resolvedType.isValid())
                            {
                                if (! newMatch.isIdentical (resolvedType))
                                {
                                    if (! shouldIgnoreErrors)
                                        throwResolutionError (call, function, wildcardToResolve->context,
                                                              "Could not find a value for " + quoteName (wildcardName) + " that satisfies all argument types");

                                    return false;
                                }
                            }
                            else
                            {
                                resolvedType = newMatch;
                            }
                        }
                    }
                }

                if (! resolvedType.isValid())
                {
                    if (! shouldIgnoreErrors)
                        throwResolutionError (call, function, wildcardToResolve->context,
                                              "Failed to resolve generic parameter " + quoteName (wildcardName));
                    return false;
                }

                resolvedTypes.push_back (resolvedType);
            }

            return true;
        }

        void applyGenericFunctionTypes (AST::Function& function, const AST::TypeArray& resolvedTypes)
        {
            SOUL_ASSERT (function.genericWildcards.size() == resolvedTypes.size());

            for (size_t i = 0; i < resolvedTypes.size(); i++)
            {
                auto wildcardToResolve = function.genericWildcards[i];
                auto& resolvedType = resolvedTypes[i];

                auto& usingDecl = allocator.allocate<AST::UsingDeclaration> (wildcardToResolve->context,
                                                                             wildcardToResolve->identifier,
                                                                             allocator.allocate<AST::ConcreteType> (AST::Context(), resolvedType));
                function.genericSpecialisations.push_back (usingDecl);
            }

            function.genericWildcards.clear();
        }


        void throwResolutionError (AST::CallOrCast& call, AST::Function& function,
                                   const AST::Context& errorLocation, const std::string& errorMessage)
        {
            CompileMessageGroup messages;

            if (function.context.location.sourceCode->isInternal)
            {
                messages.messages.push_back (CompileMessage::createError ("Could not resolve argument types for function call " + call.getDescription (function.name),
                                                                          call.context.location));
            }
            else
            {
                messages.messages.push_back (CompileMessage::createError ("Failed to resolve generic function call " + call.getDescription (function.name),
                                                                          call.context.location));

                messages.messages.push_back (CompileMessage::createError (errorMessage, errorLocation.location));
            }

            soul::throwError (messages);
        }

        Type matchParameterAgainstWildcard (AST::Expression& paramType,
                                            const Type& callerArgumentType,
                                            const Identifier& wildcardToFind,
                                            bool& anyReferencesInvolved)
        {
            if (auto unresolvedTypeName = cast<AST::QualifiedIdentifier> (paramType))
            {
                if (unresolvedTypeName->getPath().isUnqualifiedName (wildcardToFind))
                    return callerArgumentType;
            }
            else if (auto mf = cast<AST::TypeMetaFunction> (paramType))
            {
                if (mf->isMakingConst())
                    return matchParameterAgainstWildcard (mf->source, callerArgumentType.removeConstIfPresent(), wildcardToFind, anyReferencesInvolved);

                if (mf->isMakingReference())
                {
                    anyReferencesInvolved = true;
                    return matchParameterAgainstWildcard (mf->source, callerArgumentType.removeReferenceIfPresent(), wildcardToFind, anyReferencesInvolved);
                }
            }
            else if (auto sb = cast<AST::SubscriptWithBrackets> (paramType))
            {
                if (callerArgumentType.isArray() && sb->rhs == nullptr)
                    return matchParameterAgainstWildcard (sb->lhs, callerArgumentType.getElementType(), wildcardToFind, anyReferencesInvolved);

                if (callerArgumentType.isFixedSizeArray() && sb->rhs != nullptr)
                {
                    if (auto sizeConst = sb->rhs->getAsConstant())
                    {
                        if (sizeConst->value.getType().isPrimitiveInteger())
                        {
                            auto size = sizeConst->value.getAsInt64();

                            if (size == (int64_t) callerArgumentType.getArraySize())
                                return matchParameterAgainstWildcard (sb->lhs, callerArgumentType.getElementType(), wildcardToFind, anyReferencesInvolved);
                        }
                    }
                }
            }
            else if (auto sc = cast<AST::SubscriptWithChevrons> (paramType))
            {
                if (callerArgumentType.isVector())
                {
                    if (auto sizeConst = sc->rhs->getAsConstant())
                    {
                        if (sizeConst->value.getType().isPrimitiveInteger())
                        {
                            auto size = sizeConst->value.getAsInt64();

                            if (size == (int64_t) callerArgumentType.getVectorSize())
                                return matchParameterAgainstWildcard (sc->lhs, callerArgumentType.getElementType(), wildcardToFind, anyReferencesInvolved);
                        }
                    }
                }
            }

            return {};
        }
    };
};

} // namespace soul
