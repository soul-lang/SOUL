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
                auto firstAdvanceCall = findFirstAdvanceCall (*f);

                if (f->isRunFunction)
                {
                    if (firstAdvanceCall == nullptr)
                        f->location.throwError (Errors::runFunctionMustCallAdvance());
                }
                else
                {
                    if (firstAdvanceCall != nullptr)
                        firstAdvanceCall->location.throwError (Errors::advanceCannotBeCalledHere());
                }

                if (! f->isSystemInitFunction)
                {
                    visitFunctionCalls (*f, [] (heart::FunctionCall& call)
                    {
                        auto& target = *call.function;

                        if (target.isRunFunction || target.isUserInitFunction || target.isEventFunction)
                            target.location.throwError (Errors::cannotCallFunction (getFunctionName (target)));
                    });
                }

                if (f->isUserInitFunction)
                    if (auto w = findFirstWrite (*f))
                        w->location.throwError (Errors::streamsCannotBeUsedDuringInit());

                if (! f->isRunFunction)
                    if (auto w = findFirstStreamWrite (*f))
                        w->location.throwError (Errors::streamsCanOnlyBeUsedInRun());
            }
        }
    }

    //==============================================================================
    template <typename Visitor>
    static void visitFunctionCalls (heart::Function& f, Visitor&& visit)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto call = cast<heart::FunctionCall> (*s))
                    visit (*call);
    }

    static heart::WriteStreamPtr findFirstWrite (heart::Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto w = cast<heart::WriteStream> (*s))
                    return w;

        return {};
    }

    static heart::WriteStreamPtr findFirstStreamWrite (heart::Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto w = cast<heart::WriteStream> (*s))
                    if (w->target != nullptr && w->target->isStreamEndpoint())
                        return w;

        return {};
    }

    static heart::AdvanceClockPtr findFirstAdvanceCall (heart::Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto a = cast<heart::AdvanceClock> (*s))
                    return a;

        return {};
    }


    static void checkForInfiniteLoops (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                if (CallFlowGraph::doesFunctionContainInfiniteLoops (*f))
                    f->location.throwError (Errors::functionContainsAnInfiniteLoop (getFunctionName (*f)));
    }

    static std::string getFunctionName (const heart::Function& f)
    {
        auto name = f.name.toString();

        if (startsWith (name, "_"))
        {
            auto i = name.find ("_specialised_");

            if (i != std::string::npos && i > 0)
                return name.substr (1, i - 1);
        }

        return name;
    }

    static void checkForRecursiveFunctions (Program& program)
    {
        auto recursiveCallSequence = CallFlowGraph::findRecursiveFunctionCallSequences (program);

        if (! recursiveCallSequence.empty())
        {
            std::vector<std::string> functionNames;

            for (auto& fn : recursiveCallSequence)
                functionNames.push_back (quoteName (getFunctionName (*fn)));

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
