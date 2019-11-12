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
    using FunctionMappings = std::unordered_map<pool_ptr<const heart::Function>, heart::FunctionPtr>;
    using StructMappings   = std::unordered_map<const Structure*, StructurePtr>;

    ModuleCloner (const Module& source, Module& dest, FunctionMappings& functions, StructMappings& structs) noexcept
        : oldModule (source), newModule (dest), functionMappings (functions), structMappings (structs)
    {
    }

    void cloneStructAndFunctionPlaceholders()
    {
        for (auto& s : oldModule.structs)
            newModule.structs.push_back (createStructPlaceholder (*s));

        for (auto& s : oldModule.structs)
            populateClonedStruct (*s);

        for (auto& f : oldModule.functions)
            newModule.functions.push_back (createNewFunctionObject (*f));
    }

    void clone()
    {
        for (auto& io : oldModule.inputs)
            newModule.inputs.push_back (clone (*io));

        for (auto& io : oldModule.outputs)
            newModule.outputs.push_back (clone (*io));

        if (oldModule.isGraph())
        {
            for (auto& mi : oldModule.processorInstances)
                newModule.processorInstances.push_back (clone (*mi));

            for (auto& c : oldModule.connections)
                newModule.connections.push_back (clone (*c));
        }

        for (auto& v : oldModule.stateVariables)
            newModule.stateVariables.push_back (cloneVariable (*v));

        for (size_t i = 0; i < oldModule.functions.size(); ++i)
            clone (*newModule.functions[i], *oldModule.functions[i]);
    }

    const Module& oldModule;
    Module& newModule;

    FunctionMappings& functionMappings;
    StructMappings& structMappings;
    std::unordered_map<pool_ptr<const heart::InputDeclaration>, heart::InputDeclarationPtr> inputMappings;
    std::unordered_map<pool_ptr<const heart::OutputDeclaration>, heart::OutputDeclarationPtr> outputMappings;
    std::unordered_map<pool_ptr<const heart::Variable>, heart::VariablePtr> variableMappings;
    std::unordered_map<pool_ptr<const heart::Block>, heart::BlockPtr> blockMappings;
    std::unordered_map<pool_ptr<const heart::ProcessorInstance>, heart::ProcessorInstancePtr> processorInstanceMappings;

    heart::Block& getRemappedBlock (heart::Block& old)
    {
        auto& b = blockMappings[old];
        SOUL_ASSERT (b != nullptr);
        return *b;
    }

    Value getRemappedValue (Value v)
    {
        return v.cloneWithEquivalentType (cloneType (v.getType()));
    }

    heart::Expression& getRemappedExpressionRef (heart::Expression& old)
    {
        return *getRemappedExpression (old);
    }

    heart::ExpressionPtr getRemappedExpression (heart::ExpressionPtr old)
    {
        if (old == nullptr)
            return {};

        if (auto c = cast<heart::Constant> (old))
            return newModule.allocate<heart::Constant> (c->location, getRemappedValue (c->value));

        if (auto pp = cast<heart::ProcessorProperty> (old))
            return newModule.allocate<heart::ProcessorProperty> (pp->location, pp->property);

        if (auto b = cast<heart::BinaryOperator> (old))
            return newModule.allocate<heart::BinaryOperator> (b->location,
                                                              getRemappedExpressionRef (*b->lhs),
                                                              getRemappedExpressionRef (*b->rhs),
                                                              b->operation,
                                                              cloneType (b->destType));

        if (auto u = cast<heart::UnaryOperator> (old))
            return newModule.allocate<heart::UnaryOperator> (u->location, getRemappedExpressionRef (*u->source), u->operation);

        if (auto t = cast<heart::TypeCast> (old))
            return newModule.allocate<heart::TypeCast> (t->location, getRemappedExpressionRef (*t->source), cloneType (t->destType));

        if (auto f = cast<heart::PureFunctionCall> (old))
            return clone (*f);

        if (auto f = cast<heart::PlaceholderFunctionCall> (old))
            return clone (*f);

        if (auto v = cast<heart::Variable> (old))
            return getRemappedVariable (*v);

        if (auto s = cast<heart::SubElement> (old))
            return cloneSubElement (*s);

        SOUL_ASSERT_FALSE;
        return {};
    }

    heart::Variable& getRemappedVariable (heart::Variable& old)
    {
        auto v = variableMappings[old];
        return v == nullptr ? cloneVariable (old) : *v;
    }

    heart::Function& getRemappedFunction (heart::Function& old)
    {
        auto f = functionMappings[old];
        SOUL_ASSERT (f != nullptr);
        return *f;
    }

    heart::InputDeclaration& getRemappedInput (heart::InputDeclaration& old)
    {
        auto io = inputMappings[old];
        SOUL_ASSERT (io != nullptr);
        return *io;
    }

    heart::OutputDeclaration& getRemappedOutput (heart::OutputDeclaration& old)
    {
        auto io = outputMappings[old];
        SOUL_ASSERT (io != nullptr);
        return *io;
    }

    static Type cloneType (StructMappings& structMappings, const Type& t)
    {
        if (t.isStruct())
        {
            auto s = structMappings[t.getStruct().get()];
            SOUL_ASSERT (s != nullptr);
            return Type::createStruct (*s).withConstAndRefFlags (t.isConst(), t.isReference());
        }

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
        SOUL_ASSERT (inputMappings[old] == nullptr);
        inputMappings[old] = io;
        io.name = newModule.allocator.get (old.name);
        io.index = old.index;
        io.kind = old.kind;
        io.sampleTypes = cloneTypes (old.sampleTypes);
        io.annotation = old.annotation;
        io.arraySize = old.arraySize;
        return io;
    }

    heart::OutputDeclaration& clone (const heart::OutputDeclaration& old)
    {
        auto& io = newModule.allocate<heart::OutputDeclaration> (old.location);
        SOUL_ASSERT (outputMappings[old] == nullptr);
        outputMappings[old] = io;
        io.name = newModule.allocator.get (old.name);
        io.index = old.index;
        io.kind = old.kind;
        io.sampleTypes = cloneTypes (old.sampleTypes);
        io.annotation = old.annotation;
        io.arraySize = old.arraySize;
        return io;
    }

    heart::Connection& clone (const heart::Connection& old)
    {
        auto& c = newModule.allocate<heart::Connection> (old.location);
        c.interpolationType = old.interpolationType;
        c.sourceProcessor   = getRemappedProcessorInstance (old.sourceProcessor);
        c.sourceChannel     = old.sourceChannel;
        c.destProcessor     = getRemappedProcessorInstance (old.destProcessor);
        c.destChannel       = old.destChannel;
        c.delayLength       = old.delayLength;
        return c;
    }

    heart::ProcessorInstancePtr getRemappedProcessorInstance (heart::ProcessorInstancePtr old)
    {
        if (old == nullptr)
            return {};

        return processorInstanceMappings[old];
    }

    heart::ProcessorInstance& clone (const heart::ProcessorInstance& old)
    {
        auto& p = newModule.allocate<heart::ProcessorInstance>();
        SOUL_ASSERT (processorInstanceMappings[old] == nullptr);
        processorInstanceMappings[old] = p;
        p.instanceName = old.instanceName;
        p.sourceName = old.sourceName;
        p.specialisationArgs = old.specialisationArgs;
        p.clockMultiplier = old.clockMultiplier;
        p.clockDivider = old.clockDivider;
        p.arraySize = old.arraySize;
        return p;
    }

    heart::Variable& cloneVariable (const heart::Variable& old)
    {
        SOUL_ASSERT (variableMappings[old] == nullptr);
        auto& v = newModule.allocate<heart::Variable> (old.location, cloneType (old.type),
                                                       newModule.allocator.get (old.name),
                                                       old.role);
        v.annotation = old.annotation;
        variableMappings[old] = v;
        return v;
    }

    heart::SubElement& cloneSubElement (const heart::SubElement& old)
    {
        auto& s = newModule.allocate<heart::SubElement> (old.location,
                                                         getRemappedExpressionRef (*old.parent),
                                                         old.fixedStartIndex,
                                                         old.fixedEndIndex);

        s.dynamicIndex = getRemappedExpression (old.dynamicIndex);
        s.suppressWrapWarning = old.suppressWrapWarning;
        s.isRangeTrusted = old.isRangeTrusted;
        return s;
    }

    StructurePtr createStructPlaceholder (const Structure& old)
    {
        SOUL_ASSERT (structMappings[&old] == nullptr);
        StructurePtr s (new Structure (old));
        structMappings[&old] = s;
        return s;
    }

    void populateClonedStruct (const Structure& old)
    {
        auto s = structMappings[&old];
        SOUL_ASSERT (s != nullptr);

        for (auto& m : s->members)
            m.type = cloneType (m.type);
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
        return newModule.allocate<heart::Branch> (getRemappedBlock (*old.target));
    }

    heart::BranchIf& clone (const heart::BranchIf& old)
    {
        return newModule.allocate<heart::BranchIf> (getRemappedExpressionRef (*old.condition),
                                                    getRemappedBlock (*old.targets[0]),
                                                    getRemappedBlock (*old.targets[1]));
    }

    heart::ReturnVoid& clone (const heart::ReturnVoid&)
    {
        return newModule.allocate<heart::ReturnVoid>();
    }

    heart::ReturnValue& clone (const heart::ReturnValue& old)
    {
        return newModule.allocate<heart::ReturnValue> (getRemappedExpressionRef (*old.returnValue));
    }

    heart::AssignFromValue& clone (const heart::AssignFromValue& old)
    {
        return newModule.allocate<heart::AssignFromValue> (getRemappedExpressionRef (*old.target),
                                                           getRemappedExpressionRef (*old.source));
    }

    heart::FunctionCall& clone (const heart::FunctionCall& old)
    {
        auto& fc = newModule.allocate<heart::FunctionCall> (getRemappedExpression (old.target),
                                                            getRemappedFunction (old.getFunction()));

        for (auto& arg : old.arguments)
            fc.arguments.push_back (getRemappedExpression (arg));

        return fc;
    }

    heart::PureFunctionCall& clone (const heart::PureFunctionCall& old)
    {
        auto& fc = newModule.allocate<heart::PureFunctionCall> (old.location, getRemappedFunction (old.function));

        for (auto& arg : old.arguments)
            fc.arguments.push_back (getRemappedExpression (arg));

        return fc;
    }

    heart::PlaceholderFunctionCall& clone (const heart::PlaceholderFunctionCall& old)
    {
        auto& fc = newModule.allocate<heart::PlaceholderFunctionCall> (old.location, old.name, cloneType (old.returnType));

        for (auto& arg : old.arguments)
            fc.arguments.push_back (getRemappedExpression (arg));

        return fc;
    }

    heart::ReadStream& clone (const heart::ReadStream& old)
    {
        return newModule.allocate<heart::ReadStream> (getRemappedExpressionRef (*old.target),
                                                      getRemappedInput (*old.source));
    }

    heart::WriteStream& clone (const heart::WriteStream& old)
    {
        return newModule.allocate<heart::WriteStream> (getRemappedOutput (*old.target),
                                                       getRemappedExpression (old.element),
                                                       getRemappedExpressionRef (*old.value));
    }

    heart::AdvanceClock& clone (const heart::AdvanceClock&)
    {
        return newModule.allocate<heart::AdvanceClock>();
    }

    heart::Function& createNewFunctionObject (const heart::Function& old)
    {
        auto& f = newModule.allocate<heart::Function>();
        functionMappings[old] = f;
        return f;
    }

    void clone (heart::Function& f, const heart::Function& old)
    {
        blockMappings.clear();

        f.location = old.location;
        f.returnType = cloneType (old.returnType);
        f.name = newModule.allocator.get (old.name);
        f.isRunFunction = old.isRunFunction;
        f.isEventFunction = old.isEventFunction;
        f.isInitFunction = old.isInitFunction;
        f.intrinsic = old.intrinsic;
        f.isExported = old.isExported;
        f.hasNoBody = old.hasNoBody;
        f.annotation = old.annotation;

        for (auto& p : old.parameters)
            f.parameters.push_back (cloneVariable (*p));

        for (auto& b : old.blocks)
            f.blocks.push_back (createNewBlock (*b));

        for (size_t i = 0; i < f.blocks.size(); ++i)
            clone (*f.blocks[i], *old.blocks[i]);
    }
};

}
