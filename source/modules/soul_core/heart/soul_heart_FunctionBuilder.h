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
    Helper class to build HEART blocks
*/
struct BlockBuilder
{
    BlockBuilder (Module& m) : module (m) {}

    BlockBuilder (Module& m, heart::Block& block) : module (m), currentBlock (block)
    {
        lastStatementInCurrentBlock = block.statements.getLast();
    }

    virtual ~BlockBuilder() {}

    template <typename StringType>
    Identifier createIdentifier (StringType&& name)                     { return module.allocator.get (name); }
    Identifier createIdentifier (const char* prefix, uint32_t index)    { return createIdentifier (prefix + std::to_string (index)); }

    heart::Constant& createConstant (Value v)                   { return module.allocator.allocateConstant (std::move (v)); }

    template <typename IntType>
    heart::Constant& createConstantInt32 (IntType intValue)     { return createConstant (Value::createInt32 (intValue)); }

    template <typename IntType>
    heart::Constant& createConstantInt64 (IntType intValue)     { return createConstant (Value::createInt64 (intValue)); }

    heart::Constant& createZeroInitialiser (const Type& type)   { return module.allocator.allocateZeroInitialiser (type); }

    template <typename StringType>
    heart::Variable& createVariable (Type type, StringType&& name, heart::Variable::Role role)
    {
        return module.allocate<heart::Variable> (CodeLocation(), std::move (type), createIdentifier (name), role);
    }

    heart::Variable& createRegisterVariable (Type type)
    {
        return createVariable (std::move (type), Identifier(), heart::Variable::Role::constant);
    }

    heart::Variable& createRegisterVariable (Type type, std::string name)
    {
        return createVariable (std::move (type), createIdentifier (std::move (name)), heart::Variable::Role::constant);
    }

    heart::Variable& createRegisterVariable (heart::Expression& value)
    {
        auto& v = createRegisterVariable (value.getType());
        addAssignment (v, value);
        return v;
    }

    heart::Variable& createMutableLocalVariable (Type type, std::string name)
    {
        return createVariable (std::move (type), createIdentifier (std::move (name)), heart::Variable::Role::mutableLocal);
    }

    heart::Variable& createMutableLocalVariable (Type type)
    {
        return createVariable (std::move (type), Identifier(), heart::Variable::Role::mutableLocal);
    }

    heart::Variable& createRegisterVariable (heart::Expression& value, std::string name)
    {
        auto& v = createRegisterVariable (value.getType(), name);
        addAssignment (v, value);
        return v;
    }

    heart::SubElement& createFixedSubElement (heart::Expression& parent, const SubElementPath& path)
    {
        auto indexes = path.getPath();
        SOUL_ASSERT (! indexes.empty());

        auto i = indexes.begin();
        pool_ptr<heart::SubElement> result (module.allocate<heart::SubElement> (parent.location, parent, *i++));

        while (i != indexes.end())
            result = module.allocate<heart::SubElement> (result->location, *result, *i++);

        return *result;
    }

    heart::SubElement& createSubElementSlice (CodeLocation l, heart::Expression& parent, size_t start, size_t end)
    {
        if (end == start + 1)
            return createFixedSubElement (parent, start);

        return module.allocate<heart::SubElement> (std::move (l), parent, start, end);
    }

    heart::SubElement& createTrustedDynamicSubElement (heart::Expression& parent, heart::Expression& index)
    {
        return createDynamicSubElement ({}, parent, index, true, false);
    }

    heart::SubElement& createDynamicSubElement (CodeLocation l, heart::Expression& parent, heart::Expression& index,
                                                bool isTrusted, bool suppressWrapWarning)
    {
        auto& s = module.allocate<heart::SubElement> (std::move (l), parent, index);
        s.isRangeTrusted = isTrusted;
        s.suppressWrapWarning = suppressWrapWarning;
        return s;
    }

