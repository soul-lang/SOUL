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
    Converts SOUL AST -> HEART AST
*/
struct HEARTGenerator  : public ASTVisitor
{
    static void build (ArrayView<pool_ref<AST::ModuleBase>> sourceModules,
                       ArrayView<pool_ref<Module>> targetModules,
                       uint32_t maxNestedExpressionDepth = 255)
    {
        for (auto& m : sourceModules)
            SanityCheckPass::runPreHEARTGenChecks (m);

        std::vector<HEARTGenerator> generators;

        for (size_t i = 0; i < sourceModules.size(); ++i)
            generators.push_back ({ sourceModules[i], targetModules[i], maxNestedExpressionDepth });

        for (size_t i = 0; i < sourceModules.size(); ++i)
            generators[i].visitObject (sourceModules[i]);
    }

private:
    using super = ASTVisitor;

    HEARTGenerator (AST::ModuleBase& source, Module& targetModule, uint32_t maxDepth)
        : module (targetModule), builder (targetModule), maxExpressionDepth (maxDepth)
    {
        auto path = source.getFullyQualifiedPath();
        module.shortName = path.getLastPart().toString();
        module.fullName = path.toString();
        module.originalFullName = getOriginalModulePath (path);

        if (auto fns = source.getFunctionList())
        {
            for (auto& f : *fns)
            {
                if (! f->isGeneric())
                {
                    auto name = getFunctionName (f);
                    auto& af = module.functions.add (name, false);
                    f->generatedFunction = af;
                }
            }
        }

        auto& vars = source.getStateVariableList();

        for (auto& v : vars)
            if (v->isExternal)
                addExternalVariable (v);
    }

    pool_ptr<const AST::Graph> sourceGraph;
    pool_ptr<const AST::Processor> sourceProcessor;
    Module& module;

    uint32_t loopIndex = 0, ifIndex = 0;
    bool parsingStateVariables = false;

    FunctionBuilder builder;
    pool_ptr<heart::Variable> currentTargetVariable;
    uint32_t expressionDepth = 0;
    const uint32_t maxExpressionDepth;
    pool_ptr<heart::Block> breakTarget, continueTarget;

    //==============================================================================
    Identifier convertIdentifier (Identifier i)
    {
        return module.allocator.get (i);
    }

    static std::string getOriginalModulePath (IdentifierPath path)
    {
        SOUL_ASSERT (path.getFirstPart().toString() == Program::getRootNamespaceName());
        path = path.fromSecondPart();
        removeIf (path.pathSections, [] (const Identifier& section) { return choc::text::startsWith (section, "_for"); });
        return path.toString();
    }

    heart::Variable& createVariableDeclaration (AST::VariableDeclaration& v,
                                                heart::Variable::Role role,
                                                bool canBeReference)
    {
        auto& av = module.allocate<heart::Variable> (v.context.location,
                                                     canBeReference ? v.getType() : v.getType().removeReferenceIfPresent(),
                                                     convertIdentifier (v.name), role);
        v.generatedVariable = av;

        if (role == heart::Variable::Role::state && v.initialValue != nullptr)
            av.initialValue = evaluateAsConstantExpression (*v.initialValue);

        av.annotation = v.annotation.toPlainAnnotation (module.program.getStringDictionary());
        return av;
    }

    heart::Variable& addExternalVariable (AST::VariableDeclaration& v)
    {
        SOUL_ASSERT (v.isExternal);
        auto& hv = createVariableDeclaration (v, heart::Variable::Role::external, false);
        module.stateVariables.add (hv);
        return hv;
    }

    void addBranchIf (AST::Expression& condition, heart::Block& trueBranch,
                      heart::Block& falseBranch, pool_ptr<heart::Block> subsequentBranch)
    {
        builder.addBranchIf (evaluateAsExpression (condition, PrimitiveType::bool_),
                             trueBranch, falseBranch, subsequentBranch);
    }

    void visitWithDestination (pool_ptr<heart::Variable> destVar, AST::Statement& s)
    {
        auto oldTarget = currentTargetVariable;
        auto oldDepth = expressionDepth;
        currentTargetVariable = destVar;
        expressionDepth = 0;
        visitObject (s);
        currentTargetVariable = oldTarget;
        expressionDepth = oldDepth;
    }

    void visitAsStatement (AST::Statement& s)
    {
        visitWithDestination (nullptr, s);
    }

    void visitAsStatement (pool_ptr<AST::Statement> s)
    {
        if (s != nullptr)
            visitAsStatement (*s);
    }

    //==============================================================================
    void visit (AST::Processor& p) override
    {
        sourceProcessor = p;

        if (p.latency != nullptr)
            module.latency = SanityCheckPass::checkLatency (*p.latency);

        generateStructs (p.structures);

        module.annotation = p.annotation.toPlainAnnotation (module.program.getStringDictionary());

        parsingStateVariables = true;
        super::visit (p);
        parsingStateVariables = false;

        generateFunctions (p.functions);
    }

    void visit (AST::Graph& g) override
    {
        module.annotation = g.annotation.toPlainAnnotation (module.program.getStringDictionary());
        sourceGraph = g;

        parsingStateVariables = true;
        super::visit (g);
        parsingStateVariables = false;
    }

    void visit (AST::Namespace& n) override
    {
        generateStructs (n.structures);
        for (auto& f : n.functions)   visitObject (f);
        for (auto& s : n.structures)  visitObject (s);
        for (auto& u : n.usings)      visitObject (u);

        generateFunctions (n.functions);
    }

