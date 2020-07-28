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

struct heart::Checker
{
    static void sanityCheck (Program& program)
    {
        program.getMainProcessorOrThrowError();
        sanityCheckInputsAndOutputs (program);
        sanityCheckAdvanceAndStreamCalls (program);
        checkConnections (program);
        checkForRecursiveFunctions (program);
        checkForInfiniteLoops (program);
        checkBlockParameters (program);
    }

    static void sanityCheckInputsAndOutputs (Program& program)
    {
        auto& mainProcessor = program.getMainProcessorOrThrowError();

        for (auto& input : mainProcessor.inputs)
        {
            if (input->arraySize.has_value())
                input->location.throwError (Errors::notYetImplemented ("top-level arrays of inputs"));

            if (input->dataTypes.size() != 1)
                input->location.throwError (Errors::onlyOneTypeInTopLevelInputs());
        }

        for (auto& output : mainProcessor.outputs)
        {
            if (output->arraySize.has_value())
                output->location.throwError (Errors::notYetImplemented ("top-level arrays of outputs"));
        }
    }

    static void checkConnections (Program& program)
    {
        for (auto& m : program.getModules())
        {
            if (m->isGraph())
            {
                for (auto& conn : m->connections)
                {
                    pool_ptr<heart::IODeclaration> sourceOutput, destInput;
                    size_t sourceInstanceArraySize = 1, destInstanceArraySize = 1;
                    auto sourceDescription = conn->sourceEndpoint;
                    auto destDescription   = conn->destEndpoint;

                    if (conn->sourceProcessor != nullptr)
                    {
                        auto sourceModule = program.getModuleWithName (conn->sourceProcessor->sourceName);

                        if (sourceModule == nullptr)
                            conn->location.throwError (Errors::cannotFindProcessor (conn->sourceProcessor->sourceName));

                        sourceOutput = sourceModule->findOutput (conn->sourceEndpoint);
                        sourceInstanceArraySize = conn->sourceEndpointIndex.has_value() ? conn->sourceProcessor->arraySize : 1;
                        sourceDescription = conn->sourceProcessor->instanceName + "." + sourceDescription;
                    }
                    else
                    {
                        sourceOutput = m->findInput (conn->sourceEndpoint);
                    }

                    if (conn->destProcessor != nullptr)
                    {
                        auto destModule = program.getModuleWithName (conn->destProcessor->sourceName);

                        if (destModule == nullptr)
                            conn->location.throwError (Errors::cannotFindProcessor (conn->destProcessor->sourceName));

                        destInput = destModule->findInput (conn->destEndpoint);
                        destInstanceArraySize = conn->destEndpointIndex.has_value() ? conn->destProcessor->arraySize : 1;
                        destDescription = conn->destProcessor->instanceName + "." + destDescription;
                    }
                    else
                    {
                        destInput = m->findOutput (conn->destEndpoint);
                    }

                    if (sourceOutput == nullptr)  conn->location.throwError (Errors::cannotFindSource (sourceDescription));
                    if (destInput == nullptr)     conn->location.throwError (Errors::cannotFindDestination (destDescription));

                    if (conn->sourceEndpointIndex && sourceOutput->arraySize <= conn->sourceEndpointIndex)
                        conn->location.throwError (Errors::sourceEndpointIndexOutOfRange());

                    if (conn->destEndpointIndex && destInput->arraySize <= conn->destEndpointIndex)
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


    static void sanityCheckAdvanceAndStreamCalls (Program& program)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions)
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
    static void checkForInfiniteLoops (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                if (CallFlowGraph::doesFunctionContainInfiniteLoops (f))
                    f->location.throwError (Errors::functionContainsAnInfiniteLoop (f->getReadableName()));
    }

    static void checkForRecursiveFunctions (Program& program)
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
            if (functionNames.size() >  2)  location.throwError (Errors::recursiveFunctionCallSequence (joinStrings (functionNames, ", ")));
        }
    }

    //==============================================================================
    static void checkBlockParameters (Program& program)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions)
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
                                f->location.throwError (Errors::blockParametersInvalid (b->name));
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


    static void testHEARTRoundTrip (const Program& program)
    {
        ignoreUnused (program);

       #if SOUL_ENABLE_ASSERTIONS && (SOUL_TEST_HEART_ROUNDTRIP || (SOUL_DEBUG && ! defined (SOUL_TEST_HEART_ROUNDTRIP)))
        auto dump = program.toHEART();
        SOUL_ASSERT (dump == program.clone().toHEART());
        SOUL_ASSERT (dump == heart::Parser::parse (CodeLocation::createFromString ("internal test dump", dump)).toHEART());
       #endif
    }
};

}
