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
    This pass does some basic simplifications and optimisations
*/
struct Optimisations
{
    static void removeUnusedVariables (Program& program)
    {
        for (auto& m : program.getModules())
        {
            m->rebuildVariableUseCounts();

            for (auto f : m->functions)
                removeDuplicateConstants (*f);

            m->rebuildVariableUseCounts();

            for (auto f : m->functions)
                convertWriteOnceVariablesToConstants (*f);

            m->rebuildVariableUseCounts();

            for (auto f : m->functions)
                removeUnusedVariables (*f);
        }
    }

    static void removeUnusedFunctions (Program& program, Module& mainModule)
    {
        removeCallsToVoidFunctionsWithoutSideEffects (program);

        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                f->functionUseTestFlag = false;

        for (auto f : mainModule.functions)
            if (f->isExported)
                recursivelyFlagFunctionUse (*f);

        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                if (! f->functionUseTestFlag && f->annotation.getBool ("do_not_optimise"))
                    recursivelyFlagFunctionUse (*f);

        for (auto& m : program.getModules())
            removeIf (m->functions, [] (heart::FunctionPtr f) { return ! f->functionUseTestFlag; });
    }

    static void removeUnusedStructs (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& s : m->structs)
                s->activeUseFlag = false;

        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions)
            {
                recursivelyFlagStructUse (f->returnType);

                for (auto& p : f->parameters)
                    recursivelyFlagStructUse (p->getType());

                f->visitExpressions ([] (heart::ExpressionPtr& value, heart::AccessType)
                {
                    recursivelyFlagStructUse (value->getType());
                });
            }

            for (auto& i : m->inputs)
                for (auto& t : i->sampleTypes)
                    recursivelyFlagStructUse (t);

            for (auto& o : m->outputs)
                for (auto& t : o->sampleTypes)
                    recursivelyFlagStructUse (t);
        }

        for (auto& m : program.getModules())
            removeIf (m->structs, [] (const StructurePtr& s) { return ! s->activeUseFlag; });
    }

    static void optimiseFunctionBlocks (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto f : m->functions)
                optimiseFunctionBlocks (*f, program.getAllocator());
    }

    static void optimiseFunctionBlocks (heart::Function& f, heart::Allocator& allocator)
    {
        f.rebuildBlockPredecessors();
        eliminateEmptyAndUnreachableBlocks (f, allocator);
        eliminateUnreachableBlockCycles (f);
        mergeAdjacentBlocks (f);
    }

    template <typename EndpointPropertyProvider>
    static void removeUnconnectedEndpoints (Module& module, EndpointPropertyProvider& epp)
    {
        removeUnconnectedInputs (module, epp);
        removeUnconnectedOutputs (module, epp);
    }

