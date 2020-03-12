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
class Program;

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
        X(ArrayElement) \
        X(StructElement) \
        X(Constant) \
        X(TypeCast) \
        X(UnaryOperator) \
        X(BinaryOperator) \
        X(PureFunctionCall) \
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

    #define SOUL_PREDECLARE_TYPE(Type)     struct Type;
    SOUL_HEART_OBJECTS (SOUL_PREDECLARE_TYPE)
    SOUL_HEART_STATEMENTS (SOUL_PREDECLARE_TYPE)
    SOUL_HEART_TERMINATORS (SOUL_PREDECLARE_TYPE)
    #undef SOUL_PREDECLARE_TYPE

    struct Parser;
    struct Printer;
    struct Checker;
    struct Utilities;

    static constexpr const char* getInitFunctionName()              { return "_soul_init"; }
    static constexpr const char* getPrepareFunctionName()           { return "_prepare"; }
    static constexpr const char* getRenderStatsFunctionName()       { return "_get_render_stats"; }
    static constexpr const char* getRunFunctionName()               { return "run"; }
    static constexpr const char* getUserInitFunctionName()          { return "init"; }
    static constexpr const char* getGenericSpecialisationNameTag()  { return "_specialised_"; }

    static std::string getExternalEventFunctionName (const std::string& endpointName, const Type& t)    { return "_external_event_" + endpointName + "_" + t.withConstAndRefFlags (false, false).getShortIdentifierDescription(); }
    static std::string getEventFunctionName (const std::string& endpointName, const Type& t)            { return "_" + endpointName + "_" + t.withConstAndRefFlags (false, false).getShortIdentifierDescription(); }
    static std::string getEventFunctionName (const heart::InputDeclaration&  i, const Type& t)          { return getEventFunctionName (i.name.toString(), t); }
    static std::string getEventFunctionName (const heart::OutputDeclaration& o, const Type& t)          { return getEventFunctionName (o.name.toString(), t); }

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
                if (TypeRules::canSilentlyCastTo (getSampleArrayType (sampleType), t))
                    return true;

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
                if (TypeRules::canPassAsArgumentTo (isElementAccess ? sampleType : getSampleArrayType (sampleType), t, true))
                    return getSampleArrayType (sampleType);

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

    protected:
        uint32_t getSampleTypesPackedSize() const
        {
            size_t packedSize = 0;

            for (auto& sampleType : sampleTypes)
                packedSize += sampleType.getPackedSizeInBytes();

            return static_cast<uint32_t> (packedSize);
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

        std::string getEventFunctionName (const Type& t) const  { SOUL_ASSERT (isEventEndpoint());  return getExternalEventFunctionName (name.toString(), t); }
        std::string getFramesTillNextEventFunctionName() const  { SOUL_ASSERT (isEventEndpoint());  return "_setFramesTillNextEvent_" + name.toString(); }
        std::string getAddFramesFunctionName() const            { SOUL_ASSERT (isStreamEndpoint()); return "_addFramesToInput_" + name.toString(); }
        std::string getAddSparseFramesFunctionName() const      { SOUL_ASSERT (isStreamEndpoint()); return "_addSparseFramesToInput_" + name.toString(); }
        std::string getSetValueFunctionName() const             { SOUL_ASSERT (isValueEndpoint());  return "_setValue_" + name.toString(); }

        EndpointDetails getDetails() const
        {
            return { EndpointID::create ("in:" + name.toString()), name, kind, sampleTypes, getSampleTypesPackedSize(), annotation };
        }
    };

    //==============================================================================
    struct OutputDeclaration  : public IODeclaration
    {
        OutputDeclaration (CodeLocation l) : IODeclaration (std::move (l)) {}

        std::string getEventFunctionName (const Type& t) const  { SOUL_ASSERT (isEventEndpoint());  return getExternalEventFunctionName (name.toString(), t); }
        std::string getReadFramesFunctionName() const           { SOUL_ASSERT (isStreamEndpoint()); return "_readFramesFromOutput_" + name.toString(); }
        std::string getGetEventsFunctionName() const            { SOUL_ASSERT (isEventEndpoint());  return "_getEventsFromOutput_" + name.toString(); }

        EndpointDetails getDetails() const
        {
            return { EndpointID::create ("out:" + name.toString()), name, kind, sampleTypes, getSampleTypesPackedSize(), annotation };
        }
    };

    //==============================================================================
    struct ProcessorInstance  : public Object
    {
        std::string instanceName, sourceName;
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
        pool_ptr<ProcessorInstance> sourceProcessor, destProcessor;
        std::string sourceEndpoint, destEndpoint;
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
    using ExpressionVisitorFn = const std::function<void(pool_ref<Expression>&, AccessType)>&;

    struct Expression  : public Object
    {
        Expression (CodeLocation l) : Object (std::move (l)) {}

        virtual const Type& getType() const = 0;
        virtual void visitExpressions (ExpressionVisitorFn, AccessType) = 0;
        virtual bool readsVariable (Variable&) const = 0;
        virtual bool writesVariable (Variable&) const = 0;
        virtual pool_ptr<Variable> getRootVariable() = 0;
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

        const Type& getType() const override           { return type; }
        pool_ptr<Variable> getRootVariable() override  { return *this; }

        bool isAssignable() const override             { return ! (isExternal() || type.isConst()); }
        bool isState() const                           { return role == Role::state || isExternal(); }
        bool isParameter() const                       { return role == Role::parameter; }
        bool isMutable() const override                { return role != Role::constant; }
        bool isMutableLocal() const                    { return role == Role::mutableLocal; }
        bool isConstant() const                        { return role == Role::constant; }
        bool isFunctionLocal() const                   { return isMutableLocal() || isConstant(); }
        bool isExternal() const                        { return role == Role::external; }
        bool isExternalToFunction() const              { return isState() || (isParameter() && type.isReference()); }

        void visitExpressions (ExpressionVisitorFn, AccessType) override    {}

        bool readsVariable (Variable& v) const override     { return this == std::addressof (v); }
        bool writesVariable (Variable& v) const override    { return this == std::addressof (v); }
        Value getAsConstant() const override                { return {}; }

        ReadWriteCount readWriteCount;
    };

    //==============================================================================
    struct ArrayElement  : public Expression
    {
        ArrayElement() = delete;
        ArrayElement (CodeLocation l, Expression& v, size_t index) : ArrayElement (std::move (l), v, index, index + 1) {}

        ArrayElement (CodeLocation l, Expression& v, size_t startIndex, size_t endIndex)
            : Expression (std::move (l)), parent (v), fixedStartIndex (startIndex), fixedEndIndex (endIndex)
        {
            SOUL_ASSERT (v.getType().isArrayOrVector());
        }

        ArrayElement (CodeLocation l, Expression& v, Expression& elementIndex)
            : Expression (std::move (l)), parent (v), dynamicIndex (elementIndex)
        {
            SOUL_ASSERT (v.getType().isArrayOrVector());
        }

        pool_ptr<Variable> getRootVariable() override   { return parent->getRootVariable(); }
        bool isMutable() const override                 { return parent->isMutable(); }
        bool isAssignable() const override              { return parent->isAssignable(); }
        bool isDynamic() const                          { return dynamicIndex != nullptr; }

        const Type& getType() const override
        {
            const auto& aggregateType = parent->getType();
            auto sliceSize = getSliceSize();

            if (sliceSize == 1)
            {
                if (aggregateType.isPrimitive())
                    temporaryType = aggregateType;
                else
                    temporaryType = aggregateType.getElementType();
            }
            else
            {
                SOUL_ASSERT (aggregateType.isArray() || aggregateType.isVector());
                SOUL_ASSERT (aggregateType.isUnsizedArray() || aggregateType.isValidArrayOrVectorRange (fixedStartIndex, fixedEndIndex));

                temporaryType = aggregateType.createCopyWithNewArraySize (sliceSize);
            }

            return temporaryType;
        }

        void visitExpressions (ExpressionVisitorFn fn, AccessType mode) override
        {
            if (isDynamic())
            {
                dynamicIndex->visitExpressions (fn, AccessType::read);
                auto ref = dynamicIndex.getAsPoolRef();
                fn (ref, AccessType::read);
                dynamicIndex = ref;
            }

            parent->visitExpressions (fn, mode);
            fn (parent, mode);
            SOUL_ASSERT (parent != *this);
        }

        bool readsVariable (Variable& v) const override    { return parent->readsVariable (v) || (isDynamic() && dynamicIndex->readsVariable (v)); }
        bool writesVariable (Variable& v) const override   { return parent->writesVariable (v); }

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

        pool_ref<Expression> parent;
        pool_ptr<Expression> dynamicIndex;
        size_t fixedStartIndex = 0, fixedEndIndex = 1;
        bool isRangeTrusted = false, suppressWrapWarning = false;

    private:
        mutable Type temporaryType; // used for holding a temp copy of the value returned from getType(), to avoid that method needing to return
                                    // by-value, which would impact performance of all the other classes that call it
    };

    //==============================================================================
    struct StructElement  : public Expression
    {
        StructElement() = delete;

        StructElement (CodeLocation l, Expression& v, std::string member)
           : Expression (std::move (l)), parent (v), memberName (std::move (member))
        {
            SOUL_ASSERT (v.getType().isStruct() && v.getType().getStructRef().hasMemberWithName (memberName));
        }

        pool_ptr<Variable> getRootVariable() override   { return parent->getRootVariable(); }
        bool isMutable() const override                 { return parent->isMutable(); }
        bool isAssignable() const override              { return parent->isAssignable(); }
        const Type& getType() const override            { return getStruct().getMemberWithName (memberName).type; }

        void visitExpressions (ExpressionVisitorFn fn, AccessType mode) override
        {
            parent->visitExpressions (fn, mode);
            fn (parent, mode);
            SOUL_ASSERT (parent != *this);
        }

        bool readsVariable (Variable& v) const override    { return parent->readsVariable (v); }
        bool writesVariable (Variable& v) const override   { return parent->writesVariable (v); }

        Value getAsConstant() const override
        {
            auto parentValue = parent->getAsConstant();

            if (parentValue.isValid())
                return parentValue.getSubElement (getMemberIndex());

            return {};
        }

        Structure& getStruct() const     { return parent->getType().getStructRef(); }
        size_t getMemberIndex() const    { return getStruct().getMemberIndex (memberName); }

        pool_ref<Expression> parent;
        std::string memberName;
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
        pool_ptr<Variable> getRootVariable() override      { return {}; }
        bool readsVariable (Variable&) const override      { return false; }
        bool writesVariable (Variable&) const override     { return false; }
        void visitExpressions (ExpressionVisitorFn, AccessType) override {}

        Value value;
    };

    //==============================================================================
    struct TypeCast  : public Expression
    {
        TypeCast (CodeLocation l, Expression& src, const Type& type)
            : Expression (std::move (l)), source (src), destType (type)
        {}

        const Type& getType() const override                 { return destType; }
        pool_ptr<Variable> getRootVariable() override        { SOUL_ASSERT_FALSE; return {}; }
        bool readsVariable (Variable& v) const override      { return source->readsVariable (v); }
        bool writesVariable (Variable&) const override       { return false; }
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

        pool_ref<Expression> source;
        Type destType;
    };

    //==============================================================================
    struct UnaryOperator  : public Expression
    {
        UnaryOperator (CodeLocation l, Expression& src, UnaryOp::Op op)
            : Expression (std::move (l)), source (src), operation (op)
        {}

        const Type& getType() const override                 { return source->getType(); }
        pool_ptr<Variable> getRootVariable() override        { SOUL_ASSERT_FALSE; return {}; }
        bool readsVariable (Variable& v) const override      { return source->readsVariable (v); }
        bool writesVariable (Variable&) const override       { return false; }
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

        pool_ref<Expression> source;
        UnaryOp::Op operation;
    };

    //==============================================================================
    struct BinaryOperator  : public Expression
    {
        BinaryOperator (CodeLocation l, Expression& a, Expression& b, BinaryOp::Op op)
            : Expression (std::move (l)), lhs (a), rhs (b), operation (op)
        {
        }

        pool_ptr<Variable> getRootVariable() override        { SOUL_ASSERT_FALSE; return {}; }
        bool readsVariable (Variable& v) const override      { return lhs->readsVariable (v) || rhs->readsVariable (v); }
        bool writesVariable (Variable&) const override       { return false; }
        bool isMutable() const override                      { return false; }
        bool isAssignable() const override                   { return false; }

        const Type& getType() const override
        {
            temporaryType = BinaryOp::getTypes (operation, lhs->getType(), rhs->getType()).resultType;
            SOUL_ASSERT (temporaryType.isValid());
            return temporaryType;
        }

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

        pool_ref<Expression> lhs, rhs;
        BinaryOp::Op operation;

    private:
        mutable Type temporaryType;
    };

    //==============================================================================
    struct Function  : public Object
    {
        Type returnType;
        Identifier name;
        std::vector<pool_ref<Variable>> parameters;
        std::vector<pool_ref<Block>> blocks;
        Annotation annotation;
        IntrinsicType intrinsicType = IntrinsicType::none;
        pool_ptr<Variable> stateParameter;

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
        uint64_t localVariableStackSize = 0;

        pool_ptr<Block> findBlockByName (const std::string& blockName) const
        {
            for (auto b : blocks)
                if (b->name == blockName)
                    return b;

            return {};
        }

        void addStateParameter (Variable& param)
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
                p->readWriteCount.reset();

            visitExpressions ([] (pool_ref<Expression>& value, AccessType)
                              {
                                  if (auto v = cast<Variable> (value))
                                      v->readWriteCount.reset();
                              });

            visitExpressions ([] (pool_ref<Expression>& value, AccessType mode)
                              {
                                  if (auto v = cast<Variable> (value))
                                      v->readWriteCount.increment (mode);
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

        std::vector<pool_ref<Variable>> getAllLocalVariables() const
        {
            std::vector<pool_ref<Variable>> locals;

            for (auto& b : blocks)
                for (auto s : b->statements)
                    if (auto a = cast<const Assignment> (*s))
                        if (a->target != nullptr)
                            if (auto v = a->target->getRootVariable())
                                if (! (v->isParameter() || v->isState() || contains (locals, v)))
                                    locals.push_back (*v);

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
        pool_ptr<Terminator> terminator;

        std::vector<pool_ref<Block>> predecessors;
        bool doNotOptimiseAway = false;
        void* temporaryData; // a general-purpose slot, used for temporary storage by algorithms
    };

    struct Statement  : public Object
    {
        Statement (CodeLocation l) : Object (std::move (l)) {}

        virtual bool readsVariable (Variable&) const            { return false; }
        virtual bool writesVariable (Variable&) const           { return false; }
        virtual void visitExpressions (ExpressionVisitorFn)     {}
        virtual bool mayHaveSideEffects() const                 { return false; }

        Statement* nextObject = nullptr; // used by LinkedList<Statement>
    };

    //==============================================================================
    struct Terminator  : public Object
    {
        virtual ArrayView<pool_ref<Block>> getDestinationBlocks()   { return {}; }
        virtual bool isConditional() const                          { return false; }
        virtual bool isReturn() const                               { return false; }
        virtual bool readsVariable (Variable&) const                { return false; }
        virtual void visitExpressions (ExpressionVisitorFn)         {}
    };

    struct Branch  : public Terminator
    {
        Branch (Block& b)  : target (b) {}
        ArrayView<pool_ref<Block>> getDestinationBlocks() override  { return { &target, &target + 1 }; }

        pool_ref<Block> target;
    };

    struct BranchIf  : public Terminator
    {
        BranchIf (Expression& cond, Block& trueJump, Block& falseJump)
            : condition (cond), targets { trueJump, falseJump }
        {
            SOUL_ASSERT (targets[0] != targets[1]);
        }

        ArrayView<pool_ref<Block>> getDestinationBlocks() override    { return { targets, targets + (isConditional() ? 2 : 1) }; }
        bool isConditional() const override                           { return targets[0] != targets[1]; }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            condition->visitExpressions (fn, AccessType::read);
            fn (condition, AccessType::read);
        }

        pool_ref<Expression> condition;
        pool_ref<Block> targets[2];   // index 0 = true, 1 = false
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

        pool_ref<Expression> returnValue;
    };

    //==============================================================================
    struct Assignment  : public Statement
    {
        Assignment (CodeLocation l, pool_ptr<Expression> dest)  : Statement (std::move (l)), target (dest)  {}

        bool readsVariable (Variable& v) const override   { return target != nullptr && target->readsVariable (v); }
        bool writesVariable (Variable& v) const override  { return target != nullptr && target->writesVariable (v); }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            if (target != nullptr)
            {
                target->visitExpressions (fn, AccessType::write);
                auto ref = target.getAsPoolRef();
                fn (ref, AccessType::write);
                target = ref;
            }
        }

        bool mayHaveSideEffects() const override
        {
            return target != nullptr && target->getRootVariable()->isExternalToFunction();
        }

        pool_ptr<Expression> target;
    };

    struct AssignFromValue  : public Assignment
    {
        AssignFromValue (CodeLocation l, Expression& dest, Expression& src)
            : Assignment (std::move (l), dest), source (src) {}

        bool readsVariable (Variable& v) const override
        {
            return source->readsVariable (v) || Assignment::readsVariable (v);
        }

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            Assignment::visitExpressions (fn);
            source->visitExpressions (fn, AccessType::read);
            fn (source, AccessType::read);
        }

        pool_ref<Expression> source;
    };

    struct FunctionCall  : public Assignment
    {
        FunctionCall (CodeLocation l, pool_ptr<Expression> dest, pool_ptr<Function> f)
            : Assignment (std::move (l), dest), function (f)
        {}

        bool readsVariable (Variable& v) const override
        {
            for (auto& a : arguments)
                if (a->readsVariable (v))
                    return true;

            return Assignment::readsVariable (v);
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

        pool_ptr<Function> function; // may be temporarily null while building the program
        using ArgListType = ArrayWithPreallocation<pool_ref<Expression>, 4>;
        ArgListType arguments;
    };

    //==============================================================================
    struct PureFunctionCall  : public Expression
    {
        PureFunctionCall (CodeLocation l, Function& fn)  : Expression (std::move (l)), function (fn) {}

        const Type& getType() const override               { return function.returnType; }
        Value getAsConstant() const override               { return {}; }
        pool_ptr<Variable> getRootVariable() override      { SOUL_ASSERT_FALSE; return {}; }
        bool writesVariable (Variable&) const override     { return false; }
        bool isMutable() const override                    { return false; }
        bool isAssignable() const override                 { return false; }

        bool readsVariable (Variable& v) const override
        {
            for (auto& a : arguments)
                if (a->readsVariable (v))
                    return true;

            return false;
        }

        void visitExpressions (ExpressionVisitorFn fn, AccessType) override
        {
            // NB: avoid range-based-for as the vector can be mutated by the lambda
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                arguments[i]->visitExpressions (fn, AccessType::read);
                fn (arguments[i], AccessType::read);
            }
        }

        Function& function;
        std::vector<pool_ref<Expression>> arguments;
    };

    //==============================================================================
    struct ReadStream  : public Assignment
    {
        ReadStream (CodeLocation l, Expression& dest, InputDeclaration& src)
            : Assignment (std::move (l), dest), source (src) {}

        bool mayHaveSideEffects() const override     { return true; }

        pool_ref<InputDeclaration> source;
    };

    struct WriteStream  : public Statement
    {
        WriteStream (CodeLocation l, OutputDeclaration& output, pool_ptr<Expression> e, Expression& v)
            : Statement (std::move (l)), target (output), element (e), value (v) {}

        void visitExpressions (ExpressionVisitorFn fn) override
        {
            if (element != nullptr)
            {
                element->visitExpressions (fn, AccessType::read);
                auto ref = element.getAsPoolRef();
                fn (ref, AccessType::read);
                element = ref;
            }

            value->visitExpressions (fn, AccessType::read);
            fn (value, AccessType::read);
        }

        bool mayHaveSideEffects() const override
        {
            return true;
        }

        pool_ref<OutputDeclaration> target;
        pool_ptr<Expression> element;
        pool_ref<Expression> value;
    };

    //==============================================================================
    struct ProcessorProperty  : public Expression
    {
        enum class Property
        {
            none,
            period,
            frequency,
            id,
            session
        };

        ProcessorProperty (CodeLocation l, Property prop)
            : Expression (std::move (l)), property (prop),
              type (getPropertyType (prop))
        {
        }

        const Type& getType() const override                              { return type; }
        void visitExpressions (ExpressionVisitorFn, AccessType) override  {}
        bool readsVariable (Variable&) const override                     { return false; }
        bool writesVariable (Variable&) const override                    { return false; }
        pool_ptr<Variable> getRootVariable() override                     { return {}; }
        Value getAsConstant() const override                              { return {}; }
        bool isMutable() const override                                   { return false; }
        bool isAssignable() const override                                { return false; }

        static Property getPropertyFromName (const std::string& name)
        {
            if (name == "period")     return Property::period;
            if (name == "frequency")  return Property::frequency;
            if (name == "id")         return Property::id;
            if (name == "session")    return Property::session;

            return Property::none;
        }

        static const char* getPropertyName (Property p)
        {
            if (p == Property::period)     return "period";
            if (p == Property::frequency)  return "frequency";
            if (p == Property::id)         return "id";
            if (p == Property::session)    return "session";

            SOUL_ASSERT_FALSE;
            return "";
        }

        static Type getPropertyType (Property p)
        {
            if (p == Property::id || p == Property::session)
                return PrimitiveType::int32;

            return Type::getFrequencyType();
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
};

} // namespace soul
