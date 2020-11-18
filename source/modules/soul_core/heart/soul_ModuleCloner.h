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

/** Performs a deep-clone of a Module object, which is trickier than it sounds.. */
struct ModuleCloner
{
    using FunctionMappings = std::unordered_map<pool_ref<const heart::Function>, pool_ptr<heart::Function>>;
    using StructMappings   = std::unordered_map<const Structure*, StructurePtr>;
    using VariableMappings = std::unordered_map<pool_ref<const heart::Variable>, pool_ptr<heart::Variable>>;

    ModuleCloner (const Module& source, Module& dest, FunctionMappings& functions, StructMappings& structs, VariableMappings& vars)
        : oldModule (source), newModule (dest), functionMappings (functions), structMappings (structs), variableMappings (vars)
    {
    }

    void createStructPlaceholders()
    {
        for (auto& s : oldModule.structs.get())
        {
            auto& mapping = structMappings[s.get()];
            SOUL_ASSERT (mapping == nullptr);
            mapping = newModule.structs.addCopy (*s);
        }
    }

    void cloneStructAndFunctionPlaceholders()
    {
        for (auto& s : oldModule.structs.get())
            populateClonedStruct (*s);

        for (auto& f : oldModule.functions.get())
            functionMappings[f] = newModule.functions.add (f->name, f->functionType.isEvent());
    }

    void clone()
    {
        for (auto& io : oldModule.inputs)
            newModule.inputs.push_back (clone (io));

        for (auto& io : oldModule.outputs)
            newModule.outputs.push_back (clone (io));

        if (oldModule.isGraph())
        {
            for (auto& mi : oldModule.processorInstances)
                newModule.processorInstances.push_back (clone (mi));

            for (auto& c : oldModule.connections)
                newModule.connections.push_back (clone (c));
        }

        for (auto& v : oldModule.stateVariables.get())
            newModule.stateVariables.add (getRemappedVariable (v));

        for (size_t i = 0; i <  oldModule.functions.size(); ++i)
            clone (newModule.functions.at (i), oldModule.functions.at (i));

        newModule.latency = oldModule.latency;
    }

    const Module& oldModule;
    Module& newModule;

    FunctionMappings& functionMappings;
    StructMappings& structMappings;
    VariableMappings& variableMappings;
    std::unordered_map<pool_ref<const heart::InputDeclaration>, pool_ptr<heart::InputDeclaration>> inputMappings;
    std::unordered_map<pool_ref<const heart::OutputDeclaration>, pool_ptr<heart::OutputDeclaration>> outputMappings;
    std::unordered_map<pool_ref<const heart::Block>, pool_ptr<heart::Block>> blockMappings;
    std::unordered_map<pool_ref<const heart::ProcessorInstance>, pool_ptr<heart::ProcessorInstance>> processorInstanceMappings;

    heart::Expression& cloneExpression (heart::Expression& old)
    {
        if (auto c = cast<heart::Constant> (old))
            return newModule.allocate<heart::Constant> (c->location, getRemappedValue (c->value));

        if (auto b = cast<heart::BinaryOperator> (old))
            return newModule.allocate<heart::BinaryOperator> (b->location,
                                                              cloneExpression (b->lhs),
                                                              cloneExpression (b->rhs),
                                                              b->operation);

        if (auto u = cast<heart::UnaryOperator> (old))
            return newModule.allocate<heart::UnaryOperator> (u->location, cloneExpression (u->source), u->operation);

        if (auto t = cast<heart::TypeCast> (old))
            return newModule.allocate<heart::TypeCast> (t->location, cloneExpression (t->source), cloneType (t->destType));

        if (auto f = cast<heart::PureFunctionCall> (old))
            return clone (*f);

        if (auto v = cast<heart::Variable> (old))
            return getRemappedVariable (*v);

        if (auto a = cast<heart::ArrayElement> (old))
            return cloneArrayElement (*a);

        if (auto s = cast<heart::StructElement> (old))
            return cloneStructElement (*s);

        if (auto l = cast<heart::AggregateInitialiserList> (old))
            return cloneInitialiserList (*l);

        auto pp = cast<heart::ProcessorProperty> (old);
        return newModule.allocate<heart::ProcessorProperty> (pp->location, pp->property);
    }

    pool_ptr<heart::Expression> cloneExpressionPtr (pool_ptr<heart::Expression> old)
    {
        if (old != nullptr)
            return cloneExpression (*old);

        return {};
    }

    heart::Variable& getRemappedVariable (heart::Variable& old)
    {
        auto v = variableMappings[old];
        return v == nullptr ? cloneVariable (old) : *v;
    }

    Value getRemappedValue (Value v)                                            { return v.cloneWithEquivalentType (cloneType (v.getType())); }
    heart::Block& getRemappedBlock (heart::Block& old)                          { return *blockMappings[old]; }
    heart::Function& getRemappedFunction (heart::Function& old)                 { return *functionMappings[old]; }
    heart::InputDeclaration& getRemappedInput (heart::InputDeclaration& old)    { return *inputMappings[old]; }
    heart::OutputDeclaration& getRemappedOutput (heart::OutputDeclaration& old) { return *outputMappings[old]; }