    //==============================================================================
    void visit (AST::EndpointDeclaration& e) override
    {
        const auto& details = e.getDetails();

        if (e.isInput)
        {
            auto& i = module.allocate<heart::InputDeclaration> (e.context.location);
            i.name = convertIdentifier (e.name);
            i.index = (uint32_t) module.inputs.size();
            i.endpointType = details.endpointType;
            i.dataTypes = details.getResolvedDataTypes();
            i.annotation = e.annotation.toPlainAnnotation (module.program.getStringDictionary());
            i.arraySize = getProcessorArraySize (details.arraySize);
            e.generatedInput = i;

            SOUL_ASSERT (module.findOutput (e.name) == nullptr);
            SOUL_ASSERT (module.findInput (e.name) == nullptr);

            module.inputs.push_back (i);
        }
        else
        {
            auto& o = module.allocate<heart::OutputDeclaration> (e.context.location);
            o.name = convertIdentifier (e.name);
            o.index = (uint32_t) module.outputs.size();
            o.endpointType = details.endpointType;
            o.dataTypes = details.getResolvedDataTypes();
            o.annotation = e.annotation.toPlainAnnotation (module.program.getStringDictionary());
            o.arraySize = getProcessorArraySize (details.arraySize);
            e.generatedOutput = o;

            SOUL_ASSERT (module.findOutput (e.name) == nullptr);
            SOUL_ASSERT (module.findInput (e.name) == nullptr);

            module.outputs.push_back (o);
        }
    }

    void visit (AST::Connection& conn) override
    {
        auto& c = module.allocate<heart::Connection> (conn.context.location);
        module.connections.push_back (c);

        c.source.processor      = getOrAddProcessorInstance (conn.getSourceProcessor());
        c.dest.processor        = getOrAddProcessorInstance (conn.getDestProcessor());
        c.source.endpointName   = conn.getSourceEndpointName();
        c.source.endpointIndex  = conn.getSourceEndpointIndex();
        c.dest.endpointName     = conn.getDestEndpointName();
        c.dest.endpointIndex    = conn.getDestEndpointIndex();
        c.interpolationType     = conn.interpolationType;
        c.delayLength           = getDelayLength (conn.delayLength);
    }

    static std::optional<size_t> getEndpointIndex (pool_ptr<AST::Expression> index)
    {
        if (index == nullptr)
            return {};

        if (auto c = index->getAsConstant())
            return c->value.getAsInt64();

        index->context.throwError (Errors::endpointIndexMustBeConstant());
    }

    static std::optional<int64_t> getDelayLength (pool_ptr<AST::Expression> delay)
    {
        if (delay == nullptr)
            return {};

        if (auto c = delay->getAsConstant())
            return SanityCheckPass::checkDelayLineLength (c->context, c->value);

        delay->context.throwError (Errors::delayLineMustBeConstant());
    }

    static std::optional<uint32_t> getProcessorArraySize (pool_ptr<AST::Expression> size)
    {
        if (size != nullptr)
        {
            if (auto c = size->getAsConstant())
            {
                if (c->value.getType().isPrimitiveInteger())
                {
                    auto value = c->value.getAsInt64();

                    if (value < 1 || value > (int64_t) AST::maxProcessorArraySize)
                        size->context.throwError (Errors::illegalArraySize());

                    return (uint32_t) value;
                }

                size->context.throwError (Errors::expectedInteger());
            }

            size->context.throwError (Errors::expectedConstant());
        }

        return {};
    }

    pool_ptr<heart::ProcessorInstance> getOrAddProcessorInstance (pool_ptr<AST::ProcessorInstance> instance)
    {
        if (instance == nullptr)
            return {};

        auto instanceName = instance->instanceName->toString();

        for (auto i : module.processorInstances)
            if (instanceName == i->instanceName)
                return i;

        for (auto& i : sourceGraph->processorInstances)
        {
            if (i->instanceName->toString() == instanceName)
            {
                auto& targetProcessor = sourceGraph->findSingleMatchingProcessor (i);

                auto& p = module.allocate<heart::ProcessorInstance> (CodeLocation{});
                p.instanceName = instanceName;
                p.sourceName = targetProcessor.getFullyQualifiedPath().toString();
                p.arraySize = getProcessorArraySize (i->arraySize).value_or (1);

                if (i->clockMultiplierRatio != nullptr)
                {
                    if (auto c = i->clockMultiplierRatio->getAsConstant())
                        p.clockMultiplier.setMultiplier (i->clockMultiplierRatio->context, c->value);
                    else
                        i->clockMultiplierRatio->context.throwError (Errors::ratioMustBeInteger());
                }

                if (i->clockDividerRatio != nullptr)
                {
                    if (auto c = i->clockDividerRatio->getAsConstant())
                        p.clockMultiplier.setDivider (i->clockDividerRatio->context, c->value);
                    else
                        i->clockDividerRatio->context.throwError (Errors::ratioMustBeInteger());
                }

                SOUL_ASSERT (i->specialisationArgs == nullptr);

                module.processorInstances.push_back (p);
                return p;
            }
        }

        return {};
    }

