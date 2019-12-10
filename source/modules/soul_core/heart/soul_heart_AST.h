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

class Module;

//==============================================================================
struct heart
{
    #define SOUL_HEART_OBJECTS(X) \
        X(InputDeclaration) \
        X(OutputDeclaration) \
        X(Connection) \
        X(ProcessorInstance) \
        X(Function) \
        X(Block) \
        X(Expression) \
        X(Variable) \
        X(SubElement) \
        X(Constant) \
        X(TypeCast) \
        X(UnaryOperator) \
        X(BinaryOperator) \
        X(PureFunctionCall) \
        X(PlaceholderFunctionCall) \
        X(ProcessorProperty) \

    #define SOUL_HEART_STATEMENTS(X) \
        X(AssignFromValue) \
        X(FunctionCall) \
        X(ReadStream) \
        X(WriteStream) \
        X(AdvanceClock) \

    #define SOUL_HEART_TERMINATORS(X) \
        X(Branch) \
        X(BranchIf) \
        X(ReturnVoid) \
        X(ReturnValue) \

    struct Object;
    struct Statement;
    struct VariableOrValue;
    struct Assignment;
    struct Terminator;

    #define SOUL_PREDECLARE_TYPE(Type)     struct Type; using Type ## Ptr = pool_ptr<Type>;
    SOUL_HEART_OBJECTS (SOUL_PREDECLARE_TYPE)
    SOUL_HEART_STATEMENTS (SOUL_PREDECLARE_TYPE)
    SOUL_HEART_TERMINATORS (SOUL_PREDECLARE_TYPE)
    #undef SOUL_PREDECLARE_TYPE

    using ObjectPtr        = pool_ptr<Object>;
    using StatementPtr     = pool_ptr<Statement>;
    using AssignmentPtr    = pool_ptr<Assignment>;
    using TerminatorPtr    = pool_ptr<Terminator>;

    struct Parser;
    struct Printer;
    struct Checker;
    struct Utilities;

    static constexpr const char* getRunFunctionName()               { return "run"; }
    static constexpr const char* getInitFunctionName()              { return "_soul_init"; }
    static constexpr const char* getUserInitFunctionName()          { return "init"; }
    static constexpr const char* getGenericSpecialisationNameTag()  { return "_specialised_"; }

    static std::string getExternalEventFunctionName (const std::string& endpointName)           { return "_external_event_" + endpointName; }
    static std::string getEventFunctionName (const std::string& endpointName, const Type& t)    { return "_" + endpointName + "_" + t.withConstAndRefFlags (false, false).getShortIdentifierDescription(); }
    static std::string getEventFunctionName (const heart::InputDeclaration&  i, const Type& t)  { return getEventFunctionName (i.name.toString(), t); }
    static std::string getEventFunctionName (const heart::OutputDeclaration& o, const Type& t)  { return getEventFunctionName (o.name.toString(), t); }

    //==============================================================================
    struct Allocator
    {
        Allocator() = default;
        Allocator (const Allocator&) = delete;
        Allocator (Allocator&&) = default;

        template <typename Type, typename... Args>
        Type& allocate (Args&&... args)                       { return pool.allocate<Type> (std::forward<Args> (args)...); }

        template <typename ValueType>
        Constant& allocateConstant (ValueType value)          { return allocate<heart::Constant> (CodeLocation(), value); }
        Constant& allocateConstant (Value value)              { return allocate<heart::Constant> (CodeLocation(), std::move (value)); }
        Constant& allocateZeroInitialiser (const Type& type)  { return allocateConstant (Value::zeroInitialiser (type)); }

        template <typename Type>
        Identifier get (const Type& newString)  { return identifiers.get (newString); }

        PoolAllocator pool;
        Identifier::Pool identifiers;
    };

    //==============================================================================
    struct Object
    {
        Object() = default;
        Object (CodeLocation l) : location (std::move (l)) {}
        Object (const Object&) = delete;
        virtual ~Object() {}

        CodeLocation location;
    };

    //==============================================================================
    struct IODeclaration : public Object
    {
        IODeclaration (CodeLocation l) : Object (std::move (l)) {}

        Identifier name;
        uint32_t index = 0;
        EndpointKind kind;
        std::vector<Type> sampleTypes;
        uint32_t arraySize = 1;
        Annotation annotation;

        bool isEventEndpoint() const      { return isEvent (kind); }
        bool isStreamEndpoint() const     { return isStream (kind); }
        bool isValueEndpoint() const      { return isValue (kind); }
        bool isNullEndpoint() const       { return isNull (kind); }

