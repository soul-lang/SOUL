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
/** Various functions that involve tracing of execution paths through HEART blocks */
struct CallFlowGraph
{
    static void visitDownstreamBlocks (const heart::Function& f, heart::Block& start, std::function<bool(heart::Block&)> visitor)
    {
        resetVisitedFlags (f);
        visitDownstreamBlocks (start, visitor);
    }

    static void visitUpstreamBlocks (const heart::Function& f, heart::Block& start, std::function<bool(heart::Block&)> visitor)
    {
        resetVisitedFlags (f);
        visitUpstreamBlocks (start, visitor);
    }

    static std::vector<pool_ref<heart::Variable>> findVariablesBeingReadBeforeBeingWritten (const heart::Function& function)
    {
        return findUninitialisedVariableUse (function);
    }

    static bool doesFunctionContainInfiniteLoops (const heart::Function& f)
    {
        if (f.blocks.empty())
            return false;

        if (f.blocks.front()->terminator->isReturn())
            return false;

        bool hasFoundTerminator = false;

        visitDownstreamBlocks (f, f.blocks.front(), [&] (const heart::Block& b)
                               {
                                   if (! (b.terminator->isReturn() || heart::Utilities::doesBlockCallAdvance (b)))
                                       return true;

                                   hasFoundTerminator = true;
                                   return false;
                               });

        return ! hasFoundTerminator;
    }

    struct CallSequenceCheckResults
    {
        uint64_t maximumStackSize = 0;
        std::vector<pool_ref<heart::Function>> recursiveFunctionCallSequence;
    };

    /** Iterates all function call seqeunces to calculate stack usage and also to spot
        recursive call sequences.
    */
    static CallSequenceCheckResults checkFunctionCallSequences (const Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& f : m->functions.get())
                f->localVariableStackSize = 0;

        CallSequenceCheckResults results;

        for (auto& m : program.getModules())
            for (auto& f : m->functions.get())
                iterateCallSequences (f, results, nullptr, 0);

        return results;
    }

