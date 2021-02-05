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
    Provides various types of sanity-check on some AST
*/
struct SanityCheckPass  final
{
    /** Does some high-level checks after an initial parse and before name resolution, */
    static void runPreResolution (AST::ModuleBase& module)
    {
        checkOverallStructure (module);
    }

    static void runPostResolutionChecks (AST::ModuleBase& module)
    {
        checkOverallStructure (module);
        PostResolutionChecks().ASTVisitor::visitObject (module);
    }

    static void runPreHEARTGenChecks (AST::ModuleBase& module)
    {
        runEventFunctionChecker (module);
        runDuplicateNameChecker (module);
        PostResolutionChecks().ASTVisitor::visitObject (module);
        PreAndPostIncOperatorCheck().ASTVisitor::visitObject (module);
    }

    static void runEventFunctionChecker (AST::ModuleBase& module)
    {
        EventFunctionChecker().visitObject (module);
    }

    static void runDuplicateNameChecker (AST::ModuleBase& module)
    {
        DuplicateNameChecker().visitObject (module);
    }

    struct RecursiveTypeDeclVisitStack
    {
        ArrayWithPreallocation<AST::TypeDeclarationBase*, 8> stack;

        void push (AST::TypeDeclarationBase& t)
        {
            if (contains (stack, &t))
            {
                if (stack.back() == &t)
                    t.context.throwError (Errors::typeContainsItself (t.name));

                t.context.throwError (Errors::typesReferToEachOther (t.name, stack.back()->name));
            }

            stack.push_back (&t);
        }

        void pop()
        {
            stack.pop_back();
        }
    };

    static void throwErrorIfNotReadableValue (AST::Expression& e)
    {
        if (! AST::isResolvedAsValue (e))
        {
            if (is_type<AST::OutputEndpointRef> (e))
                e.context.throwError (Errors::cannotReadFromOutput());

            if (auto input = cast<AST::InputEndpointRef> (e))
                if (input->isResolved())
                    if (isEvent (input->input->getDetails()))
                        e.context.throwError (Errors::cannotReadFromEventInput());

            if (is_type<AST::ProcessorRef> (e) || is_type<AST::ProcessorInstanceRef> (e))
                e.context.throwError (Errors::cannotUseProcessorAsValue());

            e.context.throwError (Errors::expectedValue());
        }
    }

    static void throwErrorIfNotArrayOrVector (AST::Expression& e)
    {
        throwErrorIfNotReadableValue (e);

        if (! e.getResultType().isArrayOrVector())
            e.context.throwError (Errors::expectedArrayOrVector());
    }

    static void throwErrorIfNotReadableType (AST::Expression& e)
    {
        if (! AST::isResolvedAsType (e))
        {
            if (is_type<AST::ProcessorRef> (e))
                e.context.throwError (Errors::cannotUseProcessorAsType());

            e.context.throwError (Errors::expectedType());
        }
    }

    static void expectCastPossible (const AST::Context& context, const Type& targetType, const Type& sourceType)
    {
        if (! TypeRules::canCastTo (targetType, sourceType))
            context.throwError (Errors::cannotCastBetween (sourceType.getDescription(), targetType.getDescription()));
    }

    static void expectSilentCastPossible (const AST::Context& context, const Type& targetType, AST::Expression& source)
    {
        if (auto list = cast<AST::CommaSeparatedList> (source))
        {
            throwErrorIfWrongNumberOfElements (context, targetType, list->items.size());

            if (targetType.isArrayOrVector())
            {
                auto elementType = targetType.getElementType();

                for (auto& i : list->items)
                    expectSilentCastPossible (i->context, elementType, i);

                return;
            }

            if (targetType.isStruct())
            {
                auto& s = targetType.getStructRef();

                for (size_t i = 0; i < list->items.size(); ++i)
                    expectSilentCastPossible (list->items[i]->context, s.getMemberType (i), list->items[i]);

                return;
            }

            context.throwError (Errors::cannotCastListToType (targetType.getDescription()));
        }

        throwErrorIfNotReadableValue (source);

        if (! source.canSilentlyCastTo (targetType))
        {
            if (auto c = source.getAsConstant())
                if (c->getResultType().isPrimitive())
                    context.throwError (Errors::cannotImplicitlyCastValue (c->value.getDescription(),
                                                                           c->value.getType().getDescription(),
                                                                           targetType.getDescription()));

            auto resultType = source.getResultType();
            SOUL_ASSERT (resultType.isValid());
            context.throwError (Errors::cannotImplicitlyCastType (resultType.getDescription(), targetType.getDescription()));
        }
    }