        bool supportsSampleType (const Type& t) const
        {
            for (auto& sampleType : sampleTypes)
            {
                auto sampleArrayType = getSampleArrayType (sampleType);

                if (TypeRules::canSilentlyCastTo (sampleArrayType, t))
                    return true;
            }

            return false;
        }

        bool supportsElementSampleType (const Type& t) const
        {
            for (auto& sampleType : sampleTypes)
                if (TypeRules::canSilentlyCastTo (sampleType, t))
                    return true;

            return false;
        }

        std::vector<Type> getSampleArrayTypes() const
        {
            std::vector<Type> types;

            for (auto& sampleType : sampleTypes)
                types.push_back (getSampleArrayType (sampleType));

            return types;
        }

        Type getSupportedType (const Type& t, bool isElementAccess) const
        {
            for (auto& sampleType : sampleTypes)
            {
                auto type = isElementAccess ? sampleType : getSampleArrayType (sampleType);

                if (TypeRules::canSilentlyCastTo (type, t))
                    return getSampleArrayType (sampleType);
            }

            SOUL_ASSERT_FALSE;
            return {};
        }

        Type getSingleSampleType() const
        {
            SOUL_ASSERT (sampleTypes.size() == 1);
            return getSampleArrayType (sampleTypes[0]);
        }

        std::string getSampleTypesDescription() const
        {
            std::vector<std::string> sampleDescriptions;

            for (auto& sampleType : sampleTypes)
                sampleDescriptions.push_back (sampleType.getDescription());

            return soul::joinStrings (sampleDescriptions, ", ");
        }

    private:
        Type getSampleArrayType (Type t) const
        {
            return arraySize == 1 ? t : t.createArray (arraySize);
        }
    };

    //==============================================================================
    struct InputDeclaration  : public IODeclaration
    {
        InputDeclaration (CodeLocation l) : IODeclaration (std::move (l)) {}

        std::string getEventFunctionName() const                { SOUL_ASSERT (isEventEndpoint());  return getExternalEventFunctionName (name.toString()); }
        std::string getFramesTillNextEventFunctionName() const  { SOUL_ASSERT (isEventEndpoint());  return "_setFramesTillNextEvent_" + name.toString(); }
        std::string getAddFramesFunctionName() const            { SOUL_ASSERT (isStreamEndpoint()); return "_addFramesToInput_" + name.toString(); }
        std::string getAddSparseFramesFunctionName() const      { SOUL_ASSERT (isStreamEndpoint()); return "_addSparseFramesToInput_" + name.toString(); }
        std::string getAvailableNumFramesFunctionName() const   { SOUL_ASSERT (isStreamEndpoint()); return "_getAvailableNumFrames_" + name.toString(); }
        std::string getSetValueFunctionName() const             { SOUL_ASSERT (isValueEndpoint());  return "_setValue_" + name.toString(); }

        EndpointDetails getDetails() const
        {
            auto sampleType = getSingleSampleType();
            return { "in:" + name.toString(), name, kind, sampleType, (uint32_t) sampleType.getPackedSizeInBytes(), annotation };
        }
    };

    //==============================================================================
    struct OutputDeclaration  : public IODeclaration
    {
        OutputDeclaration (CodeLocation l) : IODeclaration (std::move (l)) {}

        std::string getEventFunctionName() const                { SOUL_ASSERT (isEventEndpoint());  return getExternalEventFunctionName (name.toString()); }
        std::string getReadFramesFunctionName() const           { SOUL_ASSERT (isStreamEndpoint()); return "_readFramesFromOutput_" + name.toString(); }
        std::string getGetEventsFunctionName() const            { SOUL_ASSERT (isEventEndpoint());  return "_getEventsFromOutput_" + name.toString(); }

        EndpointDetails getDetails() const
        {
            auto sampleType = getSingleSampleType();
            return { "out:" + name.toString(), name, kind, sampleType, (uint32_t) sampleType.getPackedSizeInBytes(), annotation };
        }
    };

    //==============================================================================
    struct SpecialisationArgument
    {
        Value value;
        Type type;
        std::string processorName;

        std::string toString() const
        {
            if (! processorName.empty())
                return processorName;

            if (type.isValid())
                return type.getDescription();

            return value.getDescription();
        }
    };

    struct ProcessorInstance  : public Object
    {
        std::string instanceName, sourceName;
        std::vector<SpecialisationArgument> specialisationArgs;
        int64_t clockMultiplier = 1;
        int64_t clockDivider = 1;
        uint32_t arraySize = 1;

        bool hasClockMultiplier() const    { return clockMultiplier != 1; }
        bool hasClockDivider() const       { return clockDivider != 1; }
        double getClockRatio() const       { return static_cast<double> (clockMultiplier) / static_cast<double> (clockDivider); }
    };