private:
    //==============================================================================
    static void resetVisitedFlags (const heart::Function& f)
    {
        for (auto& b : f.blocks)
            b->tempData.clear();
    }

    static bool visitDownstreamBlocks (heart::Block& start, std::function<bool(heart::Block&)> visitor)
    {
        heart::Block* front = &start;
        heart::Block* back = &start;

        for (;;)
        {
            for (auto b : front->terminator->getDestinationBlocks())
            {
                if (b->tempData.get<heart::Block*>() == nullptr)
                {
                    if (! visitor (b))
                        return false;

                    back->tempData.set (&b.get());
                    back = &b.get();
                }
            }

            if (front == back)
                break;

            front = front->tempData.get<heart::Block*>();
        }

        return true;
    }

    static bool visitUpstreamBlocks (heart::Block& start, std::function<bool(heart::Block&)> visitor)
    {
        heart::Block* front = &start;
        heart::Block* back = &start;

        for (;;)
        {
            for (auto b : front->predecessors)
            {
                if (b->tempData.get<heart::Block*>() == nullptr)
                {
                    if (! visitor (b))
                        return false;

                    back->tempData.set (&b.get());
                    back = &b.get();
                }
            }

            if (front == back)
                break;

            front = front->tempData.get<heart::Block*>();
        }

        return true;
    }

    //==============================================================================
    static std::vector<pool_ref<heart::Variable>> findUninitialisedVariableUse (const heart::Function& f)
    {
        if (f.blocks.empty())
            return {};

        struct BlockState
        {
            ArrayWithPreallocation<pool_ref<heart::Variable>, 16> variablesUsedDuringBlock, variablesUnsafeAtEnd;
            bool isFullyResolved = false;

            static BlockState& get (const heart::Block& b)  { return *b.tempData.get<BlockState*>(); }
        };

        std::vector<BlockState> states;

        {
            states.resize (f.blocks.size());
            size_t index = 0;
            std::vector<pool_ref<heart::Variable>> allVariables;

            for (auto& block : f.blocks)
            {
                auto& state = states[index++];
                block->tempData.set (std::addressof (state));

                block->visitExpressions ([&] (pool_ref<heart::Expression>& value, AccessType)
                {
                    if (auto v = cast<heart::Variable> (value))
                        if (! (v->isState() || v->isParameter()))
                            state.variablesUsedDuringBlock.push_back (*v);
                });

                sortAndRemoveDuplicates (state.variablesUsedDuringBlock);
                appendVector (allVariables, state.variablesUsedDuringBlock);
                sortAndRemoveDuplicates (allVariables);
            }

            auto& firstBlockState = BlockState::get (f.blocks.front());
            firstBlockState.variablesUnsafeAtEnd = std::move (allVariables);
            removeFromVector (firstBlockState.variablesUnsafeAtEnd, firstBlockState.variablesUsedDuringBlock);
        }

        for (;;)
        {
            bool anyChanges = false;

            for (auto& b : f.blocks)
            {
                auto& state = BlockState::get (b);

                if (! (state.isFullyResolved || b->predecessors.empty()))
                {
                    auto& firstPreState = BlockState::get (b->predecessors.front());
                    auto variablesUnsafeAtEnd = firstPreState.variablesUnsafeAtEnd;
                    bool allPredecessorsResolved = firstPreState.isFullyResolved;

                    for (size_t i = 1; i < b->predecessors.size(); ++i)
                    {
                        auto& predState = BlockState::get (b->predecessors[i]);
                        appendVector (variablesUnsafeAtEnd, predState.variablesUnsafeAtEnd);
                        sortAndRemoveDuplicates (variablesUnsafeAtEnd);
                        allPredecessorsResolved = allPredecessorsResolved && predState.isFullyResolved;
                    }

                    removeFromVector (variablesUnsafeAtEnd, state.variablesUsedDuringBlock);

                    if (state.variablesUnsafeAtEnd != variablesUnsafeAtEnd)
                    {
                        state.variablesUnsafeAtEnd = std::move (variablesUnsafeAtEnd);

                        if (! allPredecessorsResolved)
                            anyChanges = true;
                    }

                    if (allPredecessorsResolved)
                        state.isFullyResolved = true;
                }
            }

            if (! anyChanges)
                break;
        }

        std::vector<pool_ref<heart::Variable>> results;

        for (auto& b : f.blocks)
        {
            decltype (BlockState::variablesUsedDuringBlock) unsafeVariables;

            for (auto& pred : b->predecessors)
            {
                appendVector (unsafeVariables, BlockState::get (pred).variablesUnsafeAtEnd);
                sortAndRemoveDuplicates (unsafeVariables);
            }

            auto visitRead = [&] (pool_ref<heart::Expression>& value, AccessType mode)
            {
                if (mode != AccessType::write)
                    if (auto v = cast<heart::Variable> (value))
                        if (! (v->isState() || v->isParameter()) && contains (unsafeVariables, *v))
                            results.push_back (*v);
            };

            for (auto s : b->statements)
            {
                s->visitExpressions (visitRead);

                // We'll only count direct writes to a variable, not to sub-elements or struct members - this is
                // to be conservative so that writes to part of a structure, array or vector are not considered
                // as fully overwriting the previous value
                if (auto assignment = cast<heart::Assignment> (*s))
                    if (auto v = cast<heart::Variable> (assignment->target))
                        if (! (v->isState() || v->isParameter()))
                            removeItem (unsafeVariables, *v);
            }

            b->terminator->visitExpressions (visitRead);
        }

        sortAndRemoveDuplicates (results);

        std::sort (results.begin(), results.end(), [] (pool_ref<heart::Variable> a, pool_ref<heart::Variable> b) -> bool
                                                   {
                                                       return a->name.toString() < b->name.toString();
                                                   });

        return results;
    }

    //==============================================================================
    static constexpr uint64_t perCallStackOverhead = 16;
    static constexpr uint64_t stackItemAlignment = 8;

    struct PreviousCall
    {
        PreviousCall* previous;
        heart::Function& function;

        bool contains (heart::Function& f) const
        {
            return std::addressof (f) == std::addressof (function)
                    || (previous != nullptr && previous->contains (f));
        }

        void findCallSequenceUpTo (heart::Function& f, std::vector<pool_ref<heart::Function>>& sequence) const
        {
            sequence.insert (sequence.begin(), function);

            if (previous != nullptr && std::addressof (f) != std::addressof (function))
                previous->findCallSequenceUpTo (f, sequence);
        }
    };

    static void iterateCallSequences (heart::Function& f,
                                      CallSequenceCheckResults& results,
                                      PreviousCall* previous,
                                      uint64_t stackSize)
    {
        calculateLocalVariableStackSize (f);
        stackSize += perCallStackOverhead + f.localVariableStackSize;
        results.maximumStackSize = std::max (results.maximumStackSize, stackSize);

        PreviousCall newPrevious { previous, f };

        if (previous != nullptr && previous->contains (f))
        {
            if (results.recursiveFunctionCallSequence.empty())
                previous->findCallSequenceUpTo (f, results.recursiveFunctionCallSequence);

            return;
        }

        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto call = cast<heart::FunctionCall> (*s))
                    iterateCallSequences (call->getFunction(), results, std::addressof (newPrevious), stackSize);
    }

    static void calculateLocalVariableStackSize (heart::Function& f)
    {
        if (f.localVariableStackSize == 0)
        {
            uint64_t total = 0;

            for (auto& v : f.getAllLocalVariables())
                total += getAlignedSize<stackItemAlignment> (v->getType().getPackedSizeInBytes());

            f.localVariableStackSize = total;
        }
    }
};

} // namespace soul