    heart::Expression& createCast (CodeLocation l, heart::Expression& source, const Type& destType)
    {
        return module.allocate<heart::TypeCast> (std::move (l), source, destType);
    }

    heart::Expression& createCastIfNeeded (heart::Expression& source, const Type& destType)
    {
        return createCastIfNeeded (module, source, destType);
    }

    static heart::Expression& createCastIfNeeded (Module& m, heart::Expression& source, const Type& destType)
    {
        if (destType.isIdentical (source.getType()))
            return source;

        return m.allocate<heart::TypeCast> (source.location, source, destType);
    }

    heart::Expression& createUnaryOp (CodeLocation l, heart::Expression& source, UnaryOp::Op op)
    {
        return module.allocate<heart::UnaryOperator> (std::move (l), source, op);
    }

    heart::Expression& createBinaryOp (CodeLocation l, heart::Expression& lhs, heart::Expression& rhs,
                                       BinaryOp::Op op, const Type& resultType)
    {
        return module.allocate<heart::BinaryOperator> (std::move (l), lhs, rhs, op, resultType);
    }

    heart::Expression& createComparisonOp (heart::Expression& lhs, heart::Expression& rhs, BinaryOp::Op op)
    {
        return createBinaryOp ({}, lhs, rhs, op, PrimitiveType::bool_);
    }

    heart::Expression& createEqualsOp (heart::Expression& lhs, heart::Expression& rhs)
    {
        return createComparisonOp (lhs, rhs, BinaryOp::Op::equals);
    }

    virtual void addStatement (heart::Statement& s)
    {
        SOUL_ASSERT (currentBlock != nullptr);
        SOUL_ASSERT (s.nextObject == nullptr);
        lastStatementInCurrentBlock = currentBlock->statements.insertAfter (lastStatementInCurrentBlock, s);
    }

    template <typename Type, typename... Args>
    void createStatement (Args&&... args)   { addStatement (module.allocate<Type> (std::forward<Args> (args)...)); }

    void addAssignment (heart::Expression& dest, heart::Expression& source)
    {
        createStatement<heart::AssignFromValue> (CodeLocation(), dest, source);
    }

    void addAssignment (heart::Expression& dest, Value value)
    {
        addAssignment (dest, createConstant (value));
    }

    void addZeroAssignment (heart::Expression& dest)
    {
        addAssignment (dest, createZeroInitialiser (dest.getType()));
    }

    void addCastOrAssignment (heart::Expression& dest, heart::Expression& source)
    {
        addAssignment (dest, createCastIfNeeded (source, dest.getType()));
    }

    template <typename SourceArray>
    void assignSumOfValues (heart::Expression& dest, const SourceArray& values)
    {
        if (values.empty())
        {
            addZeroAssignment (dest);
            return;
        }

        auto i = values.begin();
        pool_ptr<heart::Expression> total (*i++);
        auto type = dest.getType();

        while (i != values.end())
            total = createBinaryOp (CodeLocation(), *total, **i++, BinaryOp::Op::add, type);

        addAssignment (dest, *total);
    }

    heart::Expression& createIntegerChangedByOne (heart::Expression& v, BinaryOp::Op op)
    {
        const auto& type = v.getType();
        SOUL_ASSERT (type.isInteger());
        auto& one = createConstant (Value::createInt32 (1).castToTypeExpectingSuccess (type));
        return createBinaryOp (v.location, v, one, op, type);
    }

    void changeIntegerByOne (heart::Expression& dest, BinaryOp::Op op)
    {
        addAssignment (dest, createIntegerChangedByOne (dest, op));
    }

    void incrementValue (heart::Expression& dest)
    {
        changeIntegerByOne (dest, BinaryOp::Op::add);
    }

    void decrementValue (heart::Expression& dest)
    {
        changeIntegerByOne (dest, BinaryOp::Op::subtract);
    }

    void addFunctionCall (heart::Function& function, std::initializer_list<pool_ptr<heart::Expression>> args)
    {
        addFunctionCall (nullptr, function, args);
    }