    void visit (AST::Function& f) override
    {
        if (! f.isGeneric())
        {
            auto& af = f.getGeneratedFunction();

            if (f.isIntrinsic())                af.functionType = heart::FunctionType::intrinsic();
            else if (f.isEventFunction())       af.functionType = heart::FunctionType::event();
            else if (f.isUserInitFunction())    af.functionType = heart::FunctionType::userInit();
            else if (f.isSystemInitFunction())  af.functionType = heart::FunctionType::systemInit();

            af.intrinsicType = f.intrinsic;
            af.annotation = f.annotation.toPlainAnnotation (module.program.getStringDictionary());
            af.location = f.context.location;
        }
    }

    Identifier getFunctionName (const AST::Function& f)
    {
        auto nameRoot = f.name.toString();

        if (f.isEventFunction())
        {
            auto name = heart::getEventFunctionName (nameRoot, f.parameters[0]->getType());
            SOUL_ASSERT (module.functions.find (name) == nullptr);
            return module.allocator.get (name);
        }

        return module.allocator.get (addSuffixToMakeUnique (nameRoot,
                                                            [this] (const std::string& name)
                                                            {
                                                                return module.functions.find (name) != nullptr;
                                                            }));
    }

    void generateStructs (ArrayView<pool_ref<AST::StructDeclaration>> structs)
    {
        for (auto& s : structs)
            module.structs.add (s->getStruct());
    }

    void generateFunctions (ArrayView<pool_ref<AST::Function>> functions)
    {
        for (auto& f : functions)
            if (! f->isGeneric())
                generateFunction (f);
    }

    void generateFunction (AST::Function& f)
    {
        auto& af = f.getGeneratedFunction();
        af.returnType = f.returnType->resolveAsType();

        ifIndex = 0;
        loopIndex = 0;
        builder.beginFunction (af);

        for (auto p : f.parameters)
        {
            auto& v = createVariableDeclaration (p, heart::Variable::Role::parameter, true);

            if (af.functionType.isEvent() && v.getType().isNonConstReference())
                p->context.throwError (Errors::eventParamsCannotBeNonConstReference());

            builder.addParameter (v);
        }

        if (f.block != nullptr)
        {
            visitObject (*f.block);

            builder.endFunction();

            if (! builder.checkFunctionBlocksForTermination())
            {
                // This will fail if the function isn't void but some blocks terminate without returning a value,
                // however, we'll make sure they're not unreachable before flagging this as an error
                Optimisations::optimiseFunctionBlocks (af, module.allocator);

                if (! builder.checkFunctionBlocksForTermination())
                    f.context.throwError (Errors::notAllControlPathsReturnAValue (f.name));
            }
        }
        else
        {
            af.hasNoBody = true;
            builder.endFunction();
        }
    }

    void addStateVariableInitialisationCode()
    {
        for (auto& v : sourceProcessor->stateVariables)
        {
            if (v->generatedVariable != nullptr)
            {
                if (v->initialValue != nullptr)
                    visitWithDestination (v->generatedVariable, *v->initialValue);
                else if (! v->isExternal)
                    builder.addZeroAssignment (*v->generatedVariable);
            }
        }
    }

    void visit (AST::Block& b) override
    {
        if (b.isFunctionMainBlock())
            builder.beginBlock (builder.createNewBlock());

        for (auto& s : b.statements)
        {
            builder.ensureBlockIsReady();
            expressionDepth = 0;
            visitAsStatement (s.get());
        }
    }

    heart::Expression& getAsReference (AST::Expression& e, bool isConstRef)
    {
        if (auto v = cast<AST::VariableRef> (e))
        {
            if (v->variable->generatedVariable != nullptr)
                return v->variable->getGeneratedVariable();

            if (isConstRef)
                return evaluateAsConstantExpression (*v);
        }

        if (auto member = cast<AST::StructMemberRef> (e))
            return createStructSubElement (*member, getAsReference (member->object, isConstRef));

        if (auto subscript = cast<AST::ArrayElementRef> (e))
            return createArraySubElement (*subscript, getAsReference (*subscript->object, isConstRef));

        if (isConstRef)
            return getExpressionAsMutableLocalCopy (e);

        e.context.throwError (Errors::expressionNotAssignable());
    }

    void createAssignmentToCurrentTarget (AST::Expression& source)
    {
        if (currentTargetVariable != nullptr)
            createAssignment (*currentTargetVariable, source);
        else if (! source.isOutputEndpoint())
            source.context.throwError (Errors::unusedExpression());
    }

    void createAssignment (heart::Expression& destVar, AST::Expression& source)
    {
        builder.addAssignment (destVar, evaluateAsExpression (source, destVar.getType()));
    }

    heart::Expression& getExpressionAsConstLocalCopy (AST::Expression& e)
    {
        auto& local = builder.createRegisterVariable (e.getResultType().removeConstIfPresent());
        visitWithDestination (local, e);
        return local;
    }

    heart::Expression& getExpressionAsMutableLocalCopy (AST::Expression& e)
    {
        auto& local = builder.createMutableLocalVariable (e.getResultType().removeConstIfPresent());
        visitWithDestination (local, e);
        return local;
    }

