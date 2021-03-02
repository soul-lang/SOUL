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

struct heart::Checker::Impl
{
    static void sanityCheck (const Program& program, const BuildSettings settings, bool isFlattened)
    {
        ignoreUnused (isFlattened);
        ignoreUnused (program.getMainProcessor());

        sanityCheckModules (program);
        sanityCheckAdvanceAndStreamCalls (program);
        checkConnections (program);
        checkForRecursiveFunctions (program, settings.maxStackSize);
        checkForInfiniteLoops (program);
        checkBlockParameters (program);
        checkForCyclesInGraphs (program);
        checkStreamOperations (program);

        if (! isFlattened)
        {
            checkFunctionReturnTypes (program);
        }
    }

    static void sanityCheckModules (const Program& program)
    {
        std::vector<std::string> moduleNames;

        for (auto& m : program.getModules())
        {
            if (! appendIfNotPresent (moduleNames, m->fullName))
                m->location.throwError (Errors::duplicateModule (m->fullName));

            {
                std::vector<std::string> ioNames;

                for (auto& i : m->inputs)
                {
                    auto& name = i->name.toString();

                    if (! appendIfNotPresent (ioNames, name))
                        i->location.throwError (Errors::nameInUse (name));

                    if (i->arraySize)
                    {
                        if (i->arraySize == 0 || i->arraySize > AST::maxProcessorArraySize)
                            i->location.throwError (Errors::illegalArraySize());
                    }
                }

                for (auto& o : m->outputs)
                {
                    auto& name = o->name.toString();

                    if (! appendIfNotPresent (ioNames, name))
                        o->location.throwError (Errors::nameInUse (name));

                    if (o->arraySize)
                    {
                        if (o->arraySize == 0 || o->arraySize > AST::maxProcessorArraySize)
                            o->location.throwError (Errors::illegalArraySize());
                    }
                }
            }

            if (m->isProcessor() || m->isGraph())
                if (m->outputs.empty())
                    m->location.throwError (Errors::processorNeedsAnOutput());

            {
                std::vector<std::string> processorInstanceNames;

                for (auto& processorInstance : m->processorInstances)
                    if (! appendIfNotPresent (processorInstanceNames, processorInstance->instanceName))
                        processorInstance->location.throwError (Errors::duplicateProcessor (processorInstance->instanceName));
            }
        }

        auto& mainProcessor = program.getMainProcessor();

        for (auto& input : mainProcessor.inputs)
        {
            if (input->arraySize.has_value())
                input->location.throwError (Errors::notYetImplemented ("top-level arrays of inputs"));

            if (input->dataTypes.size() != 1)
                input->location.throwError (Errors::onlyOneTypeInTopLevelInputs());
        }

        for (auto& output : mainProcessor.outputs)
            if (output->arraySize.has_value())
                output->location.throwError (Errors::notYetImplemented ("top-level arrays of outputs"));
    }