    void addFunctionCall (pool_ptr<heart::Expression> dest, heart::Function& function, std::initializer_list<pool_ptr<heart::Expression>> args)
    {
        auto& call = module.allocate<heart::FunctionCall> (CodeLocation(), dest, function);
        call.arguments.reserve (args.size());

        for (auto& a : args)
            call.arguments.push_back (a);

        SOUL_ASSERT (call.arguments.size() == function.parameters.size());
        addStatement (call);
    }

    void addFunctionCall (pool_ptr<heart::Expression> dest, heart::Function& function, heart::FunctionCall::ArgListType&& args)
    {
        auto& call = module.allocate<heart::FunctionCall> (CodeLocation(), dest, function);
        call.arguments = std::move (args);
        SOUL_ASSERT (call.arguments.size() == function.parameters.size());
        addStatement (call);
    }

    heart::PlaceholderFunctionCall& createMinInt32 (heart::Expression& a, heart::Expression& b)
    {
        SOUL_ASSERT (a.getType().isInteger32() && b.getType().isInteger32());
        auto& call = module.allocate<heart::PlaceholderFunctionCall> (a.location, getFullyQualifiedIntrinsicName (IntrinsicType::min), PrimitiveType::int32);
        call.arguments.push_back (a);
        call.arguments.push_back (b);
        return call;
    }

    heart::PlaceholderFunctionCall& createWrapInt32 (heart::Expression& n, heart::Expression& range)
    {
        return createWrapInt32 (module, n, range);
    }

    static heart::PlaceholderFunctionCall& createWrapInt32 (Module& m, heart::Expression& n, heart::Expression& range)
    {
        SOUL_ASSERT (n.getType().isInteger32() && n.getType().isPrimitiveInteger() && range.getType().isInteger32());
        auto& call = m.allocate<heart::PlaceholderFunctionCall> (n.location, getFullyQualifiedIntrinsicName (IntrinsicType::wrap), PrimitiveType::int32);
        call.arguments.push_back (n);
        call.arguments.push_back (range);
        return call;
    }

    void addReadStream (CodeLocation l, heart::Expression& dest, heart::InputDeclaration& src)
    {
        auto sourceType = src.getSingleSampleType();

        if (dest.getType().isIdentical (sourceType))
            return createStatement<heart::ReadStream> (std::move (l), dest, src);

        auto& temp = createRegisterVariable (sourceType);
        createStatement<heart::ReadStream> (l, temp, src);
        addAssignment (dest, createCast (l, temp, dest.getType()));
    }

    void addWriteStream (CodeLocation l, heart::OutputDeclaration& output, pool_ptr<heart::Expression> element, heart::Expression& value)
    {
        createStatement<heart::WriteStream> (std::move (l), output, element, value);
    }

    void setTerminator (heart::Terminator& t)
    {
        currentBlock->terminator = t;
    }

    void setReturnTerminator()
    {
        setTerminator (module.allocate<heart::ReturnVoid>());
    }

    void setReturnTerminator (heart::Expression& value)
    {
        setTerminator (module.allocate<heart::ReturnValue> (value));
    }

    void setBranchTerminator (heart::Block& target)
    {
        setTerminator (module.allocate<heart::Branch> (target));
    }

    void setBranchIfTerminator (heart::Expression& condition, heart::Block& trueBranch, heart::Block& falseBranch)
    {
        SOUL_ASSERT (std::addressof (trueBranch) != std::addressof (falseBranch));
        setTerminator (module.allocate<heart::BranchIf> (condition, trueBranch, falseBranch));
    }

    Module& module;
    pool_ptr<heart::Block> currentBlock;
    LinkedList<heart::Statement>::Iterator lastStatementInCurrentBlock;
};


//==============================================================================
/**
    Helper class to build HEART functions
*/
struct FunctionBuilder  : public BlockBuilder
{
    FunctionBuilder (Module& m) : BlockBuilder (m) {}