    heart::Expression& evaluateAsConstantExpression (AST::Expression& e)
    {
        if (auto c = e.getAsConstant())
            return module.allocator.allocate<heart::Constant> (c->context.location, c->value);

        if (auto v = cast<AST::VariableRef> (e))
        {
            if (v->variable->isAssignable())
                if (v->variable->getParentScope()->findModule() != e.getParentScope()->findModule())
                    v->context.throwError (Errors::cannotReferenceOtherProcessorVar());

            if (auto a = v->variable->generatedVariable)
                return *a;

            if (auto initial = v->variable->initialValue)
                return evaluateAsConstantExpression (*initial);
        }

        if (auto op = cast<AST::BinaryOperator> (e))
        {
            auto operandType = op->getOperandType();

            // (putting these into locals to make sure we evaluate everything in left-to-right order)
            auto& lhs = builder.createCastIfNeeded (evaluateAsConstantExpression (op->lhs), operandType);
            auto& rhs = builder.createCastIfNeeded (evaluateAsConstantExpression (op->rhs), operandType);

            return builder.createCastIfNeeded (builder.createBinaryOp (op->context.location, lhs, rhs, op->operation),
                                               op->getResultType());
        }

        if (auto op = cast<AST::UnaryOperator> (e))
        {
            auto sourceType = op->getResultType();
            auto& source = builder.createCastIfNeeded (evaluateAsConstantExpression (op->source), sourceType);

            return builder.createUnaryOp (op->context.location, source, op->operation);
        }

        if (auto pp = cast<AST::ProcessorProperty> (e))
        {
            if (module.isNamespace())
                pp->context.throwError (Errors::processorPropertyUsedOutsideDecl());

            return module.allocator.allocate<heart::ProcessorProperty> (pp->context.location, pp->property);
        }

        if (auto c = cast<AST::TypeCast> (e))
        {
            if (auto csl = cast<AST::CommaSeparatedList> (c->source))
            {
                if (csl->isCompileTimeConstant()
                     && c->targetType.isFixedSizeAggregate()
                     && c->targetType.getNumAggregateElements() == csl->items.size())
                {
                    return createAggregateInitialiserList (c->context, c->targetType, *csl);
                }
            }

            auto& sourceExp = evaluateAsConstantExpression (c->source);
            const auto& sourceType = sourceExp.getType();

            SanityCheckPass::expectCastPossible (c->source->context, c->targetType, sourceType);
            return builder.createCastIfNeeded (sourceExp, c->targetType);
        }

        e.context.throwError (Errors::expectedConstant());
    }

    heart::Expression& evaluateAsExpression (AST::Expression& e)
    {
        if (++expressionDepth < maxExpressionDepth)
        {
            if (auto c = e.getAsConstant())
                return module.allocator.allocate<heart::Constant> (c->context.location, c->value);

            if (auto v = cast<AST::VariableRef> (e))
            {
                if (v->variable->isAssignable())
                    if (v->variable->getParentScope()->findModule() != e.getParentScope()->findModule())
                        v->context.throwError (Errors::cannotReferenceOtherProcessorVar());

                if (auto a = v->variable->generatedVariable)
                    return *a;

                if (auto initial = v->variable->initialValue)
                    return evaluateAsExpression (*initial);

                return builder.createZeroInitialiser (v->getResultType());
            }

            if (auto member = cast<AST::StructMemberRef> (e))
            {
                auto structType = getStructType (*member);

                auto& source = evaluateAsExpression (member->object, structType);
                return createStructSubElement (*member, source);
            }

            if (auto subscript = cast<AST::ArrayElementRef> (e))
            {
                auto arrayOrVectorType = getArrayOrVectorType (*subscript);
                auto& source = evaluateAsExpression (*subscript->object, arrayOrVectorType);
                return createArraySubElement (*subscript, source);
            }

            if (auto c = cast<AST::TypeCast> (e))
            {
                SOUL_ASSERT (c->getNumArguments() != 0);

                pool_ref<AST::Expression> source = c->source;

                if (auto list = cast<AST::CommaSeparatedList> (c->source))
                {
                    if (list->items.size() != 1)
                        return createAggregateWithInitialisers (*c);

                    source = list->items.front();
                }

                auto& sourceExp = evaluateAsExpression (source);
                const auto& sourceType = sourceExp.getType();

                if (TypeRules::canCastTo (c->targetType, sourceType))
                    return builder.createCastIfNeeded (sourceExp, c->targetType);

                if (c->targetType.isFixedSizeAggregate() && c->targetType.getNumAggregateElements() == 1)
                    return createAggregateWithInitialisers (*c);

                SanityCheckPass::expectCastPossible (c->source->context, c->targetType, sourceType);
            }

            if (auto op = cast<AST::BinaryOperator> (e))
            {
                auto operandType = op->getOperandType();

                // (putting these into locals to make sure we evaluate everything in left-to-right order)
                auto& lhs = builder.createCastIfNeeded (evaluateAsExpression (op->lhs), operandType);
                auto& rhs = builder.createCastIfNeeded (evaluateAsExpression (op->rhs), operandType);

                return builder.createCastIfNeeded (builder.createBinaryOp (op->context.location, lhs, rhs, op->operation),
                                                   op->getResultType());
            }

            if (auto op = cast<AST::UnaryOperator> (e))
            {
                auto sourceType = op->getResultType();
                auto& source = builder.createCastIfNeeded (evaluateAsExpression (op->source), sourceType);
                return builder.createUnaryOp (op->context.location, source, op->operation);
            }

            if (auto pp = cast<AST::ProcessorProperty> (e))
            {
                if (module.isNamespace())
                    pp->context.throwError (Errors::processorPropertyUsedOutsideDecl());

                return module.allocator.allocate<heart::ProcessorProperty> (pp->context.location, pp->property);
            }
        }

        return getExpressionAsConstLocalCopy (e);
    }