    struct Connection  : public Object
    {
        Connection (CodeLocation l) : Object (std::move (l)) {}

        InterpolationType interpolationType = InterpolationType::none;
        ProcessorInstancePtr sourceProcessor, destProcessor;
        std::string sourceChannel, destChannel;
        int64_t delayLength = 0;
    };

    template <typename Thrower>
    static int64_t getClockRatioFromValue (Thrower&& errorLocation, const Value& value)
    {
        if (! value.getType().isPrimitiveInteger())
            errorLocation.throwError (Errors::ratioMustBeInteger());

        auto v = value.getAsInt64();

        if (v < 1 || v > 512)
            errorLocation.throwError (Errors::ratioOutOfRange());

        if (! isPowerOf2 (v))
            errorLocation.throwError (Errors::ratioMustBePowerOf2());

        return v;
    }

    //==============================================================================
    enum class AccessType
    {
        read,
        write,
        readWrite
    };

    using ExpressionVisitorFn = const std::function<void(ExpressionPtr&, AccessType)>&;

    struct Expression  : public Object
    {
        Expression (CodeLocation l) : Object (std::move (l)) {}

        virtual const Type& getType() const = 0;
        virtual void visitExpressions (ExpressionVisitorFn, AccessType) = 0;
        virtual bool readsVariable (VariablePtr) const = 0;
        virtual bool writesVariable (VariablePtr) const = 0;
        virtual VariablePtr getRootVariable() = 0;
        virtual Value getAsConstant() const = 0;
        virtual bool isMutable() const = 0;
        virtual bool isAssignable() const = 0;
    };

    //==============================================================================
    struct Variable  : public Expression
    {
        enum class Role
        {
            state,
            mutableLocal,
            constant,
            parameter,
            external
        };

        Variable() = delete;
        Variable (CodeLocation l, Type t, Identifier nm, Role r)  : Expression (std::move (l)), type (std::move (t)), name (nm), role (r)  {}
        Variable (CodeLocation l, Type t, Role r)  : Expression (std::move (l)), type (std::move (t)), role (r)  {}

        Type type;
        Identifier name;
        Role role;
        Annotation annotation;
        ConstantTable::Handle externalHandle = {};

        const Type& getType() const override        { return type; }
        VariablePtr getRootVariable() override      { return *this; }

        bool isAssignable() const override          { return ! (isExternal() || type.isConst()); }
        bool isState() const                        { return role == Role::state || isExternal(); }
        bool isParameter() const                    { return role == Role::parameter; }
        bool isMutable() const override             { return role != Role::constant; }
        bool isMutableLocal() const                 { return role == Role::mutableLocal; }
        bool isConstant() const                     { return role == Role::constant; }
        bool isFunctionLocal() const                { return isMutableLocal() || isConstant(); }
        bool isExternal() const                     { return role == Role::external; }
        bool isExternalToFunction() const           { return isState() || (isParameter() && type.isReference()); }

        void visitExpressions (ExpressionVisitorFn, AccessType) override    {}

        void resetUseCount()                                         { numReads = numWrites = 0; }
        bool readsVariable (VariablePtr v) const override            { return this == v; }
        bool writesVariable (VariablePtr v) const override           { return this == v; }
        Value getAsConstant() const override                         { return {}; }

        uint32_t numReads = 0, numWrites = 0;
    };

    //==============================================================================
    struct SubElement  : public Expression
    {
        SubElement() = delete;
        SubElement (CodeLocation l, ExpressionPtr v) : Expression (std::move (l)), parent (v) {}
        SubElement (CodeLocation l, ExpressionPtr v, size_t index) : Expression (std::move (l)), parent (v), fixedStartIndex (index), fixedEndIndex (index + 1) {}
        SubElement (CodeLocation l, ExpressionPtr v, size_t startIndex, size_t endIndex) : Expression (std::move (l)), parent (v), fixedStartIndex (startIndex), fixedEndIndex (endIndex) {}
        SubElement (CodeLocation l, ExpressionPtr v, ExpressionPtr elementIndex) : Expression (std::move (l)), parent (v), dynamicIndex (elementIndex) {}

        VariablePtr getRootVariable() override      { return parent->getRootVariable(); }
        bool isMutable() const override             { return parent->isMutable(); }
        bool isAssignable() const override          { return parent->isAssignable(); }
        bool isDynamic() const                      { return dynamicIndex != nullptr; }

