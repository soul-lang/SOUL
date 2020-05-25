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
    static std::string getDescriptionOfTypeList (ArrayView<Type> types, bool alwaysParenthesise)
    {
        if (! alwaysParenthesise)
        {
            if (types.empty())
                return {};

            if (types.size() == 1)
                return types.front().getDescription();
        }

        return "(" + joinStrings (types, ", ", [] (auto& t) { return t.getDescription(); }) + ")";
    }

    template <typename VisitorFn>
    static void visitAllTypes (Program& program, VisitorFn&& visit)
    {
        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions)
            {
                visit (f->returnType);

                for (auto& p : f->parameters)
                    visit (p->type);

                f->visitExpressions ([&] (pool_ref<heart::Expression>& value, AccessType)
                {
                    visit (value->getType());
                });
            }

            for (auto& i : m->inputs)
                for (auto& t : i->dataTypes)
                    visit (t);

            for (auto& o : m->outputs)
                for (auto& t : o->dataTypes)
                    visit (t);
        }
    }

    template <typename VisitorFn>
    static void visitAllMutableTypes (Program& program, VisitorFn&& visit)
    {
        for (auto& m : program.getModules())
        {
            for (auto& s : m->structs)
                for (auto& member : s->getMembers())
                    visit (member.type);

            for (auto& f : m->functions)
            {
                visit (f->returnType);

                for (auto& p : f->parameters)
                    visit (p->type);

                f->visitExpressions ([&] (pool_ref<heart::Expression>& value, AccessType)
                {
                    if (auto v = cast<heart::Variable> (value))                 return visit (v->type);
                    if (auto c = cast<heart::TypeCast> (value))                 return visit (c->destType);
                });
            }

            for (auto& i : m->inputs)
                for (auto& t : i->dataTypes)
                    visit (t);

            for (auto& o : m->outputs)
                for (auto& t : o->dataTypes)
                    visit (t);
        }
    }

    //==============================================================================
    struct VariableListByType
    {
        VariableListByType (ArrayView<pool_ref<Variable>> variables)
        {
            for (auto& v : variables)
                getType (v->type).variables.push_back (v);
        }

        struct VariablesWithType
        {
            Type type;
            std::vector<pool_ref<Variable>> variables;
        };

        std::vector<VariablesWithType> types;

    private:
        VariablesWithType& getType (Type typeNeeded)
        {
            for (auto& type : types)
                if (type.type.isIdentical (typeNeeded))
                    return type;

            types.push_back ({ std::move (typeNeeded), {} });
            return types.back();
        }
    };

    //==============================================================================
    static pool_ptr<WriteStream> findFirstWrite (Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto w = cast<WriteStream> (*s))
                    return w;

        return {};
    }

    static pool_ptr<WriteStream> findFirstStreamWrite (Function& f)
    {
        for (auto& b : f.blocks)
            for (auto s : b->statements)
                if (auto w = cast<WriteStream> (*s))
                    if (w->target->isStreamEndpoint())
                        return w;

        return {};
    }

    static pool_ptr<AdvanceClock> findFirstAdvanceCall (Function& f)
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

        auto destModule   = program.getModuleContainingFunction (parentFunction);
        auto sourceModule = program.getModuleContainingFunction (targetFunction);
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
                    auto w = findFirstStreamWrite (f);
                    auto a = findFirstAdvanceCall (f);

                    if (w != nullptr || a != nullptr)
                    {
                        if (OptimiserClass::inlineAllCallsToFunction (program, f))
                            return true;

                        if (a != nullptr)
                            a->location.throwError (Errors::advanceCannotBeCalledHere());

                        if (f->functionType.isUserInit())
                            w->location.throwError (Errors::streamsCannotBeUsedDuringInit());

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
                while (inlineNextOccurrence (m))
                {}
            }
        }
    }

    //==============================================================================
    static inline Block& insertBlock (Module& module, Function& f, size_t blockIndex, const std::string& name)
    {
        SOUL_ASSERT (blockIndex <= f.blocks.size());
        auto& newBlock = module.allocate<Block> (module.allocator.get (name));
        f.blocks.insert (getIteratorForIndex (f.blocks, blockIndex), newBlock);
        return newBlock;
    }

    static inline Block& splitBlock (Module& module, Function& f, size_t blockIndex,
                                     LinkedList<Statement>::Iterator lastStatementOfFirstBlock,
                                     const std::string& newSecondBlockName)
    {
        auto& oldBlock = f.blocks[blockIndex].get();
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

    static void replaceBlockDestination (Block& block, Block& oldDest, Block& newDest)
    {
        for (auto& dest : block.terminator->getDestinationBlocks())
            if (dest == oldDest)
                dest = newDest;
    }

    static bool areAllTerminatorsUnconditional (ArrayView<pool_ref<Block>> blocks)
    {
        for (auto b : blocks)
            if (b->terminator->isConditional())
                return false;

        return true;
    }

    static pool_ptr<Block> findBlock (const Function& f, const std::string& targetName)
    {
        for (auto b : f.blocks)
            if (b->name == targetName)
                return b;

        return {};
    }

    static Type replaceUsesOfStruct (Type type, Structure& oldStruct, Structure& newStruct)
    {
        if (type.isArray())
            return type.createCopyWithNewArrayElementType (replaceUsesOfStruct (type.getElementType(), oldStruct, newStruct));

        if (type.isStruct())
        {
            if (type.getStruct() == oldStruct)
                return Type::createStruct (newStruct);

            for (auto& m : type.getStructRef().getMembers())
                if (m.type.usesStruct (oldStruct))
                    m.type = replaceUsesOfStruct (m.type, oldStruct, newStruct);
        }

        return type;
    }

    static Value replaceUsesOfStruct (Value value, Structure& oldStruct, Structure& newStruct)
    {
        if (value.getType().isStruct() || value.getType().isArray())
        {
            auto newValue = Value::zeroInitialiser (replaceUsesOfStruct (value.getType(), oldStruct, newStruct));

            if (value.getType().isStruct())
            {
                auto& srcStruct = value.getType().getStructRef();
                auto& destStruct = newValue.getType().getStructRef();

                for (size_t i = 0; i < destStruct.getNumMembers(); ++i)
                {
                    auto oldIndex = srcStruct.getMemberIndex (destStruct.getMemberName (i));
                    newValue.modifySubElementInPlace (i, replaceUsesOfStruct (value.getSubElement (oldIndex), oldStruct, newStruct));
                }
            }
            else
            {
                for (size_t i = 0; i < newValue.getType().getArraySize(); ++i)
                    newValue.modifySubElementInPlace (i, replaceUsesOfStruct (value.getSubElement (i), oldStruct, newStruct));
            }

            return newValue;
        }

        return value;
    }
};

}