    static void checkConnections (const Program& program)
    {
        for (auto& m : program.getModules())
        {
            if (m->isGraph())
            {
                for (auto& conn : m->connections)
                {
                    pool_ptr<heart::IODeclaration> sourceOutput, destInput;
                    bool sourceWasAnInput = false, destWasAnOutput = false;
                    size_t sourceInstanceArraySize = 1, destInstanceArraySize = 1;
                    auto sourceDescription = conn->source.endpointName;
                    auto destDescription   = conn->dest.endpointName;

                    if (conn->delayLength)
                    {
                        if (conn->delayLength < 1)
                            conn->location.throwError (Errors::delayLineTooShort());

                        if (conn->delayLength > (int32_t) AST::maxDelayLineLength)
                            conn->location.throwError (Errors::delayLineTooLong());
                    }

                    if (auto sourceProcessor = conn->source.processor)
                    {
                        auto sourceModule = program.findModuleWithName (sourceProcessor->sourceName);

                        if (sourceModule == nullptr)
                            conn->location.throwError (Errors::cannotFindProcessor (sourceProcessor->sourceName));

                        sourceOutput = sourceModule->findOutput (conn->source.endpointName);
                        sourceInstanceArraySize = conn->source.endpointIndex.has_value() ? 1 : sourceProcessor->arraySize;
                        sourceDescription = sourceProcessor->instanceName + "." + sourceDescription;

                        if (sourceOutput == nullptr)
                            sourceWasAnInput = sourceModule->findInput (conn->source.endpointName) != nullptr;
                    }
                    else
                    {
                        sourceOutput = m->findInput (conn->source.endpointName);
                    }

                    if (auto destProcessor = conn->dest.processor)
                    {
                        auto destModule = program.findModuleWithName (destProcessor->sourceName);

                        if (destModule == nullptr)
                            conn->location.throwError (Errors::cannotFindProcessor (destProcessor->sourceName));

                        destInput = destModule->findInput (conn->dest.endpointName);
                        destInstanceArraySize = conn->dest.endpointIndex.has_value() ? 1 : destProcessor->arraySize;
                        destDescription = destProcessor->instanceName + "." + destDescription;

                        if (destInput == nullptr)
                            destWasAnOutput = destModule->findOutput (conn->dest.endpointName) != nullptr;
                    }
                    else
                    {
                        destInput = m->findOutput (conn->dest.endpointName);
                    }

                    if (sourceOutput == nullptr)
                        conn->location.throwError (sourceWasAnInput ? Errors::cannotConnectFromAnInput (sourceDescription, destDescription)
                                                                    : Errors::cannotFindSource (sourceDescription));

                    if (destInput == nullptr)
                        conn->location.throwError (destWasAnOutput ? Errors::cannotConnectToAnOutput (sourceDescription, destDescription)
                                                                   : Errors::cannotFindDestination (destDescription));

                    if (conn->source.endpointIndex && sourceOutput->arraySize <= conn->source.endpointIndex)
                        conn->location.throwError (Errors::sourceEndpointIndexOutOfRange());

                    if (conn->dest.endpointIndex && destInput->arraySize <= conn->dest.endpointIndex)
                        conn->location.throwError (Errors::destinationEndpointIndexOutOfRange());

                    if (sourceOutput->endpointType != destInput->endpointType)
                        conn->location.throwError (Errors::cannotConnect (sourceDescription, getEndpointTypeName (sourceOutput->endpointType),
                                                                          destDescription, getEndpointTypeName (destInput->endpointType)));

                    if (! areConnectionTypesCompatible (sourceOutput->isEventEndpoint(),
                                                        *sourceOutput,
                                                        sourceInstanceArraySize,
                                                        *destInput,
                                                        destInstanceArraySize))
                        conn->location.throwError (Errors::cannotConnect (sourceDescription, sourceOutput->getTypesDescription(),
                                                                          destDescription, destInput->getTypesDescription()));
                }
            }
        }
    }

    static bool areConnectionTypesCompatible (bool isEvent,
                                              const heart::IODeclaration& sourceOutput, size_t sourceInstanceArraySize,
                                              const heart::IODeclaration& destInput, size_t destInstanceArraySize)
    {
        // Different rules for different connection types
        if (isEvent)
        {
            auto sourceSize = sourceInstanceArraySize * sourceOutput.arraySize.value_or (1);
            auto destSize = destInstanceArraySize * destInput.arraySize.value_or (1);

            // Sizes do not match - 1->1, 1->N, N->1 and N->N are only supported sizes
            if (sourceSize != 1 && destSize != 1 && sourceSize != destSize)
                return false;

            // Now compare the underlying types, ignoring array sizes, at least 1 should match
            for (auto& sourceType : sourceOutput.dataTypes)
                for (auto& destType : destInput.dataTypes)
                    if (TypeRules::canSilentlyCastTo (destType, sourceType))
                        return true;

            return false;
        }

        auto sourceSampleType = sourceOutput.getFrameOrValueType();
        auto destSampleType = destInput.getFrameOrValueType();

        if (sourceSampleType.isEqual (destSampleType, Type::ignoreVectorSize1))
            return true;

        if (sourceSampleType.isArray() && sourceSampleType.getElementType().isEqual (destSampleType, Type::ignoreVectorSize1))
            return true;

        if (destSampleType.isArray() && destSampleType.getElementType().isEqual (sourceSampleType, Type::ignoreVectorSize1))
            return true;

        return false;
    }