        const Type& getType() const override
        {
            const auto& aggregateType = parent->getType();

            if (aggregateType.isStruct())
            {
                SOUL_ASSERT (! isDynamic());
                SOUL_ASSERT (isSingleElement());
                SOUL_ASSERT (fixedStartIndex < aggregateType.getStruct()->members.size());
                return aggregateType.getStruct()->members[fixedStartIndex].type;
            }

            auto sliceSize = getSliceSize();

            if (sliceSize == 1)
            {
                if (aggregateType.isPrimitive())
                    temporaryType = aggregateType;
                else
                    temporaryType = aggregateType.getElementType();

                return temporaryType;
            }

            SOUL_ASSERT (aggregateType.isArray() || aggregateType.isVector());
            SOUL_ASSERT (aggregateType.isUnsizedArray() || aggregateType.isValidArrayOrVectorRange (fixedStartIndex, fixedEndIndex));

            temporaryType = aggregateType.createCopyWithNewArraySize (sliceSize);
            return temporaryType;
        }

        void visitExpressions (ExpressionVisitorFn fn, AccessType mode) override
        {
            if (isDynamic())
            {
                dynamicIndex->visitExpressions (fn, AccessType::read);
                fn (dynamicIndex, AccessType::read);
            }

            parent->visitExpressions (fn, mode);
            fn (parent, mode);
            SOUL_ASSERT (parent != this);
        }

        bool readsVariable (VariablePtr v) const override    { return parent->readsVariable (v) || (isDynamic() && dynamicIndex->readsVariable (v)); }
        bool writesVariable (VariablePtr v) const override   { return parent->writesVariable (v); }

        bool isSingleElement() const        { return getSliceSize() == 1; }
        bool isSlice() const                { return getSliceSize() != 1; }
        size_t getSliceSize() const         { return isDynamic() ? 1 : (fixedEndIndex - fixedStartIndex); }

        Value getAsConstant() const override
        {
            auto parentValue = parent->getAsConstant();

            if (parentValue.isValid())
            {
                if (isSlice())
                    return parentValue.getSlice (fixedStartIndex, fixedEndIndex);

                if (dynamicIndex == nullptr)
                    return parentValue.getSubElement (fixedStartIndex);

                auto indexValue = dynamicIndex->getAsConstant();

                if (indexValue.isValid())
                {
                    auto index = indexValue.getAsInt64();

                    if (index >= 0)
                        return parentValue.getSubElement ((size_t) index);
                }
            }

            return {};
        }

        void optimiseDynamicIndexIfPossible()
        {
            if (dynamicIndex != nullptr)
            {
                auto constIndex = dynamicIndex->getAsConstant();

                if (constIndex.isValid())
                {
                    const auto& arrayOrVectorType = parent->getType();

                    if (arrayOrVectorType.isVector() || arrayOrVectorType.isFixedSizeArray())
                    {
                        dynamicIndex = nullptr;
                        fixedStartIndex = TypeRules::checkAndGetArrayIndex (location, constIndex, arrayOrVectorType);
                        fixedEndIndex = fixedStartIndex + 1;
                        isRangeTrusted = true;
                    }
                }
            }
        }

        ExpressionPtr parent, dynamicIndex;
        size_t fixedStartIndex = 0, fixedEndIndex = 1;
        bool isRangeTrusted = false, suppressWrapWarning = false;

    private:
        mutable Type temporaryType; // used for holding a temp copy of the value returned from getType(), to avoid that method needing to return
                                    // by-value, which would impact performance of all the other classes that call it
    };

    //==============================================================================
    struct Constant  : public Expression
    {
        Constant (CodeLocation l, Value v) : Expression (std::move (l)), value (std::move (v)) {}
        Constant (CodeLocation l, const Type& t) : Expression (std::move (l)), value (Value::zeroInitialiser (t)) {}

        const Type& getType() const override               { return value.getType(); }
        Value getAsConstant() const override               { return value; }
        bool isMutable() const override                    { return false; }
        bool isAssignable() const override                 { return false; }
        VariablePtr getRootVariable() override             { return {}; }
        bool readsVariable (VariablePtr) const override    { return false; }
        bool writesVariable (VariablePtr) const override   { return false; }
        void visitExpressions (ExpressionVisitorFn, AccessType) override {}

        Value value;
    };

    //==============================================================================
    struct TypeCast  : public Expression
    {
        TypeCast (CodeLocation l, ExpressionPtr src, const Type& type)
            : Expression (std::move (l)), source (src), destType (type)
        {
            SOUL_ASSERT (source != nullptr);
        }