    static void expectSilentCastPossible (const AST::Context& context, ArrayView<Type> targetTypes, AST::Expression& source)
    {
        auto sourceType = source.getResultType();

        int matches = 0;

        for (auto& type : targetTypes)
        {
            // If we have an exact match, it doesn't matter how many other types could be used silently
            if (sourceType.isEqual (type, Type::ignoreVectorSize1))
                return;

            if (source.canSilentlyCastTo (type))
                ++matches;
        }

        if (matches == 0)
            context.throwError (Errors::cannotImplicitlyCastType (sourceType.getDescription(),
                                                                  heart::Utilities::getDescriptionOfTypeList (targetTypes, false)));

        if (matches > 1)
            context.throwError (Errors::ambiguousCastBetween (sourceType.getDescription(),
                                                              heart::Utilities::getDescriptionOfTypeList (targetTypes, false)));
    }

    static void throwErrorIfMultidimensionalArray (const AST::Context& location, const Type& type)
    {
        if (type.isArray())
        {
            auto elementType = type.getArrayElementType();

            if (elementType.isArray())
                location.throwError (Errors::notYetImplemented ("Multi-dimensional arrays"));

            throwErrorIfMultidimensionalArray (location, elementType);
        }

        if (type.isStruct())
            for (auto& m : type.getStructRef().getMembers())
                throwErrorIfMultidimensionalArray (location, m.type);
    }

    static void checkArraySubscript (AST::ArrayElementRef& s)
    {
        if (! AST::isResolvedAsEndpoint (s.object))
            throwErrorIfNotArrayOrVector (*s.object);
    }

    static void throwErrorIfWrongNumberOfElements (const AST::Context& c, const Type& type, size_t numberAvailable)
    {
        if (type.isFixedSizeAggregate() && type.getNumAggregateElements() != numberAvailable)
            c.throwError (Errors::wrongNumArgsForAggregate (type.getDescription()));
    }

    static void throwErrorForBinaryOperatorTypes (AST::BinaryOperator& b)
    {
        if (b.lhs->getResultType().isArray() && b.rhs->getResultType().isArray())
            b.context.throwError (Errors::cannotOperateOnArrays (getSymbol (b.operation)));

        b.context.throwError (Errors::illegalTypesForBinaryOperator (getSymbol (b.operation),
                                                                     b.lhs->getResultType().getDescription(),
                                                                     b.rhs->getResultType().getDescription()));
    }

    static int64_t checkDelayLineLength (const AST::Context& context, const Value& v)
    {
        if (! v.getType().isPrimitiveInteger())
            context.throwError (Errors::delayLineMustHaveIntLength());

        auto value = v.getAsInt64();

        if (value < 1)
            context.throwError (Errors::delayLineTooShort());

        if (value > (int64_t) AST::maxDelayLineLength)
            context.throwError (Errors::delayLineTooLong());

        return value;
    }

    static uint32_t checkLatency (AST::Expression& latency)
    {
        if (AST::isResolvedAsConstant (latency))
        {
            if (auto constant = latency.getAsConstant())
            {
                if (constant->value.getType().isPrimitiveInteger())
                {
                    auto value = constant->value.getAsInt64();

                    if (value < 0 || value > AST::maxInternalLatency)
                        latency.context.throwError (Errors::latencyOutOfRange());

                    return static_cast<uint32_t> (value);
                }
            }
        }

        latency.context.throwError (Errors::latencyMustBeConstInteger());
    }

    static void checkForDuplicateFunctions (ArrayView<pool_ref<AST::Function>> functions)
    {
        std::vector<std::string> functionSigs;
        functionSigs.reserve (functions.size());

        for (auto& f : functions)
        {
            if (! f->isGeneric())
            {
                auto newSig = ASTUtilities::getFunctionSignature (f);

                if (contains (functionSigs, newSig))
                    f->context.throwError (Errors::duplicateFunction());

                functionSigs.push_back (newSig);
            }
        }
    }