private:
    static bool eliminateEmptyAndUnreachableBlocks (heart::Function& f, heart::Allocator& allocator)
    {
        return BlockHelpers::removeBlocks (f, [&] (heart::BlockPtr b) -> bool
        {
            if (b->doNotOptimiseAway || b == f.blocks.front())
                return false;

            if (b->predecessors.empty())
                return true;

            if (! b->statements.empty())
                return false;

            if (b->terminator == nullptr)
                return false;

            auto destinations = b->terminator->getDestinationBlocks();
            auto numDestinations = destinations.size();

            if (numDestinations > 1)
                return false;

            if (numDestinations == 1)
            {
                if (b == *destinations.begin())
                    return false;

                for (auto pred : b->predecessors)
                {
                    SOUL_ASSERT (pred->terminator != nullptr);
                    BlockHelpers::replaceBlockDestination (*pred, b, *destinations.begin());
                }

                return true;
            }

            if (is_type<heart::ReturnVoid> (b->terminator))
            {
                if (BlockHelpers::areAllTerminatorsUnconditional (b->predecessors))
                {
                    for (auto pred : b->predecessors)
                        pred->terminator = allocator.allocate<heart::ReturnVoid>();

                    return true;
                }
            }

            return false;
        });
    }

    //==============================================================================
    static bool eliminateUnreachableBlockCycles (heart::Function& f)
    {
        return BlockHelpers::removeBlocks (f, [&] (heart::BlockPtr b) -> bool
        {
            return f.blocks.front() != b
                    && ! isReachableFrom (f, *b, f.blocks.front());
        });
    }

    static bool isReachableFrom (heart::Function& f, heart::Block& dest, heart::BlockPtr source)
    {
        bool result = false;

        CallFlowGraph::visitUpstreamBlocks (f, dest,
            [source, &result] (heart::Block& b) -> bool
            {
                if (std::addressof (b) == source) { result = true; return false; }
                return true;
            });

        return result;
    }

    //==============================================================================
    static bool mergeAdjacentBlocks (heart::Function& f)
    {
        return BlockHelpers::removeBlocks (f, [&] (heart::BlockPtr b) -> bool
        {
            if (b->predecessors.size() != 1 || b->doNotOptimiseAway)
                return false;

            auto pred = b->predecessors.front();

            if (pred == b || pred->terminator->isConditional())
                return false;

            SOUL_ASSERT (*pred->terminator->getDestinationBlocks().begin() == b);

            if (auto first = b->statements.begin())
                if (*first != nullptr)
                    pred->statements.append (**first);

            pred->terminator = b->terminator;
            return true;
        });
    }

    //==============================================================================
    static void recursivelyFlagFunctionUse (heart::Function& sourceFn)
    {
        if (! sourceFn.functionUseTestFlag)
        {
            sourceFn.functionUseTestFlag = true;

            sourceFn.visitStatements<heart::FunctionCall> ([] (heart::FunctionCall& fc)
            {
                recursivelyFlagFunctionUse (fc.getFunction());
            });

            sourceFn.visitExpressions ([] (heart::ExpressionPtr& value, heart::AccessType)
            {
                if (auto fc = cast<heart::PureFunctionCall> (value))
                    recursivelyFlagFunctionUse (fc->function);
            });
        }
    }

    static void removeCallsToVoidFunctionsWithoutSideEffects (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                for (auto& b : f->blocks)
                    b->statements.removeMatches ([] (heart::Statement& s)
                                                 {
                                                     if (auto call = cast<heart::FunctionCall> (s))
                                                         return call->target == nullptr && ! call->getFunction().mayHaveSideEffects();

                                                     return false;
                                                 });
    }

    static void recursivelyFlagStructUse (const Type& type)
    {
        if (type.isStruct())
        {
            type.getStructRef().activeUseFlag = true;

            for (auto& m : type.getStructRef().members)
                recursivelyFlagStructUse (m.type);
        }
        else if (type.isArray())
        {
            recursivelyFlagStructUse (type.getArrayElementType());
        }
    }

    //==============================================================================
    static bool findAndReplaceFirstDuplicateConstant (heart::Function& f)
    {
        for (auto b : f.blocks)
        {
            LinkedList<heart::Statement>::Iterator last;

            for (auto s : b->statements)
            {
                if (auto a = cast<heart::AssignFromValue> (*s))
                {
                    if (auto target = cast<heart::Variable> (a->target))
                    {
                        if (target->isConstant())
                        {
                            if (auto source = cast<heart::Variable> (a->source))
                            {
                                if (source->isConstant())
                                {
                                    b->statements.removeNext (last);

                                    f.visitExpressions ([target, source] (heart::ExpressionPtr& value, heart::AccessType mode)
                                    {
                                        if (value == target && mode == heart::AccessType::read)
                                            value = source;
                                    });

                                    return true;
                                }
                            }
                        }
                    }
                }

                last = *s;
            }
        }

        return false;
    }

    static void removeDuplicateConstants (heart::Function& f)
    {
        while (findAndReplaceFirstDuplicateConstant (f))
        {}
    }

    static void removeUnusedVariables (heart::Function& f)
    {
        for (auto b : f.blocks)
        {
            b->statements.removeMatches ([] (heart::Statement& s)
            {
                if (auto a = cast<heart::Assignment> (s))
                    if (auto target = cast<heart::Variable> (a->target))
                        return target->numWrites == 1 && target->numReads == 0 && target->isFunctionLocal();

                return false;
            });
        }
    }

    static void convertWriteOnceVariablesToConstants (heart::Function& f)
    {
        f.visitStatements<heart::Assignment> ([] (heart::Assignment& a)
        {
            if (auto target = cast<heart::Variable> (a.target))
                if (target->numWrites == 1 && target->isMutableLocal())
                    target->role = heart::Variable::Role::constant;
        });
    }

    //==============================================================================
    template <typename EndpointPropertyProvider>
    static void removeUnconnectedInputs (Module& module, EndpointPropertyProvider& epp)
    {
        std::vector<heart::InputDeclarationPtr> toRemove;

        for (auto& i : module.inputs)
            if (! epp.findInputEndpointProperties (*i).initialised)
                toRemove.push_back (i);

        removeFromVector (module.inputs, toRemove);

        removeIf (module.connections,
                  [&] (heart::ConnectionPtr& connection)
                  {
                      if (connection->sourceProcessor == nullptr)
                          for (auto& i : toRemove)
                              if (connection->sourceChannel == i->name.toString())
                                  return true;

                      return false;
                  });

        for (auto& f : module.functions)
        {
            f->visitExpressions ([&] (heart::ExpressionPtr& value, heart::AccessType mode)
            {
                if (mode == heart::AccessType::read)
                {
                    if (auto i = cast<heart::InputDeclaration> (value))
                        if (contains (toRemove, i))
                            value = module.allocator.allocateZeroInitialiser (value->getType());
                }
            });
        }
    }

    template <typename EndpointPropertyProvider>
    static void removeUnconnectedOutputs (Module& module, EndpointPropertyProvider& epp)
    {
        std::vector<heart::OutputDeclarationPtr> toRemove;

        for (auto& o : module.outputs)
            if (! epp.findOutputEndpointProperties (*o).initialised)
                toRemove.push_back (o);

        removeFromVector (module.outputs, toRemove);

        removeIf (module.connections,
                  [&] (heart::ConnectionPtr& connection)
                  {
                      if (connection->destProcessor == nullptr)
                          for (auto& i : toRemove)
                              if (connection->destChannel == i->name.toString())
                                  return true;

                      return false;
                  });

        for (auto& f : module.functions)
        {
            for (auto& b : f->blocks)
            {
                b->statements.removeMatches ([&] (heart::StatementPtr s)
                {
                    if (auto w = cast<heart::WriteStream> (s))
                        return contains (toRemove, w->target);

                    return false;
                });
            }
        }
    }
};

} // namespace soul
