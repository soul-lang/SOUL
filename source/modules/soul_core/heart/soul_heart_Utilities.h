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
    static std::string getDescriptionOfTypeList (choc::span<Type> types, bool alwaysParenthesise)
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
            for (auto& f : m->functions.get())
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

            for (auto& v : m->stateVariables.get())
                visit (v->type);

        }
    }

    template <typename VisitorFn>
    static void visitAllMutableTypes (Program& program, VisitorFn&& visit)
    {
        for (auto& m : program.getModules())
        {
            for (auto& s : m->structs.get())
                for (auto& member : s->getMembers())
                    visit (member.type);

            for (auto& f : m->functions.get())
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
        VariableListByType (choc::span<pool_ref<Variable>> variables)
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
    static pool_ptr<Statement> findFirstStreamAccess (Function& f)
    {
        for (auto& b : f.blocks)
        {
            for (auto s : b->statements)
            {
                if (auto w = cast<WriteStream> (*s))
                    if (w->target->isStreamEndpoint())
                        return w;

                if (auto r = cast<ReadStream> (*s))
                    return r;
            }
        }

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
            for (auto& f : module.functions.get())
            {
                if (! f->functionType.isRun())
                {
                    auto s = findFirstStreamAccess (f);
                    auto a = findFirstAdvanceCall (f);

                    if (s != nullptr || a != nullptr)
                    {
                        if (f->functionType.isUserInit() || f->functionType.isEvent())
                        {
                            if (a != nullptr)
                                a->location.throwError (Errors::advanceCannotBeCalledHere());

                            if (f->functionType.isUserInit())
                                s->location.throwError (Errors::streamsCannotBeUsedDuringInit());
                            else
                                s->location.throwError (Errors::streamsCannotBeUsedInEventCallbacks());
                        }

                        if (OptimiserClass::inlineAllCallsToFunction (program, f))
                            return true;

                        if (a != nullptr)
                            a->location.throwError (Errors::advanceCannotBeCalledHere());

                        s->location.throwError (Errors::streamsCanOnlyBeUsedInRun());
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

    static bool areAllTerminatorsUnconditional (choc::span<pool_ref<Block>> blocks)
    {
        for (auto b : blocks)
            if (b->terminator->isConditional())
                return false;

        return true;
    }

    template <typename StringType>
    static pool_ptr<Block> findBlock (const Function& f, const StringType& targetName)
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

    //==============================================================================
    template <typename Subclass, typename ProcessorType, typename ConnectionType, typename ContextType>
    struct GraphTraversalHelper
    {
        void reserve (size_t numNodes)    { nodes.reserve (numNodes); }

        void addNode (const ProcessorType& p)
        {
            nodes.push_back ({ p });
        }

        void addConnection (const ProcessorType& source, const ProcessorType& dest, const ConnectionType& c)
        {
            auto src = findNode (source);
            auto dst = findNode (dest);
            SOUL_ASSERT (src != nullptr && dst != nullptr);
            dst->sources.push_back ({ *src, c });
        }

        void checkAndThrowErrorIfCycleFound() const
        {
            std::vector<const Node*> visitedStack;
            visitedStack.reserve (256);

            for (auto& n : nodes)
            {
                check (n, visitedStack, {});
                SOUL_ASSERT (visitedStack.empty());
            }
        }

    private:
        struct Node
        {
            struct Source
            {
                Node& node;
                const ConnectionType& connection;
            };

            const ProcessorType& processor;
            ArrayWithPreallocation<Source, 4> sources;
        };

        std::vector<Node> nodes;

        Node* findNode (const ProcessorType& p)
        {
            for (auto& n : nodes)
                if (std::addressof (n.processor) == std::addressof (p))
                    return std::addressof (n);

            return {};
        }

        static void check (const Node& node, std::vector<const Node*>& visitedStack, const ContextType& errorContext)
        {
            if (std::find (visitedStack.begin(), visitedStack.end(), std::addressof (node)) != visitedStack.end())
                throwCycleError (visitedStack, errorContext);

            visitedStack.push_back (std::addressof (node));

            for (auto& source : node.sources)
                check (source.node, visitedStack, Subclass::getContext (source.connection));

            visitedStack.pop_back();
        }

        static void throwCycleError (choc::span<const Node*> stack, const ContextType& errorContext)
        {
            std::vector<std::string> nodesInCycle;

            for (auto& node : stack)
                nodesInCycle.push_back (Subclass::getProcessorName (node->processor));

            nodesInCycle.push_back (nodesInCycle.front());
            std::reverse (nodesInCycle.begin(), nodesInCycle.end());

            errorContext.throwError (Errors::feedbackInGraph (choc::text::joinStrings (nodesInCycle, " -> ")));
        }
    };

    //==============================================================================
    struct CycleDetector  : public GraphTraversalHelper<CycleDetector, heart::ProcessorInstance, heart::Connection, CodeLocation>
    {
        CycleDetector (Module& graph)
        {
            reserve (graph.processorInstances.size());

            for (auto& p : graph.processorInstances)
                addNode (p);

            for (auto& c : graph.connections)
                if ((! c->delayLength) && c->source.processor != nullptr && c->dest.processor != nullptr)
                    addConnection (*c->source.processor, *c->dest.processor, c);
        }

        static std::string getProcessorName (const heart::ProcessorInstance& p)  { return p.instanceName; }
        static const CodeLocation& getContext (const heart::Connection& c)       { return c.location; }
    };
};

}