    static void sanityCheckAdvanceAndStreamCalls (const Program& program)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions.get())
            {
                auto firstAdvanceCall = heart::Utilities::findFirstAdvanceCall (f);

                if (f->functionType.isRun() && firstAdvanceCall == nullptr)
                    f->location.throwError (Errors::runFunctionMustCallAdvance());

                if (firstAdvanceCall != nullptr && ! m->isProcessor())
                    firstAdvanceCall->location.throwError (Errors::advanceCannotBeCalledHere());

                if (! f->functionType.isSystemInit())
                {
                    f->visitStatements<heart::FunctionCall> ([] (heart::FunctionCall& call)
                    {
                        auto& target = *call.function;

                        if (target.functionType.isRun() || target.functionType.isUserInit() || target.functionType.isEvent())
                            target.location.throwError (Errors::cannotCallFunction (target.getReadableName()));
                    });
                }

                if (f->functionType.isUserInit())
                    if (auto rw = heart::Utilities::findFirstStreamAccess (f))
                        rw->location.throwError (Errors::streamsCannotBeUsedDuringInit());
            }
        }
    }

    //==============================================================================
    static void checkForInfiniteLoops (const Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& f : m->functions.get())
                if (CallFlowGraph::doesFunctionContainInfiniteLoops (f))
                    f->location.throwError (Errors::functionContainsAnInfiniteLoop (f->getReadableName()));
    }

    static void checkForRecursiveFunctions (const Program& program, uint64_t maxStackSize)
    {
        auto callSequenceCheckResult = CallFlowGraph::checkFunctionCallSequences (program);

        if (! callSequenceCheckResult.recursiveFunctionCallSequence.empty())
        {
            std::vector<std::string> functionNames;

            for (auto& fn : callSequenceCheckResult.recursiveFunctionCallSequence)
                functionNames.push_back (quoteName (fn->getReadableName()));

            auto location = callSequenceCheckResult.recursiveFunctionCallSequence.front()->location;

            if (functionNames.size() == 1)  location.throwError (Errors::functionCallsItselfRecursively (functionNames.front()));
            if (functionNames.size() == 2)  location.throwError (Errors::functionsCallEachOtherRecursively (functionNames[0], functionNames[1]));
            if (functionNames.size() >  2)  location.throwError (Errors::recursiveFunctionCallSequence (choc::text::joinStrings (functionNames, ", ")));
        }

        if (maxStackSize != 0 && callSequenceCheckResult.maximumStackSize > maxStackSize)
            CodeLocation().throwError (Errors::maximumStackSizeExceeded (choc::text::getByteSizeDescription (callSequenceCheckResult.maximumStackSize),
                                                                         choc::text::getByteSizeDescription (maxStackSize)));
    }

    static void checkStreamOperations (const Program& program)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions.get())
            {
                for (auto& b : f->blocks)
                {
                    for (auto s : b->statements)
                    {
                        if (auto r = cast<heart::ReadStream> (*s))
                        {
                            if (f->functionType.isUserInit())
                                s->location.throwError (Errors::streamsCannotBeUsedDuringInit());

                            if (! f->functionType.isRun())
                                s->location.throwError (Errors::streamsCanOnlyBeUsedInRun());

                            if (r->element)
                            {
                                if (! r->source->arraySize.has_value())
                                    s->location.throwError (Errors::endpointIndexInvalid());

                                auto constElement = r->element->getAsConstant();

                                if (constElement.isValid())
                                    TypeRules::checkAndGetArrayIndex (r->location, constElement,
                                                                      r->source->dataTypes.front().createArray (*r->source->arraySize));
                            }
                        }

                        if (auto w = cast<heart::WriteStream> (*s))
                        {
                            if (f->functionType.isUserInit())
                                s->location.throwError (Errors::streamsCannotBeUsedDuringInit());

                            if (! (f->functionType.isRun() || w->target->isEventEndpoint()))
                                s->location.throwError (Errors::streamsCanOnlyBeUsedInRun());

                            if (! w->element)
                            {
                                if (! w->target->canHandleType (w->value->getType()))
                                    s->location.throwError (Errors::wrongTypeForEndpoint());
                            }
                            else
                            {
                                if (! w->target->arraySize.has_value())
                                    s->location.throwError (Errors::endpointIndexInvalid());

                                if (! w->target->canHandleElementType (w->value->getType()))
                                    s->location.throwError (Errors::wrongTypeForEndpoint());

                                auto constElement = w->element->getAsConstant();

                                if (constElement.isValid())
                                    TypeRules::checkAndGetArrayIndex (w->location, constElement,
                                                                      w->target->dataTypes.front().createArray (*w->target->arraySize));
                            }
                        }
                    }
                }
            }
        }
    }

    static void checkFunctionReturnTypes (const Program& program)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions.get())
                if (f->returnType.isReference())
                    f->location.throwError (Errors::cannotReturnReferenceType());
        }
    }

    static void checkBlockParameters (const Program& program)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions.get())
            {
                if (! f->blocks.empty())
                {
                    if (! f->blocks[0]->parameters.empty())
                        f->location.throwError (Errors::functionBlockCantBeParameterised (f->blocks[0]->name));

                    for (auto& b : f->blocks)
                    {
                        for (auto& param : b->parameters)
                        {
                            auto& type = param->getType();

                            if (type.isReference() || type.isVoid())
                                param->location.throwError (Errors::blockParametersInvalid (b->name));
                        }

                        if (auto branch = cast<heart::Branch> (b->terminator))
                        {
                            if (branch->target->parameters.size() != branch->targetArgs.size())
                                f->location.throwError (Errors::branchInvalidParameters (b->name));

                            for (size_t n = 0; n < branch->targetArgs.size(); n++)
                            {
                                auto& argType = branch->targetArgs[n]->getType();
                                auto& parameterType = branch->target->parameters[n]->getType();

                                if (! TypeRules::canSilentlyCastTo (parameterType, argType))
                                    f->location.throwError (Errors::branchInvalidParameters (b->name));
                            }
                        }
                        else if (auto branchIf = cast<heart::BranchIf> (b->terminator))
                        {
                            if (! branchIf->targetArgs[0].empty() || ! branchIf->targetArgs[1].empty())
                                f->location.throwError (Errors::notYetImplemented ("BranchIf parameterised blocks"));
                        }
                    }
                }
            }
        }
    }

    static void checkForCyclesInGraphs (const Program& program)
    {
        for (auto& m : program.getModules())
            if (m->isGraph())
                heart::Utilities::CycleDetector (m).checkAndThrowErrorIfCycleFound();
    }

    static void testHEARTRoundTrip (const Program& program)
    {
        ignoreUnused (program);

       #if SOUL_ENABLE_ASSERTIONS && (SOUL_TEST_HEART_ROUNDTRIP || (SOUL_DEBUG && ! defined (SOUL_TEST_HEART_ROUNDTRIP)))
        auto dump = program.toHEART();
        SOUL_ASSERT (dump == program.clone().toHEART());
        auto roundTrip = heart::Parser::parse (CodeLocation::createFromString ("internal test dump", dump)).toHEART();
        SOUL_ASSERT (dump == roundTrip);
       #endif
    }
};

void heart::Checker::sanityCheck (const Program& program, const BuildSettings settings, bool isFlattened)
{
    heart::Checker::Impl::sanityCheck (program, settings, isFlattened);
}

void heart::Checker::testHEARTRoundTrip (const Program& program)
{
    heart::Checker::Impl::testHEARTRoundTrip (program);
}

}