    ~FunctionBuilder() override
    {
        SOUL_ASSERT (soul::inExceptionHandler() || currentBlock == nullptr || currentBlock->isTerminated());
    }

    static heart::Function& getOrCreateFunction (Module& m, const std::string& name, Type returnType,
                                                 std::function<void(FunctionBuilder&)> buildFunction)
    {
        for (auto& f : m.functions)
            if (f->name == name)
                return f;

        return createFunction (m, name, std::move (returnType), std::move (buildFunction));
    }

    static heart::Function& createEmptyFunction (Module& m, const std::string& name, Type returnType)
    {
        auto& fn = m.allocate<heart::Function>();
        fn.name = m.allocator.get (name);
        fn.returnType = std::move (returnType);
        fn.hasNoBody = true;
        m.functions.push_back (fn);

        return fn;
    }

    static void populateFunctionBody (Module& m, heart::Function& fn, std::function<void(FunctionBuilder&)> buildFunction)
    {
        SOUL_ASSERT (fn.hasNoBody);

        fn.hasNoBody = false;

        FunctionBuilder builder (m);
        builder.beginFunction (fn);
        buildFunction (builder);
        builder.endFunction();

        if (! fn.hasNoBody)
        {
            bool terminationOK = builder.checkFunctionBlocksForTermination();
            SOUL_ASSERT (terminationOK); ignoreUnused (terminationOK);
        }
    }

    static heart::Function& createFunction (Module& m, const std::string& name, Type returnType,
                                            std::function<void(FunctionBuilder&)> buildFunction)
    {
        auto& fn = createEmptyFunction (m, name, returnType);
        populateFunctionBody (m, fn, buildFunction);

        return fn;
    }

    void beginFunction (heart::Function& f)
    {
        currentFunction = f;
        currentBlock = {};
        lastStatementInCurrentBlock = {};
        blockIndex = 0;
        localVarIndex = 0;
    }

    void endFunction()
    {
        if (currentFunction != nullptr)
            if (! currentFunction->blocks.empty())
                currentFunction->blocks.front()->doNotOptimiseAway = true;

        currentBlock = {};
        lastStatementInCurrentBlock = {};
    }

    bool checkFunctionBlocksForTermination()
    {
        auto& blocks = currentFunction->blocks;

        if (blocks.empty())
            ensureBlockIsReady();

        for (size_t i = 0; i < blocks.size(); ++i)
        {
            auto b = blocks[i];

            if (! b->isTerminated())
            {
                if (i == blocks.size() - 1)
                {
                    if (! currentFunction->returnType.isVoid())
                        return false;

                    b->terminator = module.allocate<heart::ReturnVoid>();
                }
                else
                {
                    b->terminator = module.allocate<heart::Branch> (*blocks[i + 1]);
                }
            }
        }

        return true;
    }

    void addParameter (heart::Variable& v)
    {
        currentFunction->parameters.push_back (v);
    }

    heart::Variable& addParameter (const std::string& name, const Type& type)
    {
        auto& v = module.allocate<heart::Variable> (CodeLocation(), type, module.allocator.get (name),
                                                    heart::Variable::Role::parameter);
        addParameter (v);
        return v;
    }

    heart::Variable& addStateParameter (const std::string& name, const Type& type)
    {
        auto& param = addParameter (name, type);
        currentFunction->stateParameter = param;

        return param;
    }

    heart::Block& createBlock (Identifier name)
    {
        return module.allocate<heart::Block> (name);
    }

    heart::Block& createBlock (const char* prefix, uint32_t index)
    {
        return createBlock (createIdentifier (prefix, index));
    }

    heart::Block& createBlock (const char* name)
    {
        return createBlock (module.allocator.get (name));
    }

    heart::Block& createNewBlock()
    {
        return createBlock ("@block_", blockIndex++);
    }

    void ensureBlockIsReady()
    {
        if (currentBlock == nullptr)
            beginBlock (createNewBlock());
    }