    heart::Expression& evaluateAsExpression (AST::Expression& e, const Type& targetType)
    {
        if (targetType.isReference() && ! targetType.isIdentical (e.getResultType()))
            return evaluateAsExpression (e, targetType.removeReference());

        if (auto list = cast<AST::CommaSeparatedList> (e))
        {
            if (targetType.isArrayOrVector() || targetType.isStruct())
            {
                auto& temp = builder.createMutableLocalVariable (targetType);
                initialiseArrayOrStructElements (temp, list->items, list->context);
                return temp;
            }

            SOUL_ASSERT_FALSE;
        }

        auto& resolved = evaluateAsExpression (e);
        const auto& resolvedType = resolved.getType();

        if (resolvedType.isIdentical (targetType))
            return resolved;

        if (targetType.isReference() && ! resolved.isMutable())
            e.context.throwError (Errors::cannotPassConstAsNonConstRef());

        auto constValue = resolved.getAsConstant();

        if (constValue.isValid() && TypeRules::canSilentlyCastTo (targetType, constValue))
            return module.allocate<heart::Constant> (e.context.location, constValue.castToTypeExpectingSuccess (targetType));

        if (! TypeRules::canSilentlyCastTo (targetType, resolvedType))
            e.context.throwError (Errors::expectedExpressionOfType (targetType.getDescription()));

        return builder.createCastIfNeeded (resolved, targetType);
    }

    heart::AggregateInitialiserList& createAggregateInitialiserList (const AST::Context& context, const Type& targetType, AST::CommaSeparatedList& list)
    {
        auto& result = module.allocator.allocate<heart::AggregateInitialiserList> (context.location, targetType);
        uint32_t index = 0;

        for (auto& item : AST::CommaSeparatedList::getAsExpressionList (list))
        {
            auto elementType = targetType.isStruct() ? targetType.getStructRef().getMemberType (index)
                                                     : targetType.getElementType();

            SanityCheckPass::expectSilentCastPossible (item->context, elementType, item);
            result.items.push_back (evaluateAsExpression (item, elementType));
            ++index;
        }

        return result;
    }

    heart::StructElement& createStructSubElement (AST::StructMemberRef& member, heart::Expression& source)
    {
        return builder.createStructElement (source, member.memberName);
    }

    heart::ArrayElement& createArraySubElement (AST::ArrayElementRef& subscript, heart::Expression& source)
    {
        auto arrayOrVectorType = getArrayOrVectorType (subscript);

        if (subscript.isSlice)
        {
            if (arrayOrVectorType.isUnsizedArray())
                subscript.context.throwError (Errors::notYetImplemented ("Slices of dynamic arrays"));

            auto range = subscript.getResolvedSliceRange();
            SOUL_ASSERT (arrayOrVectorType.isValidArrayOrVectorRange (range.start, range.end));

            auto& result = builder.module.allocate<heart::ArrayElement> (subscript.context.location, source, range.start, range.end);
            result.suppressWrapWarning = subscript.suppressWrapWarning;
            result.isRangeTrusted = true;
            return result;
        }

        auto& index = evaluateAsExpression (*subscript.startIndex);
        auto& result = builder.module.allocate<heart::ArrayElement> (subscript.context.location, source, index);
        result.suppressWrapWarning = subscript.suppressWrapWarning;
        result.optimiseDynamicIndexIfPossible();
        return result;
    }

    void visit (AST::IfStatement& i) override
    {
        auto labelIndex = ifIndex++;

        auto& trueBlock   = builder.createBlock ("@if_", labelIndex);
        auto& falseBlock  = builder.createBlock ("@ifnot_", labelIndex);

        addBranchIf (i.condition, trueBlock, falseBlock, trueBlock);

        visitAsStatement (i.trueBranch.get());

        if (i.falseBranch != nullptr)
        {
            auto& endBlock = builder.createBlock ("@ifend_", labelIndex);
            builder.addBranch (endBlock, falseBlock);
            visitAsStatement (*i.falseBranch);
            builder.beginBlock (endBlock);
        }
        else
        {
            builder.beginBlock (falseBlock);
        }
    }