    static void checkEndpointDataTypes (AST::EndpointDeclaration& endpoint)
    {
        if (endpoint.isUnresolvedChildReference())
            return;

        auto& details = endpoint.getDetails();
        auto& types = details.dataTypes;

        for (auto& t : types)
            throwErrorIfNotReadableType (t);

        auto resolvedTypes = details.getResolvedDataTypes();
        SOUL_ASSERT (types.size() != 0 && resolvedTypes.size() == types.size());
        auto dataType = resolvedTypes.front();

        if (isStream (details.endpointType))
        {
            SOUL_ASSERT (types.size() == 1);

            if (! (dataType.isPrimitive() || dataType.isVector()))
                types.front()->context.throwError (Errors::illegalTypeForEndpoint());
        }
        else
        {
            checkTypeSupportedForExternalEvents (types.front()->context, dataType);
        }

        if (types.size() > 1)
        {
            for (size_t i = 1; i < types.size(); ++i)
                for (size_t j = 0; j < i; ++j)
                    if (resolvedTypes[i].isEqual (resolvedTypes[j], Type::ignoreVectorSize1))
                        types[j]->context.throwError (Errors::duplicateTypesInList (resolvedTypes[j].getDescription(),
                                                                                    resolvedTypes[i].getDescription()));
        }

        if (details.arraySize != nullptr)
            for (size_t i = 0; i < types.size(); ++i)
                if (resolvedTypes[i].isArray())
                    types[i]->context.throwError (Errors::illegalTypeForEndpointArray());
    }

    static void checkTypeSupportedForExternalEvents (const AST::Context& context, const Type& t)
    {
        if (t.isBoundedInt())
            context.throwError (Errors::notYetImplemented ("Endpoints using wrap or clamp types"));

        if (t.isArray())
            checkTypeSupportedForExternalEvents (context, t.getArrayElementType());

        if (t.isStruct())
            for (auto& m : t.getStructRef().getMembers())
                checkTypeSupportedForExternalEvents (context, m.type);
    }

    //==============================================================================
    struct RecursiveGraphDetector
    {
        static void check (AST::Graph& g, const RecursiveGraphDetector* stack = nullptr)
        {
            for (auto s = stack; s != nullptr; s = s->previous)
                if (s->graph == std::addressof (g))
                    g.context.throwError (Errors::recursiveTypes (g.getFullyQualifiedPath()));

            const RecursiveGraphDetector newStack { stack, std::addressof (g) };

            for (auto& p : g.processorInstances)
            {
                // avoid using findSingleMatchingSubModule() as we don't want an error thrown if
                // a processor specialisation alias has not yet been resolved

                pool_ptr<AST::Graph> sub;

                if (auto pr = cast<AST::ProcessorRef> (p->targetProcessor))
                {
                    sub = cast<AST::Graph> (pr->processor);
                }
                else if (auto name = cast<AST::QualifiedIdentifier> (p->targetProcessor))
                {
                    auto modulesFound = g.getMatchingSubModules (name->getPath());

                    if (modulesFound.size() == 1)
                        sub = cast<AST::Graph> (modulesFound.front());
                }

                if (sub != nullptr)
                    return check (*sub, std::addressof (newStack));
            }
        }

        const RecursiveGraphDetector* previous = nullptr;
        const AST::Graph* graph = nullptr;
    };


private:
    //==============================================================================
    static void checkOverallStructure (AST::ModuleBase& module)
    {
        if (auto p = cast<AST::ProcessorBase> (module))
            checkOverallStructureOfProcessor (*p);

        for (auto m : module.getSubModules())
            checkOverallStructure (m);
    }

    static void checkOverallStructureOfProcessor (AST::ProcessorBase& processorOrGraph)
    {
        if (processorOrGraph.getNumOutputs() == 0)
            processorOrGraph.context.throwError (Errors::processorNeedsAnOutput());

        if (auto processor = cast<AST::Processor> (processorOrGraph))
        {
            int numRunFunctions = 0;

            for (auto& f : processor->getFunctions())
            {
                if (f->isRunFunction() || f->isUserInitFunction())
                {
                    if (! f->returnType->resolveAsType().isVoid())
                        f->context.throwError (Errors::functionMustBeVoid (f->name));

                    if (! f->parameters.empty())
                        f->context.throwError (Errors::functionHasParams (f->name));

                    if (f->isRunFunction())
                        ++numRunFunctions;
                }
            }

            // If the processor has non-event I/O then we need a run processor
            if (numRunFunctions == 0)
            {
                auto areAllEndpointsResolved = [&]
                {
                    for (auto e : processorOrGraph.getEndpoints())
                        if (! e->isResolved())
                            return false;

                    return true;
                };

                auto hasAnEventEndpoint = [&]
                {
                    for (auto e : processorOrGraph.getEndpoints())
                        if (isEvent (e->getDetails()))
                            return true;

                    return false;
                };

                if (areAllEndpointsResolved() && ! hasAnEventEndpoint())
                    processor->context.throwError (Errors::processorNeedsRunFunction());
            }

            if (numRunFunctions > 1)
                processor->context.throwError (Errors::multipleRunFunctions());
        }
    }