        const Type& getType() const override                 { return destType; }
        VariablePtr getRootVariable() override               { SOUL_ASSERT_FALSE; return {}; }
        bool readsVariable (VariablePtr v) const override    { return source->readsVariable (v); }
        bool writesVariable (VariablePtr) const override     { return false; }
        bool isMutable() const override                      { return false; }
        bool isAssignable() const override                   { return false; }

        Value getAsConstant() const override
        {
            auto sourceValue = source->getAsConstant();

            if (sourceValue.isValid())
                return sourceValue.tryCastToType (destType);

            return {};
        }

        void visitExpressions (ExpressionVisitorFn fn, AccessType) override
        {
            source->visitExpressions (fn, AccessType::read);
            fn (source, AccessType::read);
        }

        ExpressionPtr source;
        Type destType;
    };

    //==============================================================================
    struct UnaryOperator  : public Expression
    {
        UnaryOperator (CodeLocation l, ExpressionPtr src, UnaryOp::Op op)
            : Expression (std::move (l)), source (src), operation (op)
        {
            SOUL_ASSERT (source != nullptr);
        }

        const Type& getType() const override                 { return source->getType(); }
        VariablePtr getRootVariable() override               { SOUL_ASSERT_FALSE; return {}; }
        bool readsVariable (VariablePtr v) const override    { return source->readsVariable (v); }
        bool writesVariable (VariablePtr) const override     { return false; }
        bool isMutable() const override                      { return false; }
        bool isAssignable() const override                   { return false; }

        Value getAsConstant() const override
        {
            auto sourceValue = source->getAsConstant();

            if (sourceValue.isValid())
                if (UnaryOp::apply (sourceValue, operation))
                    return sourceValue;

            return {};
        }

        void visitExpressions (ExpressionVisitorFn fn, AccessType) override
        {
            source->visitExpressions (fn, AccessType::read);
            fn (source, AccessType::read);
        }

        ExpressionPtr source;
        UnaryOp::Op operation;
    };

    //==============================================================================
    struct BinaryOperator  : public Expression
    {
        BinaryOperator (CodeLocation l, ExpressionPtr a, ExpressionPtr b, BinaryOp::Op op, const Type& resultType)
            : Expression (std::move (l)), lhs (a), rhs (b), operation (op), destType (resultType)
        {
            SOUL_ASSERT (lhs != nullptr && rhs != nullptr && destType.isValid());
        }

        const Type& getType() const override                 { return destType; }
        VariablePtr getRootVariable() override               { SOUL_ASSERT_FALSE; return {}; }
        bool readsVariable (VariablePtr v) const override    { return lhs->readsVariable (v) || rhs->readsVariable (v); }
        bool writesVariable (VariablePtr) const override     { return false; }
        bool isMutable() const override                      { return false; }
        bool isAssignable() const override                   { return false; }

        void visitExpressions (ExpressionVisitorFn fn, AccessType) override
        {
            lhs->visitExpressions (fn, AccessType::read);
            rhs->visitExpressions (fn, AccessType::read);
            fn (lhs, AccessType::read);
            fn (rhs, AccessType::read);
        }

        Value getAsConstant() const override
        {
            auto a = lhs->getAsConstant();

            if (a.isValid())
            {
                auto b = rhs->getAsConstant();

                if (b.isValid())
                    if (BinaryOp::apply (a, b, operation,
                                         [this] (CompileMessage message) { location.throwError (message); }))
                        return a;
            }

            return {};
        }

        ExpressionPtr lhs, rhs;
        BinaryOp::Op operation;
        Type destType;
    };

    //==============================================================================
    struct FunctionCallExpression  : public Expression
    {
        FunctionCallExpression (CodeLocation l) : Expression (std::move (l)) {}

        VariablePtr getRootVariable() override               { SOUL_ASSERT_FALSE; return {}; }
        bool readsVariable (VariablePtr v) const override    { return contains (arguments, v); }
        bool writesVariable (VariablePtr) const override     { return false; }
        bool isMutable() const override                      { return false; }
        bool isAssignable() const override                   { return false; }

        void visitExpressions (ExpressionVisitorFn fn, AccessType) override
        {
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                arguments[i]->visitExpressions (fn, AccessType::read);
                fn (arguments[i], AccessType::read);
            }
        }

