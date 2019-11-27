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

    static std::vector<heart::VariablePtr> findVariablesBeingReadBeforeBeingWritten (const heart::Function& function)
    {
        return findUninitialisedVariableUse (function);
    }

    static bool doesFunctionCallAdvance (const heart::Function& f)
    {
        for (auto& b : f.blocks)
            if (doesBlockCallAdvance (*b))
                return true;

        return false;
    }

    static bool doesFunctionContainInfiniteLoops (const heart::Function& f)
    {
        if (f.blocks.empty())
            return false;

        if (f.blocks.front()->terminator->isReturn())
            return false;

        bool hasFoundTerminator = false;

        visitDownstreamBlocks (f, *f.blocks.front(), [&] (const heart::Block& b)
                               {
                                   if (! (b.terminator->isReturn() || doesBlockCallAdvance (b)))
                                       return true;

                                   hasFoundTerminator = true;
                                   return false;
                               });

        return ! hasFoundTerminator;
    }

    /** Returns the first set of functions which call each other in a cycle (or an empty vector if
        no cycles were found)
    */
    static std::vector<heart::FunctionPtr> findRecursiveFunctionCallSequences (Program& program)
    {
        std::vector<heart::FunctionPtr> callerFns;

        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions)
            {
                callerFns.clear();

                auto recursiveCallSequence = findRecursiveFunctions (*f, callerFns);

                if (! recursiveCallSequence.empty())
                    return recursiveCallSequence;
            }
        }

        return {};
    }

private:
    //==============================================================================
    static void resetVisitedFlags (const heart::Function& f)
    {
        for (auto& b : f.blocks)
            b->temporaryData = {};
    }

    static bool visitDownstreamBlocks (heart::Block& start, std::function<bool(heart::Block&)> visitor)
    {
        for (auto b : start.terminator->getDestinationBlocks())
        {
            if (b->temporaryData == nullptr)
            {
                if (! visitor (*b))
                    return false;

                b->temporaryData = (void*) 1u;

                if (! visitDownstreamBlocks (*b, visitor))
                    return false;
            }
        }

        return true;
    }

    static bool visitUpstreamBlocks (heart::Block& start, std::function<bool(heart::Block&)> visitor)
    {
        for (auto b : start.predecessors)
        {
            if (b->temporaryData == nullptr)
            {
                if (! visitor (*b))
                    return false;

                b->temporaryData = (void*) 1u;

                if (! visitUpstreamBlocks (*b, visitor))
                    return false;
            }
        }

        return true;
    }

    //==============================================================================
    static std::vector<heart::VariablePtr> findUninitialisedVariableUse (const heart::Function& f)
    {
        if (f.blocks.empty())
            return {};

        struct BlockState
        {
            ArrayWithPreallocation<heart::VariablePtr, 16> variablesUsedDuringBlock, variablesUnsafeAtEnd;
            bool isFullyResolved = false;

            static BlockState& get (const heart::Block& b)  { return *static_cast<BlockState*> (b.temporaryData); }
        };

        std::vector<BlockState> states;

        {
            states.resize (f.blocks.size());
            size_t index = 0;
            std::vector<heart::VariablePtr> allVariables;

            for (auto& block : f.blocks)
            {
                auto& state = states[index++];
                block->temporaryData = std::addressof (state);

                block->visitExpressions ([&] (heart::ExpressionPtr& value, heart::AccessType)
                {
                    if (auto v = cast<heart::Variable> (value))
                        if (! (v->isState() || v->isParameter()))
                            state.variablesUsedDuringBlock.push_back (v);
                });

                sortAndRemoveDuplicates (state.variablesUsedDuringBlock);
                appendVector (allVariables, state.variablesUsedDuringBlock);
                sortAndRemoveDuplicates (allVariables);
            }

            auto& firstBlockState = BlockState::get (*f.blocks.front());
            firstBlockState.variablesUnsafeAtEnd = std::move (allVariables);
            removeFromVector (firstBlockState.variablesUnsafeAtEnd, firstBlockState.variablesUsedDuringBlock);
        }

        for (;;)
        {
            bool anyChanges = false;

            for (auto& b : f.blocks)
            {
                auto& state = BlockState::get (*b);

                if (! (state.isFullyResolved || b->predecessors.empty()))
                {
                    auto& firstPreState = BlockState::get (*b->predecessors.front());
                    auto variablesUnsafeAtEnd = firstPreState.variablesUnsafeAtEnd;
                    bool allPredecessorsResolved = firstPreState.isFullyResolved;

                    for (size_t i = 1; i < b->predecessors.size(); ++i)
                    {
                        auto& predState = BlockState::get (*b->predecessors[i]);
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

        std::vector<heart::VariablePtr> results;

        for (auto& b : f.blocks)
        {
            decltype (BlockState::variablesUsedDuringBlock) unsafeVariables;

            for (auto& pred : b->predecessors)
            {
                appendVector (unsafeVariables, BlockState::get (*pred).variablesUnsafeAtEnd);
                sortAndRemoveDuplicates (unsafeVariables);
            }

            auto visitRead = [&] (heart::ExpressionPtr& value, heart::AccessType mode)
            {
                if (mode != heart::AccessType::write)
                    if (auto v = cast<heart::Variable> (value))
                        if (! (v->isState() || v->isParameter()) && contains (unsafeVariables, v))
                            results.push_back (v);
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
                            removeItem (unsafeVariables, v);
            }

            b->terminator->visitExpressions (visitRead);
        }

        sortAndRemoveDuplicates (results);

        std::sort (results.begin(), results.end(), [] (heart::VariablePtr a, heart::VariablePtr b) -> bool
                                                   {
                                                       return a->name.toString() < b->name.toString();
                                                   });

        return results;
    }

    //==============================================================================
    static std::vector<heart::FunctionPtr> findRecursiveFunctions (heart::Function& f,
                                                                   std::vector<heart::FunctionPtr>& callerFns)
    {
        callerFns.emplace_back (f);

        for (auto& b : f.blocks)
        {
            for (auto s : b->statements)
            {
                if (auto call = cast<heart::FunctionCall> (*s))
                {
                    for (size_t i = 0; i < callerFns.size(); ++i)
                    {
                        if (callerFns[i] == call->function)
                        {
                            std::vector<heart::FunctionPtr> functions;

                            while (i < callerFns.size())
                                functions.push_back (callerFns[i++]);

                            return functions;
                        }
                    }

                    auto result = findRecursiveFunctions (call->getFunction(), callerFns);

                    if (! result.empty())
                        return result;
                }
            }
        }

        callerFns.pop_back();
        return {};
    }

    static bool doesBlockCallAdvance (const heart::Block& b)
    {
        for (auto s : b.statements)
            if (is_type<heart::AdvanceClock> (*s))
                return true;

        return false;
    }
};

} // namespace soul