    void visit (AST::LoopStatement& l) override
    {
        auto labelIndex = loopIndex++;
        auto oldbreakTarget = breakTarget;
        auto oldcontinueTarget = continueTarget;
        auto& breakBlock    = builder.createBlock ("@break_", labelIndex);
        auto& continueBlock = builder.createBlock ("@cont_",  labelIndex);

        breakTarget = breakBlock;
        continueTarget = continueBlock;

        auto& startBlock = builder.createBlock ("@loop_", labelIndex);
        auto& bodyBlock  = builder.createBlock ("@body_", labelIndex);

        if (auto rangeLoopVar = l.rangeLoopInitialiser)
        {
            SOUL_ASSERT (l.iterator == nullptr && l.condition == nullptr);
            auto type = rangeLoopVar->getType();

            if (! type.isBoundedInt())
                rangeLoopVar->context.throwError (Errors::rangeBasedForMustBeWrapType());

            auto numIterations = type.getBoundedIntLimit();
            auto& counterVar = builder.createMutableLocalVariable (Type::getBoundedIntSizeType(), "$counter_" + std::to_string (labelIndex));

            if (auto init = rangeLoopVar->initialValue)
                builder.addAssignment (counterVar, builder.createCastIfNeeded (rangeLoopVar->getGeneratedVariable(), Type::getBoundedIntSizeType()));
            else
                builder.addZeroAssignment (counterVar);

            builder.beginBlock (startBlock);
            auto& isCounterInRange = builder.createBinaryOp (l.context.location, counterVar,
                                                             builder.createConstant (Value (numIterations)),
                                                             BinaryOp::Op::lessThan);
            builder.addBranchIf (isCounterInRange, bodyBlock, breakBlock, bodyBlock);

            builder.addCastOrAssignment (rangeLoopVar->getGeneratedVariable(), counterVar);
            visitAsStatement (l.body);
            builder.beginBlock (continueBlock);
            builder.incrementValue (counterVar);
        }
        else if (l.numIterations != nullptr)
        {
            SOUL_ASSERT (l.iterator == nullptr && l.condition == nullptr);
            auto indexType = l.numIterations->getResultType();

            if (! indexType.isPrimitiveInteger())
                l.numIterations->context.throwError (Errors::expectedInteger());

            if (indexType.isInteger64())
            {
                if (auto constNumIterations = l.numIterations->getAsConstant())
                {
                    auto num = constNumIterations->value.getAsInt64();

                    if (num <= 0x7fffffff)
                        indexType = PrimitiveType::int32;
                }
            }

            auto& counterVar = builder.createMutableLocalVariable (indexType, "$counter_" + std::to_string (labelIndex));
            builder.addAssignment (counterVar, builder.createCastIfNeeded (evaluateAsExpression (*l.numIterations), indexType));

            builder.beginBlock (startBlock);
            auto& isCounterInRange = builder.createBinaryOp (l.context.location, counterVar,
                                                             builder.createZeroInitialiser (indexType),
                                                             BinaryOp::Op::greaterThan);
            builder.addBranchIf (isCounterInRange, bodyBlock, breakBlock, bodyBlock);
            visitAsStatement (l.body);
            builder.beginBlock (continueBlock);
            builder.decrementValue (counterVar);
        }
        else
        {
            builder.beginBlock (startBlock);

            if (l.condition == nullptr)
                builder.addBranch (bodyBlock, bodyBlock);
            else if (auto c = l.condition->getAsConstant())
                builder.addBranch (c->value.getAsBool() ? bodyBlock : breakBlock, bodyBlock);
            else
                addBranchIf (*l.condition, bodyBlock, breakBlock, bodyBlock);

            visitAsStatement (l.body);
            builder.beginBlock (continueBlock);
            visitAsStatement (l.iterator);
        }

        builder.addBranch (startBlock, breakBlock);
        breakTarget = oldbreakTarget;
        continueTarget = oldcontinueTarget;
    }

    void visit (AST::ReturnStatement& r) override
    {
        if (r.returnValue != nullptr)
            builder.addReturn (evaluateAsExpression (*r.returnValue, builder.currentFunction->returnType));
        else
            builder.addReturn();
    }

    void visit (AST::BreakStatement&) override
    {
        builder.addBranch (*breakTarget, builder.createNewBlock());
    }

    void visit (AST::ContinueStatement&) override
    {
        builder.addBranch (*continueTarget, builder.createNewBlock());
    }

    void visit (AST::TernaryOp& t) override
    {
        if (currentTargetVariable == nullptr)
            t.context.throwError (Errors::ternaryCannotBeStatement());

        auto labelIndex = ifIndex++;
        auto& targetVar = *currentTargetVariable;

        auto& trueBlock   = builder.createBlock ("@ternary_true_", labelIndex);
        auto& falseBlock  = builder.createBlock ("@ternary_false_", labelIndex);
        auto& endBlock    = builder.createBlock ("@ternary_end_", labelIndex);

        auto& paramVar = module.allocate<heart::Variable> (t.context.location,
                                                           targetVar.getType().removeReferenceIfPresent(),
                                                           module.allocator.get ("$_T" + std::to_string (labelIndex)),
                                                           heart::Variable::Role::parameter);

        endBlock.addParameter (paramVar);

        auto resultType  = t.getResultType();

        addBranchIf (t.condition, trueBlock, falseBlock, trueBlock);
        auto& trueVariable = builder.createRegisterVariable (evaluateAsExpression (t.trueBranch));
        builder.addBranch (endBlock, { trueVariable }, falseBlock);
        auto& falseVariable = builder.createRegisterVariable (evaluateAsExpression (t.falseBranch));
        builder.addBranch (endBlock, { falseVariable }, endBlock);
        builder.addAssignment (targetVar, paramVar);
    }

    void visit (AST::Constant& o) override
    {
        if (currentTargetVariable != nullptr)
            builder.addAssignment (*currentTargetVariable, o.value.castToTypeWithError (currentTargetVariable->getType(), o.context));
    }

    void visit (AST::VariableDeclaration& v) override
    {
        if (sourceGraph != nullptr)
            return;

        if (parsingStateVariables)
        {
            if (! v.isExternal)
            {
                const auto& type = v.getType();

                // Skip writing constant or unwritten-to variables to the state
                if (! (v.numWrites == 0 && (type.isPrimitive() || type.isBoundedInt())))
                    module.stateVariables.add (createVariableDeclaration (v, heart::Variable::Role::state, false));
            }
        }
        else
        {
            auto& target = createVariableDeclaration (v, heart::Variable::Role::mutableLocal, false);

            if (v.initialValue != nullptr)
                visitWithDestination (target, *v.initialValue);
            else
                builder.addZeroAssignment (target);
        }
    }

    void visit (AST::VariableRef& v) override
    {
        if (currentTargetVariable != nullptr)
            builder.addCastOrAssignment (*currentTargetVariable, v.variable->getGeneratedVariable());
    }