    void beginBlock (pool_ptr<heart::Block> b)
    {
        SOUL_ASSERT (currentFunction != nullptr);
        SOUL_ASSERT (currentBlock != b);

        if (b != nullptr && currentBlock != nullptr && ! currentBlock->isTerminated())
            return addBranch (*b, b);

        currentBlock = b;

        if (b != nullptr)
        {
            SOUL_ASSERT (heart::Utilities::findBlock (*currentFunction, b->name) == nullptr);
            currentFunction->blocks.push_back (b);
            lastStatementInCurrentBlock = b->statements.getLast();
        }
    }

    void addStatement (heart::Statement& s) override
    {
        ensureBlockIsReady();
        BlockBuilder::addStatement (s);
    }

    void addTerminatorStatement (heart::Terminator& t, pool_ptr<heart::Block> subsequentBlock)
    {
        ensureBlockIsReady();
        setTerminator (t);
        beginBlock (subsequentBlock);
    }

    void addReturn()
    {
        addTerminatorStatement (module.allocate<heart::ReturnVoid>(), nullptr);
    }

    void addReturn (heart::Expression& value)
    {
        addTerminatorStatement (module.allocate<heart::ReturnValue> (value), nullptr);
    }

    void addBranch (heart::Block& target, pool_ptr<heart::Block> subsequentBlock)
    {
        addTerminatorStatement (module.allocate<heart::Branch> (target), subsequentBlock);
    }

    void addBranchIf (heart::Expression& condition, heart::Block& trueBranch,
                      heart::Block& falseBranch, pool_ptr<heart::Block> subsequentBlock)
    {
        addTerminatorStatement (module.allocate<heart::BranchIf> (condition, trueBranch, falseBranch),
                                subsequentBlock);
    }

    void addAdvance (CodeLocation l)
    {
        createStatement<heart::AdvanceClock> (std::move (l));
    }

    void createIfElse (const std::string& blockNamePrefix,
                       heart::Expression& condition,
                       const std::function<void(FunctionBuilder&)>& createTrueBranch,
                       const std::function<void(FunctionBuilder&)>& createFalseBranch)
    {
        auto& conditionTrueBlock   = createBlock (module.allocator.get (blockNamePrefix + "_true"));
        auto& conditionFalseBlock  = createBlock (module.allocator.get (blockNamePrefix + "_false"));
        auto& continueBlock        = createBlock (module.allocator.get (blockNamePrefix + "_continue"));

        addBranchIf (condition, conditionTrueBlock, conditionFalseBlock, conditionTrueBlock);
        createTrueBranch (*this);
        addBranch (continueBlock, conditionFalseBlock);
        createFalseBranch (*this);
        addBranch (continueBlock, continueBlock);
    }

    void incrementAndWrap (heart::Expression& dest, heart::Expression& source, size_t limit)
    {
        SOUL_ASSERT (limit > 0);

        auto& plusOne = createIntegerChangedByOne (source, BinaryOp::Op::add);

        if (isPowerOf2 (limit) && limit < (1u << 30))
        {
            addAssignment (dest, createBinaryOp (source.location,
                                                 plusOne, createConstantInt32 (limit - 1),
                                                 BinaryOp::Op::bitwiseAnd, source.getType()));
        }
        else
        {
            auto& inRangeBlock   = createNewBlock();
            auto& wrappedBlock   = createNewBlock();
            auto& continueBlock  = createNewBlock();

            addBranchIf (createEqualsOp (plusOne, createConstantInt32 (limit)),
                         wrappedBlock, inRangeBlock, inRangeBlock);

            addAssignment (dest, plusOne);
            addBranch (continueBlock, wrappedBlock);
            addZeroAssignment (dest);
            addBranch (continueBlock, continueBlock);
        }
    }

    pool_ptr<heart::Function> currentFunction;
    uint32_t blockIndex = 0, localVarIndex = 0;
};

} // namespace soul