    static Type cloneType (StructMappings& structMappings, const Type& t)
    {
        if (t.isStruct())
            return Type::createStruct (*structMappings[t.getStruct().get()])
                     .withConstAndRefFlags (t.isConst(), t.isReference());

        if (t.isArray())
            return t.createCopyWithNewArrayElementType (cloneType (structMappings, t.getArrayElementType()));

        return t;
    }

    Type cloneType (const Type& t)
    {
        return cloneType (structMappings, t);
    }

    std::vector<Type> cloneTypes (ArrayView<Type> types)
    {
        std::vector<Type> result;
        result.reserve (types.size());

        for (auto& type : types)
            result.push_back (cloneType (type));

        return result;
    }

    heart::InputDeclaration& clone (const heart::InputDeclaration& old)
    {
        auto& io = newModule.allocate<heart::InputDeclaration> (old.location);
        auto& mapping = inputMappings[old];
        SOUL_ASSERT (mapping == nullptr);
        mapping = io;
        io.name = newModule.allocator.get (old.name);
        io.index = old.index;
        io.endpointType = old.endpointType;
        io.dataTypes = cloneTypes (old.dataTypes);
        io.annotation = old.annotation;
        io.arraySize = old.arraySize;
        return io;
    }

    heart::OutputDeclaration& clone (const heart::OutputDeclaration& old)
    {
        auto& io = newModule.allocate<heart::OutputDeclaration> (old.location);
        auto& mapping = outputMappings[old];
        SOUL_ASSERT (mapping == nullptr);
        mapping = io;
        io.name = newModule.allocator.get (old.name);
        io.index = old.index;
        io.endpointType = old.endpointType;
        io.dataTypes = cloneTypes (old.dataTypes);
        io.annotation = old.annotation;
        io.arraySize = old.arraySize;
        return io;
    }

    heart::Connection& clone (const heart::Connection& old)
    {
        auto& c = newModule.allocate<heart::Connection> (old.location);
        c.interpolationType     = old.interpolationType;
        c.source.processor      = getRemappedProcessorInstance (old.source.processor);
        c.source.endpointName   = old.source.endpointName;
        c.source.endpointIndex  = old.source.endpointIndex;
        c.dest.processor        = getRemappedProcessorInstance (old.dest.processor);
        c.dest.endpointName     = old.dest.endpointName;
        c.dest.endpointIndex    = old.dest.endpointIndex;
        c.delayLength           = old.delayLength;
        return c;
    }

    pool_ptr<heart::ProcessorInstance> getRemappedProcessorInstance (pool_ptr<heart::ProcessorInstance> old)
    {
        if (old == nullptr)
            return {};

        return processorInstanceMappings[*old];
    }

    heart::ProcessorInstance& clone (const heart::ProcessorInstance& old)
    {
        auto& p = newModule.allocate<heart::ProcessorInstance> (old.location);
        auto& mapping = processorInstanceMappings[old];
        SOUL_ASSERT (mapping == nullptr);
        mapping = p;
        p.instanceName = old.instanceName;
        p.sourceName = old.sourceName;
        p.clockMultiplier = old.clockMultiplier;
        p.arraySize = old.arraySize;
        return p;
    }

    heart::Variable& cloneVariable (const heart::Variable& old)
    {
        auto& mapping = variableMappings[old];
        SOUL_ASSERT (mapping == nullptr);
        auto& v = newModule.allocate<heart::Variable> (old.location, cloneType (old.type),
                                                       newModule.allocator.get (old.name),
                                                       old.role);
        v.externalHandle = old.externalHandle;

        if (old.initialValue != nullptr)
            v.initialValue = cloneExpression (*old.initialValue);

        v.annotation = old.annotation;
        mapping = v;
        return v;
    }

    heart::ArrayElement& cloneArrayElement (const heart::ArrayElement& old)
    {
        auto& s = newModule.allocate<heart::ArrayElement> (old.location,
                                                           cloneExpression (old.parent),
                                                           old.fixedStartIndex,
                                                           old.fixedEndIndex);

        s.dynamicIndex = cloneExpressionPtr (old.dynamicIndex);
        s.suppressWrapWarning = old.suppressWrapWarning;
        s.isRangeTrusted = old.isRangeTrusted;
        return s;
    }

    heart::StructElement& cloneStructElement (const heart::StructElement& old)
    {
        return newModule.allocate<heart::StructElement> (old.location,
                                                         cloneExpression (old.parent),
                                                         old.memberName);
    }

    void populateClonedStruct (const Structure& old)
    {
        for (auto& m : structMappings[std::addressof (old)]->getMembers())
            m.type = cloneType (m.type);
    }

    heart::AggregateInitialiserList& cloneInitialiserList (const heart::AggregateInitialiserList& old)
    {
        auto& l = newModule.allocate<heart::AggregateInitialiserList> (old.location, old.type);

        for (auto& i : old.items)
            l.items.push_back (cloneExpression (i));

        return l;
    }