        std::vector<ExpressionPtr> arguments;
    };

    //==============================================================================
    struct PureFunctionCall  : public FunctionCallExpression
    {
        PureFunctionCall (CodeLocation l, Function& fn)
            : FunctionCallExpression (std::move (l)), function (fn) {}

        const Type& getType() const override    { return function.returnType; }
        Value getAsConstant() const override    { return {}; }

        Function& function;
    };

    //==============================================================================
    struct PlaceholderFunctionCall  : public FunctionCallExpression
    {
        PlaceholderFunctionCall (CodeLocation l, std::string functionName, Type retType)
            : FunctionCallExpression (std::move (l)), name (std::move (functionName)), returnType (std::move (retType))
        {
            SOUL_ASSERT (! name.empty());
        }

        const Type& getType() const override    { return returnType; }
        Value getAsConstant() const override    { return {}; }

        std::string name;
        Type returnType;
    };

    //==============================================================================
    struct Function  : public Object
    {
        Type returnType;
        Identifier name;
        std::vector<VariablePtr> parameters;
        std::vector<BlockPtr> blocks;
        Annotation annotation;
        IntrinsicType intrinsicType = IntrinsicType::none;
        VariablePtr stateParameter = nullptr;

        struct FunctionType
        {
            enum Type
            {
                normal,
                event,
                run,
                systemInit,
                userInit,
                intrinsic
            };

            void setNormal()        { functionType = Type::normal; }
            void setEvent()         { SOUL_ASSERT (functionType == Type::normal); functionType = Type::event; }
            void setRun()           { SOUL_ASSERT (functionType == Type::normal); functionType = Type::run; }
            void setSystemInit()    { SOUL_ASSERT (functionType == Type::normal); functionType = Type::systemInit; }
            void setUserInit()      { SOUL_ASSERT (functionType == Type::normal); functionType = Type::userInit; }
            void setIntrinsic()     { SOUL_ASSERT (functionType == Type::normal); functionType = Type::intrinsic; }

            bool isNormal() const       { return functionType == Type::normal; }
            bool isEvent() const        { return functionType == Type::event; }
            bool isRun() const          { return functionType == Type::run; }
            bool isSystemInit() const   { return functionType == Type::systemInit; }
            bool isUserInit() const     { return functionType == Type::userInit; }
            bool isIntrinsic() const    { return functionType == Type::intrinsic; }

        private:
            Type functionType = Type::normal;
        };

        FunctionType functionType;

        bool isExported = false;
        bool hasNoBody = false;
        bool functionUseTestFlag = false;

        BlockPtr findBlockByName (const std::string& blockName) const
        {
            for (auto b : blocks)
                if (b->name == blockName)
                    return b;

            return nullptr;
        }

        void addStateParameter (VariablePtr param)
        {
            SOUL_ASSERT (! hasStateParameter());

            parameters.insert (parameters.begin(), param);
            stateParameter = param;
        }

        bool hasStateParameter() const
        {
            return stateParameter != nullptr;
        }

        bool hasSameSignature (const Function& other) const
        {
            if (name != other.name || parameters.size() != other.parameters.size())
                return false;

            for (size_t i = 0; i < parameters.size(); ++i)
                if (! parameters[i]->getType().isEqual (other.parameters[i]->getType(), Type::ignoreVectorSize1))
                    return false;

            return true;
        }

        std::string getReadableName() const
        {
            auto nm = name.toString();

            if (startsWith (nm, "_"))
            {
                auto i = nm.find (getGenericSpecialisationNameTag());

                if (i != std::string::npos && i > 0)
                    return nm.substr (1, i - 1);
            }

            return nm;
        }

        bool mayHaveSideEffects() const
        {
            for (auto& b : blocks)
                for (auto s : b->statements)
                    if (s->mayHaveSideEffects())
                        return true;

            return false;
        }

        void rebuildBlockPredecessors()
        {
            for (auto& b : blocks)
                b->predecessors.clear();

            for (auto& b : blocks)
            {
                // terminator is not normally null, but could be if we are trying to resolve
                // a non-terminated function
                if (auto t = b->terminator)
                    for (auto dest : t->getDestinationBlocks())
                        dest->predecessors.push_back (b);
            }
        }

        void rebuildVariableUseCounts()
        {
            for (auto& p : parameters)
                p->resetUseCount();

            visitExpressions ([] (ExpressionPtr& value, AccessType)
                              {
                                  if (auto v = cast<Variable> (value))
                                      v->resetUseCount();
                              });

            visitExpressions ([] (ExpressionPtr& value, AccessType mode)
                              {
                                  if (auto v = cast<Variable> (value))
                                  {
                                      if (mode != AccessType::write) v->numReads++;
                                      if (mode != AccessType::read)  v->numWrites++;
                                  }
                              });
        }

        void visitExpressions (ExpressionVisitorFn fn)
        {
            for (auto b : blocks)
                b->visitExpressions (fn);
        }

        void visitAllStatements (std::function<void(Statement&)> fn)
        {
            for (auto b : blocks)
                for (auto s : b->statements)
                    fn (*s);
        }

        template <typename StatementType>
        void visitStatements (std::function<void(StatementType&)> fn)
        {
            for (auto b : blocks)
                for (auto s : b->statements)
                    if (auto t = cast<StatementType> (*s))
                        fn (*t);
        }

        std::vector<VariablePtr> getAllLocalVariables() const
        {
            std::vector<VariablePtr> locals;

            for (auto& b : blocks)
                for (auto s : b->statements)
                    if (auto a = cast<const Assignment> (*s))
                        if (a->target != nullptr)
                            if (auto v = a->target->getRootVariable())
                                if (! (v->isParameter() || v->isState() || contains (locals, v)))
                                    locals.push_back (v);

            return locals;
        }
    };

    //==============================================================================
    struct Block  : public Object
    {
        Block() = delete;
        Block (Identifier nm) : name (nm)  { SOUL_ASSERT (nm.toString()[0] == '@'); }

        bool isTerminated() const      { return terminator != nullptr; }

        void visitExpressions (ExpressionVisitorFn fn)
        {
            for (auto s : statements)
                s->visitExpressions (fn);

            terminator->visitExpressions (fn);
        }

        Identifier name;
        LinkedList<Statement> statements;
        TerminatorPtr terminator;

        std::vector<BlockPtr> predecessors;
        bool doNotOptimiseAway = false;
        void* temporaryData; // a general-purpose slot, used for temporary storage by algorithms
    };

    struct Statement  : public Object
    {
        Statement (CodeLocation l) : Object (std::move (l)) {}

        virtual bool readsVariable (VariablePtr) const          { return false; }
        virtual bool writesVariable (VariablePtr) const         { return false; }
        virtual void visitExpressions (ExpressionVisitorFn)     {}
        virtual bool mayHaveSideEffects() const                 { return false; }

        Statement* nextObject = nullptr; // used by LinkedList<Statement>
    };

    //==============================================================================
    struct Terminator  : public Object
    {
        virtual ArrayView<BlockPtr> getDestinationBlocks()      { return {}; }
        virtual bool isConditional() const                      { return false; }
        virtual bool isReturn() const                           { return false; }
        virtual bool readsVariable (VariablePtr) const          { return false; }
        virtual void visitExpressions (ExpressionVisitorFn)     {}
    };

    struct Branch  : public Terminator
    {
        Branch (BlockPtr b)  : target (b) {}
        ArrayView<BlockPtr> getDestinationBlocks() override  { return { &target, &target + 1 }; }

        BlockPtr target;
    };

    struct BranchIf  : public Terminator
    {
        BranchIf (ExpressionPtr cond, BlockPtr trueJump, BlockPtr falseJump)
            : condition (cond)
        {
            SOUL_ASSERT (trueJump != falseJump && trueJump != nullptr && falseJump != nullptr);
            targets[0] = trueJump;
            targets[1] = falseJump;
        }

        ArrayView<BlockPtr> getDestinationBlocks() override           { return { targets, targets + (isConditional() ? 2 : 1) }; }
        bool isConditional() const override                           { return targets[0] != targets[1]; }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            condition->visitExpressions (fn, AccessType::read);
            fn (condition, AccessType::read);
        }

        ExpressionPtr condition;
        BlockPtr targets[2];   // index 0 = true, 1 = false
    };

    struct ReturnVoid  : public Terminator
    {
        bool isReturn() const override            { return true; }
    };

    struct ReturnValue  : public Terminator
    {
        ReturnValue (Expression& v)  : returnValue (v) {}

        bool isReturn() const override            { return true; }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            returnValue->visitExpressions (fn, AccessType::read);
            fn (returnValue, AccessType::read);
        }

        ExpressionPtr returnValue;
    };

    //==============================================================================
    struct Assignment  : public Statement
    {
        Assignment (CodeLocation l, ExpressionPtr dest)  : Statement (std::move (l)), target (dest)     {}

        bool readsVariable (VariablePtr v) const override   { return target != nullptr && target->readsVariable (v); }
        bool writesVariable (VariablePtr v) const override  { return target != nullptr && target->writesVariable (v); }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            if (target != nullptr)
            {
                target->visitExpressions (fn, AccessType::write);
                fn (target, AccessType::write);
            }
        }

        bool mayHaveSideEffects() const override
        {
            return target != nullptr && target->getRootVariable()->isExternalToFunction();
        }

        ExpressionPtr target;
    };

    struct AssignFromValue  : public Assignment
    {
        AssignFromValue (CodeLocation l, ExpressionPtr dest, Expression& src)
            : Assignment (std::move (l), dest), source (src) {}

        bool readsVariable (VariablePtr v) const override
        {
            return source->readsVariable (v) || Assignment::readsVariable (v);
        }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            Assignment::visitExpressions (fn);
            source->visitExpressions (fn, AccessType::read);
            fn (source, AccessType::read);
        }

        ExpressionPtr source;
    };

    struct FunctionCall  : public Assignment
    {
        FunctionCall (CodeLocation l, ExpressionPtr dest, FunctionPtr f) : Assignment (std::move (l), dest), function (f)
        {
        }

        bool readsVariable (VariablePtr v) const override
        {
            return contains (arguments, v) || Assignment::readsVariable (v);
        }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            SOUL_ASSERT (function != nullptr);
            Assignment::visitExpressions (fn);

            for (size_t i = 0; i < arguments.size(); ++i)
            {
                auto mode = getFunction().parameters[i]->type.isReference() ? AccessType::readWrite
                                                                            : AccessType::read;
                arguments[i]->visitExpressions (fn, mode);
                fn (arguments[i], mode);
            }
        }

        bool mayHaveSideEffects() const override
        {
            return Assignment::mayHaveSideEffects() || getFunction().mayHaveSideEffects();
        }

        Function& getFunction() const
        {
            SOUL_ASSERT (function != nullptr);
            return *function;
        }

        FunctionPtr function; // may be temporarily null while building the program
        ArrayWithPreallocation<ExpressionPtr, 4> arguments;
    };

    //==============================================================================
    struct ReadStream  : public Assignment
    {
        ReadStream (CodeLocation l, Expression& dest, InputDeclaration& src)
            : Assignment (std::move (l), dest), source (src) {}

        bool mayHaveSideEffects() const override     { return true; }

        InputDeclarationPtr source;
    };

    struct WriteStream  : public Statement
    {
        WriteStream (CodeLocation l, OutputDeclaration& output, ExpressionPtr e, Expression& v)
            : Statement (std::move (l)), target (output), element (e), value (v) {}

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            if (element != nullptr)
            {
                element->visitExpressions (fn, AccessType::read);
                fn (element, AccessType::read);
            }

            value->visitExpressions (fn, AccessType::read);
            fn (value, AccessType::read);
        }

        bool mayHaveSideEffects() const override
        {
            return true;
        }

        OutputDeclarationPtr target;
        ExpressionPtr element, value;
    };

    //==============================================================================
    struct ProcessorProperty  : public Expression
    {
        enum class Property
        {
            none,
            period,
            frequency,
            id
        };

        ProcessorProperty (CodeLocation l, Property prop)
            : Expression (std::move (l)), property (prop),
              type (getPropertyType (prop))
        {
        }

        const Type& getType() const override                              { return type; }
        void visitExpressions (ExpressionVisitorFn, AccessType) override  {}
        bool readsVariable (VariablePtr) const override                   { return false; }
        bool writesVariable (VariablePtr) const override                  { return false; }
        VariablePtr getRootVariable() override                            { return {}; }
        Value getAsConstant() const override                              { return {}; }
        bool isMutable() const override                                   { return false; }
        bool isAssignable() const override                                { return false; }

        static Property getPropertyFromName (const std::string& name)
        {
            if (name == "period")     return Property::period;
            if (name == "frequency")  return Property::frequency;
            if (name == "id")         return Property::id;

            return Property::none;
        }

        static const char* getPropertyName (Property p)
        {
            if (p == Property::period)     return "period";
            if (p == Property::frequency)  return "frequency";
            if (p == Property::id)         return "id";

            SOUL_ASSERT_FALSE;
            return "";
        }

        static Type getPropertyType (Property p)
        {
            return p == Property::id ? PrimitiveType::int32 : Type::getFrequencyType();
        }

        const char* getPropertyName() const
        {
            return getPropertyName (property);
        }

        const Property property;
        const Type type;
    };

    struct AdvanceClock  : public Statement
    {
        AdvanceClock (CodeLocation l) : Statement (std::move (l)) {}
        bool mayHaveSideEffects() const override    { return true; }
    };

    //==============================================================================
    struct VariableListByType
    {
        VariableListByType (ArrayView<VariablePtr> variables)
        {
            for (auto& v : variables)
                getType (v->type).variables.push_back (v);
        }

        struct VariablesWithType
        {
            Type type;
            std::vector<VariablePtr> variables;
        };

        std::vector<VariablesWithType> types;

    private:
        VariablesWithType& getType (Type typeNeeded)
        {
            for (auto& type : types)
                if (type.type.isIdentical (typeNeeded))
                    return type;

            types.push_back ({ typeNeeded, {} });
            return types.back();
        }
    };
};

} // namespace soul