    void createFunctionCall (const AST::FunctionCall& call, pool_ptr<heart::Variable> targetVariable)
    {
        auto numArgs = call.getNumArguments();
        SOUL_ASSERT (call.targetFunction.generatedFunction != nullptr);
        SOUL_ASSERT (call.targetFunction.parameters.size() == numArgs);

        auto& fc = module.allocate<heart::FunctionCall> (call.context.location, targetVariable,
                                                         call.targetFunction.generatedFunction);

        for (size_t i = 0; i < numArgs; ++i)
        {
            auto paramType = call.targetFunction.parameters[i]->getType();
            auto& arg = call.arguments->items[i].get();

            if (paramType.isReference())
                fc.arguments.push_back (getAsReference (arg, paramType.isConst()));
            else
                fc.arguments.push_back (evaluateAsExpression (arg, paramType));
        }

        builder.addStatement (fc);
    }

    void visit (AST::FunctionCall& call) override
    {
        if (currentTargetVariable != nullptr)
        {
            auto returnType = call.getResultType();
            auto targetType = currentTargetVariable->getType();

            if (! returnType.isIdentical (targetType))
            {
                auto& temp = builder.createRegisterVariable (returnType);
                createFunctionCall (call, temp);
                builder.addAssignment (*currentTargetVariable, builder.createCast (call.context.location, temp, targetType));
                return;
            }
        }

        createFunctionCall (call, currentTargetVariable);
    }

    void visit (AST::TypeCast& c) override
    {
        if (c.getNumArguments() > 1)
            if (currentTargetVariable != nullptr && currentTargetVariable->isMutable())
                return initialiseArrayOrStructElements (*currentTargetVariable, c);

        createAssignmentToCurrentTarget (c);
    }

    void initialiseArrayOrStructElements (heart::Expression& target, ArrayView<pool_ref<AST::Expression>> list,
                                          const AST::Context& errorLocation)
    {
        const auto& targetType = target.getType();
        SOUL_ASSERT (targetType.isFixedSizeAggregate());
        SanityCheckPass::throwErrorIfWrongNumberOfElements (errorLocation, targetType, list.size());
        bool isStruct = target.getType().isStruct();

        builder.addZeroAssignment (target);

        for (size_t i = 0; i < list.size(); ++i)
        {
            auto& sourceValue = list[i].get();

            if (auto constElement = sourceValue.getAsConstant())
                if (constElement->value.isZero()) // no need to assign to elements which are zero
                    continue;

            createAssignment (isStruct ? (heart::Expression&) builder.createStructElement (target, target.getType().getStructRef().getMemberName (i))
                                       : (heart::Expression&) builder.createFixedArrayElement (target, i),
                              sourceValue);
        }
    }

    void initialiseArrayOrStructElements (heart::Expression& target, const AST::TypeCast& tc)
    {
        SOUL_ASSERT (target.isMutable());

        if (auto list = cast<AST::CommaSeparatedList> (tc.source))
            initialiseArrayOrStructElements (target, list->items, tc.source->context);
        else
            initialiseArrayOrStructElements (target, { std::addressof (tc.source), 1u }, tc.source->context);
    }

    heart::Variable& createAggregateWithInitialisers (const AST::TypeCast& tc)
    {
        auto& temp = builder.createMutableLocalVariable (tc.targetType);
        initialiseArrayOrStructElements (temp, tc);
        return temp;
    }

    void visit (AST::UnaryOperator& op) override
    {
        createAssignmentToCurrentTarget (op);
    }

    void visit (AST::BinaryOperator& op) override
    {
        createAssignmentToCurrentTarget (op);
    }

    void visit (AST::Assignment& o) override
    {
        createAssignment (getAsReference (o.target, false), o.newValue);
    }

    void visit (AST::ArrayElementRef& a) override
    {
        auto arrayOrVectorType = getArrayOrVectorType (a);
        auto& source = evaluateAsExpression (*a.object, arrayOrVectorType);

        if (a.isSlice)
        {
            auto sliceRange = a.getResolvedSliceRange();

            if (currentTargetVariable != nullptr)
                builder.addCastOrAssignment (*currentTargetVariable,
                                             builder.createFixedArraySlice (a.context.location,
                                                                            source, sliceRange.start, sliceRange.end));
            return;
        }

        auto& index = evaluateAsExpression (*a.startIndex);

        if (currentTargetVariable != nullptr)
            builder.addCastOrAssignment (*currentTargetVariable,
                                         builder.createDynamicSubElement (a.context.location, source, index,
                                                                          false, a.suppressWrapWarning));
    }

    void visit (AST::StructMemberRef& a) override
    {
        auto structType = getStructType (a);
        auto& source = evaluateAsExpression (a.object, structType);

        if (currentTargetVariable != nullptr)
            builder.addCastOrAssignment (*currentTargetVariable, builder.createStructElement (source, a.memberName));
    }

    void visit (AST::PreOrPostIncOrDec& p) override
    {
        auto resultDestVar = currentTargetVariable;
        auto op = p.isIncrement ? BinaryOp::Op::add
                                : BinaryOp::Op::subtract;

        auto& dest = getAsReference (p.target, false);
        auto type = dest.getType().removeReferenceIfPresent();

        auto& oldValue = builder.createRegisterVariable (type);
        builder.addAssignment (oldValue, dest);
        auto& one = module.allocator.allocate<heart::Constant> (p.context.location, Value::createInt32 (1).castToTypeExpectingSuccess (type));
        auto& incrementedValue = builder.createBinaryOp (p.context.location, oldValue, one, op);

        if (resultDestVar == nullptr)
        {
            builder.addAssignment (dest, incrementedValue);
        }
        else if (p.isPost)
        {
            builder.addAssignment (dest, incrementedValue);
            builder.addAssignment (*resultDestVar, oldValue);
        }
        else
        {
            builder.addAssignment (*resultDestVar, incrementedValue);
            builder.addAssignment (dest, *resultDestVar);
        }
    }