    heart::Block& createNewBlock (const heart::Block& old)
    {
        auto& b = newModule.allocate<heart::Block> (newModule.allocator.get (old.name));
        blockMappings[old] = b;
        return b;
    }

    void clone (heart::Block& b, const heart::Block& old)
    {
        LinkedList<heart::Statement>::Iterator last;

        for (auto p : old.parameters)
            b.parameters.push_back (cloneVariable (p));

        for (auto s : old.statements)
            last = b.statements.insertAfter (last, cloneStatement (*s));

        SOUL_ASSERT (old.isTerminated());
        b.terminator = cloneTerminator (*old.terminator);
    }

    heart::Statement& cloneStatement (heart::Statement& s)
    {
        #define SOUL_CLONE_STATEMENT(Type)     if (auto t = cast<const heart::Type> (s)) return clone (*t);
        SOUL_HEART_STATEMENTS (SOUL_CLONE_STATEMENT)
        #undef SOUL_CLONE_STATEMENT
        SOUL_ASSERT_FALSE;
        return s;
    }

    heart::Terminator& cloneTerminator (heart::Terminator& s)
    {
        #define SOUL_CLONE_TERMINATOR(Type)    if (auto t = cast<const heart::Type> (s)) return clone (*t);
        SOUL_HEART_TERMINATORS (SOUL_CLONE_TERMINATOR)
        #undef SOUL_CLONE_TERMINATOR
        SOUL_ASSERT_FALSE;
        return s;
    }

    heart::Branch& clone (const heart::Branch& old)
    {
        auto& b = newModule.allocate<heart::Branch> (getRemappedBlock (old.target));

        for (auto& arg : old.targetArgs)
            b.targetArgs.push_back (cloneExpression (arg));

        return b;
    }

    heart::BranchIf& clone (const heart::BranchIf& old)
    {
        auto& b =  newModule.allocate<heart::BranchIf> (cloneExpression (old.condition),
                                                        getRemappedBlock (old.targets[0]),
                                                        getRemappedBlock (old.targets[1]));

        for (auto& arg : old.targetArgs[0])
            b.targetArgs[0].push_back (cloneExpression (arg));

        for (auto& arg : old.targetArgs[1])
            b.targetArgs[1].push_back (cloneExpression (arg));

        return b;
    }

    heart::ReturnVoid& clone (const heart::ReturnVoid&)
    {
        return newModule.allocate<heart::ReturnVoid>();
    }

    heart::ReturnValue& clone (const heart::ReturnValue& old)
    {
        return newModule.allocate<heart::ReturnValue> (cloneExpression (old.returnValue));
    }

    heart::AssignFromValue& clone (const heart::AssignFromValue& old)
    {
        return newModule.allocate<heart::AssignFromValue> (old.location,
                                                           cloneExpression (*old.target),
                                                           cloneExpression (old.source));
    }

    heart::FunctionCall& clone (const heart::FunctionCall& old)
    {
        auto& fc = newModule.allocate<heart::FunctionCall> (old.location,
                                                            cloneExpressionPtr (old.target),
                                                            getRemappedFunction (old.getFunction()));

        for (auto& arg : old.arguments)
            fc.arguments.push_back (cloneExpression (arg));

        return fc;
    }

    heart::PureFunctionCall& clone (const heart::PureFunctionCall& old)
    {
        auto& fc = newModule.allocate<heart::PureFunctionCall> (old.location,
                                                                getRemappedFunction (old.function));

        for (auto& arg : old.arguments)
            fc.arguments.push_back (cloneExpression (arg));

        return fc;
    }

    heart::ReadStream& clone (const heart::ReadStream& old)
    {
        return newModule.allocate<heart::ReadStream> (old.location,
                                                      cloneExpression (*old.target),
                                                      getRemappedInput (old.source));
    }

    heart::WriteStream& clone (const heart::WriteStream& old)
    {
        return newModule.allocate<heart::WriteStream> (old.location,
                                                       getRemappedOutput (old.target),
                                                       cloneExpressionPtr (old.element),
                                                       cloneExpression (old.value));
    }

    heart::AdvanceClock& clone (const heart::AdvanceClock& a)
    {
        return newModule.allocate<heart::AdvanceClock> (a.location);
    }

    void clone (heart::Function& f, const heart::Function& old)
    {
        blockMappings.clear();

        f.location = old.location;
        f.returnType = cloneType (old.returnType);
        f.name = newModule.allocator.get (old.name);
        f.functionType = old.functionType;
        f.intrinsicType = old.intrinsicType;
        f.isExported = old.isExported;
        f.hasNoBody = old.hasNoBody;
        f.annotation = old.annotation;

        for (auto& p : old.parameters)
            f.parameters.push_back (cloneVariable (p));

        for (auto& b : old.blocks)
            f.blocks.push_back (createNewBlock (b));

        for (size_t i = 0; i < f.blocks.size(); ++i)
            clone (f.blocks[i], old.blocks[i]);
    }
};

}
