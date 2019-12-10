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
        checkForRecursiveFunctions (program);
        checkForInfiniteLoops (program);
    }

    static void sanityCheckInputsAndOutputs (Program& program)
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

    static void sanityCheckAdvanceAndStreamCalls (Program& program)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions)
            {
                auto firstAdvanceCall = heart::Utilities::findFirstAdvanceCall (*f);

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
                    if (auto w = heart::Utilities::findFirstWrite (*f))
                        w->location.throwError (Errors::streamsCannotBeUsedDuringInit());
            }
        }
    }

    //==============================================================================
    static void checkForInfiniteLoops (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                if (CallFlowGraph::doesFunctionContainInfiniteLoops (*f))
                    f->location.throwError (Errors::functionContainsAnInfiniteLoop (f->getReadableName()));
    }

    static void checkForRecursiveFunctions (Program& program)
    {
        auto recursiveCallSequence = CallFlowGraph::findRecursiveFunctionCallSequences (program);

        if (! recursiveCallSequence.empty())
        {
            std::vector<std::string> functionNames;

            for (auto& fn : recursiveCallSequence)
                functionNames.push_back (quoteName (fn->getReadableName()));

            auto location = recursiveCallSequence.front()->location;

            if (functionNames.size() == 1)
                location.throwError (Errors::functionCallsItselfRecursively (functionNames.front()));

            if (functionNames.size() == 2)
                location.throwError (Errors::functionsCallEachOtherRecursively (functionNames[0], functionNames[1]));

            if (functionNames.size() > 2)
                location.throwError (Errors::recursiveFunctionCallSequence (joinStrings (functionNames, ", ")));
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