    //==============================================================================
    struct EventFunctionChecker : public ASTVisitor
    {
        using super = ASTVisitor;

        void visit (AST::Processor& p) override
        {
            super::visit (p);

            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& e : p.getEndpoints())   duplicateNameChecker.check (e->name, e->context);
            for (auto& v : p.stateVariables)   duplicateNameChecker.check (v->name, v->context);
            for (auto& s : p.structures)       duplicateNameChecker.check (s->name, s->context);
            for (auto& u : p.usings)           duplicateNameChecker.check (u->name, u->context);

            // (functions must be scanned last)
            for (auto& f : p.functions)
            {
                if (f->isEventFunction())
                {
                    bool nameFound = false;

                    for (auto& e : p.getEndpoints())
                    {
                        if (e->isInput && e->name == f->name)
                        {
                            nameFound = true;
                            const auto& details = e->getDetails();

                            if (details.arraySize == nullptr && f->parameters.size() == 1)
                            {
                                auto eventType = f->parameters.front()->getType().removeConstIfPresent().removeReferenceIfPresent();
                                auto types = details.getResolvedDataTypes();

                                if (! eventType.isPresentIn (types))
                                    f->context.throwError (Errors::eventFunctionInvalidType (f->name, eventType.getDescription()));
                            }
                            else if (details.arraySize != nullptr && f->parameters.size() == 2)
                            {
                                auto indexType = f->parameters.front()->getType().removeConstIfPresent().removeReferenceIfPresent();
                                auto eventType = f->parameters.back()->getType().removeConstIfPresent().removeReferenceIfPresent();
                                auto types = details.getResolvedDataTypes();

                                if (! indexType.isInteger())
                                    f->context.throwError (Errors::eventFunctionIndexInvalid());

                               if (! eventType.isPresentIn (types))
                                   f->context.throwError (Errors::eventFunctionInvalidType (f->name, eventType.getDescription()));
                            }
                            else
                            {
                                f->context.throwError (Errors::eventFunctionInvalidArguments());
                            }
                        }
                   }

                    if (! nameFound)
                        f->context.throwError (Errors::noSuchInputEvent (f->name));
                }
            }
        }
    };

    //==============================================================================
    struct DuplicateNameChecker  : public ASTVisitor
    {
        using super = ASTVisitor;

        void visit (AST::Processor& p) override
        {
            super::visit (p);
            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& e : p.endpoints)        duplicateNameChecker.check (e->name, e->context);
            for (auto& v : p.stateVariables)   duplicateNameChecker.check (v->name, v->context);
            for (auto& s : p.structures)       duplicateNameChecker.check (s->name, s->context);
            for (auto& u : p.usings)           duplicateNameChecker.check (u->name, u->context);
            for (auto& a : p.namespaceAliases) duplicateNameChecker.check (a->name, a->context);

            // (functions must be scanned last)
            for (auto& f : p.functions)
                if (! f->isEventFunction())
                    duplicateNameChecker.checkWithoutAdding (f->name, f->nameLocation);

            for (auto& m : p.getSubModules())
                duplicateNameChecker.check (m->name, m->context);
        }

        void visit (AST::Annotation& a) override
        {
            super::visit (a);
            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& p : a.properties)
                duplicateNameChecker.check (p.name->toString(), p.name->context);
        }

        void visit (AST::Graph& g) override
        {
            super::visit (g);
            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& e : g.getEndpoints())
                duplicateNameChecker.check (e->name, e->context);
        }

        void visit (AST::Namespace& n) override
        {
            super::visit (n);

            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& s : n.structures)       duplicateNameChecker.check (s->name, s->context);
            for (auto& u : n.usings)           duplicateNameChecker.check (u->name, u->context);
            for (auto& m : n.subModules)       duplicateNameChecker.check (m->name, m->context);
            for (auto& c : n.constants)        duplicateNameChecker.check (c->name, c->context);
            for (auto& a : n.namespaceAliases) duplicateNameChecker.check (a->name, a->context);

            // (functions must be scanned last)
            for (auto& f : n.functions)     duplicateNameChecker.checkWithoutAdding (f->name, f->nameLocation);
        }

        void visit (AST::Block& b) override
        {
            super::visit (b);
            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& s : b.statements)
                if (auto v = cast<AST::VariableDeclaration> (s))
                    duplicateNameChecker.check (v->name, v->context);
        }

        void visit (AST::Function& f) override
        {
            super::visit (f);
            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& param : f.parameters)
                duplicateNameChecker.check (param->name, param->context);

            if (f.block != nullptr)
            {
                // Ensure top level block variables do not duplicate parameter names
                for (auto& s : f.block->statements)
                    if (auto v = cast<AST::VariableDeclaration> (s))
                        duplicateNameChecker.check (v->name, v->context);
            }
        }

        void visit (AST::StructDeclaration& s) override
        {
            super::visit (s);
            soul::DuplicateNameChecker duplicateNameChecker;

            for (auto& m : s.getMembers())
                duplicateNameChecker.check (m.name, s.context);
        }
    };

    //==============================================================================
    struct PostResolutionChecks  : public ASTVisitor
    {
        using super = ASTVisitor;

        void visit (AST::UnqualifiedName& name) override
        {
            super::visit (name);
            name.context.throwError (Errors::unresolvedSymbol (name.toString()));
        }

        void visit (AST::QualifiedIdentifier& qi) override
        {
            super::visit (qi);
            qi.context.throwError (Errors::unresolvedSymbol (qi.getPath()));
        }

        void visit (AST::CallOrCast& c) override
        {
            super::visit (c);
            c.context.throwError (Errors::cannotResolveFunctionOrCast());
        }

        void visit (AST::VariableDeclaration& v) override
        {
            super::visit (v);

            if (v.declaredType == nullptr)
                throwErrorIfNotReadableValue (*v.initialValue);
            else
                throwErrorIfNotReadableType (*v.declaredType);

            auto type = v.getType();
            auto context = (v.declaredType != nullptr ? v.declaredType->context : v.context);

            if (type.isVoid())
                context.throwError (Errors::variableCannotBeVoid());

            throwErrorIfMultidimensionalArray (context, type);
        }

        void visit (AST::Processor& p) override
        {
            super::visit (p);
            checkForDuplicateFunctions (p.functions);

            for (auto input : p.endpoints)
                checkEndpointDataTypes (input.get());

            for (auto& v : p.stateVariables)
                if (v->initialValue != nullptr && ! v->initialValue->isCompileTimeConstant())
                    v->initialValue->context.throwError (Errors::expectedConstant());
        }

        void visit (AST::Graph& g) override
        {
            super::visit (g);

            for (auto input : g.endpoints)
                checkEndpointDataTypes (input.get());

            for (auto& v : g.constants)
                if (! v->isCompileTimeConstant())
                    v->context.throwError (Errors::nonConstInGraph());

            RecursiveGraphDetector::check (g);

            struct CycleDetector  : public heart::Utilities::GraphTraversalHelper<CycleDetector, AST::ProcessorInstance, AST::Connection, AST::Context>
            {
                CycleDetector (AST::Graph& graph)
                {
                    reserve (graph.processorInstances.size());

                    for (auto& p : graph.processorInstances)
                        addNode (p);

                    for (auto& c : graph.connections)
                        if (c->delayLength == nullptr)
                            if (auto src = c->getSourceProcessor())
                                if (auto dst = c->getDestProcessor())
                                    addConnection (*src, *dst, c);
                }

                static std::string getProcessorName (const AST::ProcessorInstance& p)  { return p.getReadableName(); }
                static const AST::Context& getContext (const AST::Connection& c)       { return c.context; }
            };

            CycleDetector (g).checkAndThrowErrorIfCycleFound();
        }

        void visit (AST::Namespace& n) override
        {
            super::visit (n);
            checkForDuplicateFunctions (n.functions);

            for (auto& v : n.constants)
                if (! v->isCompileTimeConstant())
                    v->context.throwError (Errors::nonConstInNamespace());
        }

        void visit (AST::Function& f) override
        {
            if (! f.isGeneric())
            {
                for (auto& p : f.parameters)
                    if (AST::isResolvedAsType (p->declaredType) && p->getType().isVoid())
                        p->context.throwError (Errors::parameterCannotBeVoid());

                super::visit (f);
                throwErrorIfNotReadableType (*f.returnType);
            }
        }

        void visit (AST::StructDeclaration& s) override
        {
            recursiveTypeDeclVisitStack.push (s);
            super::visit (s);
            recursiveTypeDeclVisitStack.pop();

            for (auto& m : s.getMembers())
            {
                throwErrorIfNotReadableType (m.type);

                if (m.type->getConstness() == AST::Constness::definitelyConst)
                    m.type->context.throwError (Errors::memberCannotBeConst());
            }
        }

        void visit (AST::UsingDeclaration& u) override
        {
            recursiveTypeDeclVisitStack.push (u);
            super::visit (u);
            recursiveTypeDeclVisitStack.pop();
        }

        RecursiveTypeDeclVisitStack recursiveTypeDeclVisitStack;

        void visit (AST::EndpointDeclaration& e) override
        {
            super::visit (e);

            if (heart::isReservedFunctionName (e.name))
                e.context.throwError (Errors::invalidEndpointName (e.name));

            if (e.isResolved())
            {
                checkEndpointDataTypes (e);
                checkArraySize (e.getDetails().arraySize, AST::maxEndpointArraySize);
            }
        }

        void visit (AST::ProcessorInstance& i) override
        {
            super::visit (i);
            checkArraySize (i.arraySize, AST::maxProcessorArraySize);

            if (i.clockMultiplierRatio != nullptr)   validateClockRatio (*i.clockMultiplierRatio);
            if (i.clockDividerRatio != nullptr)      validateClockRatio (*i.clockDividerRatio);
        }

        static void validateClockRatio (AST::Expression& ratio)
        {
            if (auto c = ratio.getAsConstant())
                heart::getClockRatioFromValue (ratio.context, c->value);
            else
                ratio.context.throwError (Errors::ratioMustBeConstant());
        }

        void visit (AST::Connection& c) override
        {
            super::visit (c);

            if (c.delayLength != nullptr)
            {
                throwErrorIfNotReadableValue (*c.delayLength);

                if (auto cv = c.delayLength->getAsConstant())
                    checkDelayLineLength (cv->context, cv->value);
            }
        }

        void visit (AST::Assignment& a) override
        {
            super::visit (a);

            if (! a.target->isAssignable())
                a.context.throwError (Errors::operatorNeedsAssignableTarget ("="));

            expectSilentCastPossible (a.context,
                                      a.target->getResultType().withConstAndRefFlags (false, false),
                                      a.newValue);
        }

        void visitObject (AST::Statement& t) override
        {
            if (auto e = cast<AST::Expression> (t))
            {
                if (AST::isResolvedAsType (e))
                    t.context.throwError (Errors::expectedStatement());

                if (e->isCompileTimeConstant())
                    t.context.throwError (Errors::expressionHasNoEffect());
            }

            super::visitObject (t);
        }

        void visit (AST::PreOrPostIncOrDec& p) override
        {
            super::visit (p);

            auto getOperatorName = [] (const AST::PreOrPostIncOrDec& pp)  { return pp.isIncrement ? "++" : "--"; };

            if (! p.target->isAssignable())
                p.context.throwError (Errors::operatorNeedsAssignableTarget (getOperatorName (p)));

            auto type = p.target->getResultType();

            if (type.isBool() || ! (type.isPrimitive() || type.isBoundedInt()))
                p.context.throwError (Errors::illegalTypeForOperator (getOperatorName (p)));
        }

        void visit (AST::IfStatement& i) override
        {
            if (i.isConstIf)
                i.condition->context.throwError (Errors::expectedConstant());

            super::visit (i);
        }

        void visit (AST::UnaryOperator& u) override
        {
            super::visit (u);

            if (! UnaryOp::isTypeSuitable (u.operation, u.source->getResultType()))
                u.source->context.throwError (Errors::wrongTypeForUnary());
        }

        void visit (AST::BinaryOperator& b) override
        {
            super::visit (b);

            throwErrorIfNotReadableValue (b.rhs);

            if (b.isOutputEndpoint())
                return;

            throwErrorIfNotReadableValue (b.lhs);

            auto operandType = b.getOperandType();

            if (! operandType.isValid())
                throwErrorForBinaryOperatorTypes (b);

            if (BinaryOp::isComparisonOperator (b.operation))
            {
                auto lhsConst = b.lhs->getAsConstant();
                auto rhsConst = b.rhs->getAsConstant();
                int result = 0;

                if (lhsConst != nullptr && rhsConst == nullptr)
                    result = BinaryOp::getResultOfComparisonWithBoundedType (b.operation, lhsConst->value, b.rhs->getResultType());

                if (lhsConst == nullptr && rhsConst != nullptr)
                    result = BinaryOp::getResultOfComparisonWithBoundedType (b.operation, b.lhs->getResultType(), rhsConst->value);

                if (result != 0)
                    b.context.throwError (result > 0 ? Errors::comparisonAlwaysTrue()
                                                     : Errors::comparisonAlwaysFalse());
            }
        }

        void visit (AST::TernaryOp& t) override
        {
            super::visit (t);
            throwErrorIfNotReadableValue (t.condition);
            throwErrorIfNotReadableValue (t.trueBranch);
            throwErrorIfNotReadableValue (t.falseBranch);
            expectSilentCastPossible (t.context, Type (PrimitiveType::bool_), t.condition);
        }

        void visit (AST::TypeCast& c) override
        {
            super::visit (c);
            SOUL_ASSERT (c.getNumArguments() != 0); // should have already been caught by the constant folder

            if (c.targetType.isUnsizedArray())
                c.context.throwError (Errors::notYetImplemented ("cast to unsized arrays"));

            if (auto list = cast<AST::CommaSeparatedList> (c.source))
            {
                auto numArgs = list->items.size();

                if (numArgs != 1)
                    throwErrorIfWrongNumberOfElements (c.context, c.targetType, numArgs);
            }
        }

        void visit (AST::ReturnStatement& r) override
        {
            super::visit (r);

            auto returnTypeExp = r.getParentFunction()->returnType;
            throwErrorIfNotReadableType (*returnTypeExp);
            auto returnType = returnTypeExp->resolveAsType();

            if (r.returnValue != nullptr)
                expectSilentCastPossible (r.context, returnType, *r.returnValue);
            else if (! returnType.isVoid())
                r.context.throwError (Errors::voidFunctionCannotReturnValue());
        }

        void visit (AST::LoopStatement& loop) override
        {
            super::visit (loop);

            if (loop.numIterations != nullptr)
            {
                if (auto c = loop.numIterations->getAsConstant())
                    if (c->value.getAsInt64() <= 0)
                        loop.numIterations->context.throwError (Errors::negativeLoopCount());

                expectSilentCastPossible (loop.numIterations->context,
                                          Type (PrimitiveType::int64), *loop.numIterations);
            }
        }

        static Type getDataTypeOfArrayRefLHS (AST::ASTObject& o)
        {
            if (auto e = cast<AST::EndpointDeclaration> (o))
                if (e->isResolved())
                    return e->getDetails().getSampleArrayTypes().front();

            if (auto e = cast<AST::Expression> (o))
            {
                if (auto endpoint = e->getAsEndpoint())
                    if (endpoint->isResolved())
                        return endpoint->getDetails().getSampleArrayTypes().front();

                return e->getResultType();
            }

            return {};
        }

        void visit (AST::ArrayElementRef& s) override
        {
            super::visit (s);

            auto lhsType = getDataTypeOfArrayRefLHS (*s.object);

            if (! lhsType.isArrayOrVector())
            {
                if (AST::isResolvedAsEndpoint (s.object))
                    s.object->context.throwError (Errors::cannotUseBracketOnEndpoint());

                s.object->context.throwError (Errors::expectedArrayOrVectorForBracketOp());
            }

            if (auto startIndexConst = s.startIndex->getAsConstant())
            {
                auto startIndex = TypeRules::checkAndGetArrayIndex (s.startIndex->context, startIndexConst->value);

                if (! (lhsType.isUnsizedArray() || lhsType.isValidArrayOrVectorIndex (startIndex)))
                    s.startIndex->context.throwError (Errors::indexOutOfRange());

                if (s.isSlice)
                {
                    if (lhsType.isUnsizedArray())
                        s.startIndex->context.throwError (Errors::notYetImplemented ("Slices of dynamic arrays"));

                    if (! lhsType.getElementType().isPrimitive())
                        s.startIndex->context.throwError (Errors::notYetImplemented ("Slices of non-primitive arrays"));

                    if (s.endIndex != nullptr)
                    {
                        if (auto endIndexConst = s.endIndex->getAsConstant())
                        {
                            auto endIndex = TypeRules::checkAndGetArrayIndex (s.endIndex->context, endIndexConst->value);

                            if (! lhsType.isValidArrayOrVectorRange (startIndex, endIndex))
                                s.endIndex->context.throwError (Errors::illegalSliceSize());
                        }
                        else
                        {
                            s.endIndex->context.throwError (Errors::notYetImplemented ("Dynamic slice indexes"));
                        }
                    }
                }
            }
            else
            {
                if (s.isSlice)
                    s.startIndex->context.throwError (Errors::notYetImplemented ("Dynamic slice indexes"));

                throwErrorIfNotReadableValue (*s.startIndex);
                auto indexType = s.startIndex->getResultType();

                if (lhsType.isUnsizedArray())
                {
                    if (! (indexType.isInteger() || indexType.isBoundedInt()))
                        s.startIndex->context.throwError (Errors::nonIntegerArrayIndex());
                }
                else
                {
                    expectSilentCastPossible (s.startIndex->context,
                                              Type (PrimitiveType::int64), *s.startIndex);
                }
            }
        }

        void visit (AST::WriteToEndpoint& w) override
        {
            super::visit (w);

            throwErrorIfNotReadableValue (w.value);
            auto& topLevelWrite = ASTUtilities::getTopLevelWriteToEndpoint (w);

            // Either an OutputEndpointRef, or an ArrayElementRef of an OutputEndpointRef
            if (auto outputEndpoint = cast<AST::OutputEndpointRef> (topLevelWrite.target))
            {
                if (outputEndpoint->isResolved())
                    expectSilentCastPossible (w.context, outputEndpoint->output->getDetails().getSampleArrayTypes(), w.value);

                return;
            }

            if (auto arraySubscript = cast<AST::ArrayElementRef> (topLevelWrite.target))
            {
                if (auto outputEndpoint = cast<AST::OutputEndpointRef> (arraySubscript->object))
                {
                    if (outputEndpoint->isResolved())
                        expectSilentCastPossible (w.context, outputEndpoint->output->getDetails().getResolvedDataTypes(), w.value);

                    return;
                }
            }

            w.context.throwError (Errors::targetMustBeOutput());
            return;
        }

        void visit (AST::Annotation& a) override
        {
            super::visit (a);

            for (auto& property : a.properties)
                checkPropertyValue (property.value);
        }

        static void checkPropertyValue (AST::Expression& value)
        {
            if (! value.isCompileTimeConstant())
                value.context.throwError (Errors::propertyMustBeConstant());

            if (auto constValue = value.getAsConstant())
            {
                auto type = constValue->getResultType();

                if (! (type.isPrimitiveFloat() || type.isPrimitiveInteger()
                        || type.isPrimitiveBool() || type.isStringLiteral()))
                    value.context.throwError (Errors::illegalPropertyType());
            }
        }

        void checkArraySize (pool_ptr<AST::Expression> arraySize, int64_t maxSize)
        {
            if (arraySize != nullptr)
            {
                if (auto c = arraySize->getAsConstant())
                {
                    // Should only be an integer, and must be >= 1
                    if (c->getResultType().isInteger())
                    {
                        auto size = c->value.getAsInt64();

                        if (size < 1 || size > maxSize)
                            arraySize->context.throwError (Errors::illegalArraySize());
                    }
                    else
                    {
                        arraySize->context.throwError (Errors::nonIntegerArraySize());
                    }
                }
                else
                {
                    arraySize->context.throwError (Errors::nonConstArraySize());
                }
            }
        }
    };

    //==============================================================================
    struct PreAndPostIncOperatorCheck  : public ASTVisitor
    {
        using super = ASTVisitor;
        using VariableList = ArrayWithPreallocation<pool_ref<AST::VariableDeclaration>, 16>;
        VariableList* variablesModified = nullptr;
        VariableList* variablesReferenced = nullptr;
        bool isInsidePreIncOp = false;

        void visitObject (AST::Statement& s) override
        {
            VariableList modified, referenced;
            auto oldMod = variablesModified;
            auto oldRef = variablesReferenced;
            variablesModified = std::addressof (modified);
            variablesReferenced = std::addressof (referenced);
            super::visitObject (s);
            variablesModified = oldMod;
            variablesReferenced = oldRef;
        }

        void visit (AST::VariableRef& v) override
        {
            if (variablesModified != nullptr)
            {
                throwIfVariableFound (*variablesModified, v);
                variablesReferenced->push_back (v.variable);
            }

            super::visit (v);
        }

        void visit (AST::PreOrPostIncOrDec& p) override
        {
            if (auto v = cast<AST::VariableRef> (p.target))
            {
                SOUL_ASSERT (variablesModified != nullptr);

                throwIfVariableFound (*variablesReferenced, *v);
                variablesModified->push_back (v->variable);
                variablesReferenced->push_back (v->variable);
            }
            else
            {
                super::visit (p);
            }
        }

        void throwIfVariableFound (VariableList& list, AST::VariableRef& v)
        {
            if (contains (list, v.variable))
                v.context.throwError (Errors::preIncDecCollision());
        }
    };
};

} // namespace soul
