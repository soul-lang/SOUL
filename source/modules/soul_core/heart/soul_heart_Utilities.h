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

struct heart::Utilities
{
    static WriteStreamPtr findFirstWrite (Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto w = cast<WriteStream> (*s))
                    return w;

        return {};
    }

    static WriteStreamPtr findFirstStreamWrite (Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto w = cast<WriteStream> (*s))
                    if (w->target != nullptr && w->target->isStreamEndpoint())
                        return w;

        return {};
    }

    static AdvanceClockPtr findFirstAdvanceCall (Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto a = cast<AdvanceClock> (*s))
                    return a;

        return {};
    }

    static bool doesBlockCallAdvance (const Block& b)
    {
        for (auto s : b.statements)
            if (is_type<AdvanceClock> (*s))
                return true;

        return false;
    }

    static bool canFunctionBeInlined (Program& program,
                                      Function& parentFunction,
                                      FunctionCall& call)
    {
        auto& targetFunction = call.getFunction();

        if (targetFunction.functionType.isRun() || targetFunction.functionType.isSystemInit()
             || targetFunction.functionType.isUserInit() || targetFunction.functionType.isEvent()
             || targetFunction.hasNoBody)
            return false;

        auto destModule   = program.findModuleContainingFunction (parentFunction);
        auto sourceModule = program.findModuleContainingFunction (targetFunction);
        SOUL_ASSERT (destModule != nullptr && sourceModule != nullptr);

        // NB: cross-processor inlining is not allowed, to avoid confusion over endpoints, advances, etc
        return destModule == sourceModule || sourceModule->isNamespace();
    }

    template <typename OptimiserClass>
    static void inlineFunctionsThatUseAdvanceOrStreams (Program& program)
    {
        auto inlineNextOccurrence = [&] (Module& module) -> bool
        {
            for (auto& f : module.functions)
            {
                if (! f->functionType.isRun())
                {
                    auto w = findFirstStreamWrite (*f);
                    auto a = findFirstAdvanceCall (*f);

                    if (w != nullptr || a != nullptr)
                    {
                        if (OptimiserClass::inlineAllCallsToFunction (program, *f))
                            return true;

                        if (a != nullptr)
                            a->location.throwError (Errors::advanceCannotBeCalledHere());
                        else if (f->functionType.isUserInit())
                            w->location.throwError (Errors::streamsCannotBeUsedDuringInit());
                        else
                            w->location.throwError (Errors::streamsCanOnlyBeUsedInRun());
                    }
                }
            }

            return false;
        };

        for (auto& m : program.getModules())
        {
            if (m->isProcessor())
            {
                while (inlineNextOccurrence (*m))
                {}
            }
        }
    }

    //==============================================================================
    static inline Block& insertBlock (Module& module, Function& f, size_t blockIndex, const std::string& name)
    {
        SOUL_ASSERT (blockIndex <= f.blocks.size());
        auto& newBlock = module.allocate<Block> (module.allocator.get (name));
        f.blocks.insert (f.blocks.begin() + (ssize_t) blockIndex, newBlock);
        return newBlock;
    }

    static inline Block& splitBlock (Module& module, Function& f, size_t blockIndex,
                                     LinkedList<Statement>::Iterator lastStatementOfFirstBlock,
                                     const std::string& newSecondBlockName)
    {
        auto& oldBlock = *f.blocks[blockIndex];
        auto& newBlock = insertBlock (module, f, blockIndex + 1, newSecondBlockName);

        if (lastStatementOfFirstBlock != nullptr)
        {
            if (auto next = lastStatementOfFirstBlock.next())
            {
                newBlock.statements.append (*next);
                lastStatementOfFirstBlock.removeAllSuccessors();
            }
        }
        else
        {
            newBlock.statements = oldBlock.statements;
            oldBlock.statements.clear();
        }

        newBlock.terminator = oldBlock.terminator;
        oldBlock.terminator = module.allocate<Branch> (newBlock);

        return newBlock;
    }

    template <typename Pred>
    static bool removeBlocks (Function& f, Pred shouldRemove)
    {
        bool anyRemoved = false;

        while (removeFirst (f.blocks, shouldRemove))
        {
            anyRemoved = true;
            f.rebuildBlockPredecessors();
        }

        return anyRemoved;
    }

    static void replaceBlockDestination (Block& block, BlockPtr oldDest, BlockPtr newDest)
    {
        for (auto& dest : block.terminator->getDestinationBlocks())
            if (dest == oldDest)
                dest = newDest;
    }

    static bool areAllTerminatorsUnconditional (ArrayView<BlockPtr> blocks)
    {
        for (auto b : blocks)
            if (b->terminator->isConditional())
                return false;

        return true;
    }

    static BlockPtr findBlock (const Function& f, const std::string& targetName) noexcept
    {
        for (auto b : f.blocks)
            if (b->name == targetName)
                return b;

        return {};
    }
};

}