    void visit (AST::AdvanceClock& a) override
    {
        builder.addAdvance (a.context.location);
    }

    void createSeriesOfWrites (AST::Expression& target, ArrayView<pool_ref<AST::Expression>> values)
    {
        // Two choices - the target can be an output declaration, or an element of an output declaration
        if (auto output = cast<AST::OutputEndpointRef> (target))
        {
            for (auto v : values)
            {
                const auto& details = output->output->getDetails();

                if (! details.supportsDataType (v))
                    target.context.throwError (Errors::cannotWriteTypeToEndpoint (v->getResultType().getDescription(),
                                                                                  details.getTypesDescription()));

                auto sampleType = details.getDataType (v);

                builder.addWriteStream (output->context.location,
                                        *output->output->generatedOutput, nullptr,
                                        evaluateAsExpression (v, sampleType));
            }

            return;
        }

        if (auto arraySubscript = cast<AST::ArrayElementRef> (target))
        {
            if (auto outputRef = cast<AST::OutputEndpointRef> (arraySubscript->object))
            {
                const auto& details = outputRef->output->getDetails();

                if (details.arraySize == nullptr)
                    arraySubscript->context.throwError (Errors::cannotUseBracketsOnNonArrayEndpoint());

                for (auto v : values)
                {
                    // Find the element type that our expression will write to
                    auto sampleType = details.getElementDataType (v);
                    auto& value = evaluateAsExpression (v, sampleType);

                    if (arraySubscript->isSlice)
                    {
                        auto slice = arraySubscript->getResolvedSliceRange();

                        for (auto i = slice.start; i < slice.end; ++i)
                            builder.addWriteStream (outputRef->output->context.location,
                                                    *outputRef->output->generatedOutput,
                                                    builder.createConstantInt32 (i), value);
                    }
                    else
                    {
                        auto& index = evaluateAsExpression (*arraySubscript->startIndex);
                        auto& context = arraySubscript->startIndex->context;
                        auto constIndex = index.getAsConstant();
                        auto arraySize = outputRef->output->generatedOutput->arraySize.value_or (1);

                        if (constIndex.isValid())
                        {
                            auto fixedIndex = TypeRules::checkAndGetArrayIndex (context, constIndex);
                            TypeRules::checkConstantArrayIndex (context, fixedIndex, (Type::ArraySize) arraySize);

                            builder.addWriteStream (outputRef->output->context.location,
                                                    *outputRef->output->generatedOutput,
                                                    builder.createConstantInt32 (fixedIndex), value);
                        }
                        else
                        {
                            auto indexType = Type::createWrappedInt ((Type::BoundedIntSize) arraySize);
                            auto& wrappedIndex = builder.createCast (context.location, index, indexType);

                            builder.addWriteStream (outputRef->output->context.location,
                                                    *outputRef->output->generatedOutput, wrappedIndex, value);
                        }
                    }
                }

                return;
            }
        }

        target.context.throwError (Errors::targetMustBeOutput());
    }

    static AST::WriteToEndpoint& getTopLevelWriteToEndpoint (AST::WriteToEndpoint& ws, ArrayWithPreallocation<pool_ref<AST::Expression>, 4>& values)
    {
        values.insert (values.begin(), ws.value);

        if (auto chainedWrite = cast<AST::WriteToEndpoint> (ws.target))
            return getTopLevelWriteToEndpoint (*chainedWrite, values);

        return ws;
    }

    void visit (AST::WriteToEndpoint& ws) override
    {
        ArrayWithPreallocation<pool_ref<AST::Expression>, 4> values;
        auto& topLevelWrite = getTopLevelWriteToEndpoint (ws, values);
        createSeriesOfWrites (topLevelWrite.target, values);
    }

    void visit (AST::OutputEndpointRef& o) override
    {
        o.context.throwError (Errors::cannotReadFromOutput());
    }

    void visit (AST::InputEndpointRef& i) override
    {
        if (currentTargetVariable != nullptr)
            builder.addReadStream (i.context.location, *currentTargetVariable, *i.input->generatedInput);
        else
            i.context.throwError (Errors::unusedExpression());
    }

    void visit (AST::ProcessorProperty& p) override
    {
        createAssignmentToCurrentTarget (p);
    }

    void visit (AST::QualifiedIdentifier&) override
    {
        SOUL_ASSERT_FALSE;
    }

    void visit (AST::UnqualifiedName&) override
    {
        SOUL_ASSERT_FALSE;
    }

    Type getStructType (AST::StructMemberRef& a)
    {
        auto structType = a.object->getResultType();

        if (! structType.isStruct())
            a.object->context.throwError (Errors::expectedStructForDotOperator());

        return structType;
    }

    Type getArrayOrVectorType (AST::ArrayElementRef& a)
    {
        auto arrayOrVectorType = a.object->getResultType();

        if (! arrayOrVectorType.isArrayOrVector())
            a.object->context.throwError (Errors::expectedArrayOrVectorForBracketOp());

        return arrayOrVectorType;
    }
};

} // namespace soul
