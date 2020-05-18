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

namespace Operator
{
    SOUL_OPERATORS (SOUL_DECLARE_TOKEN)
}

//==============================================================================
struct DummyKeywordMatcher
{
    static TokenType match (int, UTF8Reader) noexcept  { return {}; }
};

//==============================================================================
struct IdentifierMatcher
{
    static constexpr bool isIdentifierAnywhere (UnicodeChar c) noexcept  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    static constexpr bool isIdentifierStart    (UnicodeChar c) noexcept  { return isIdentifierAnywhere (c) || c == '$'; }
    static constexpr bool isIdentifierBody     (UnicodeChar c) noexcept  { return isIdentifierAnywhere (c) || (c >= '0' && c <= '9'); }
};

//==============================================================================
namespace HEARTOperator
{
    // NB: declaration order matters here for operators of different lengths that start the same way
    #define SOUL_HEART_OPERATORS(X) \
        X(semicolon,          ";")      X(dot,                ".")  \
        X(comma,              ",")      X(at,                 "@")  \
        X(openParen,          "(")      X(closeParen,         ")")  \
        X(openBrace,          "{")      X(closeBrace,         "}")  \
        X(openDoubleBracket,  "[[")     X(closeDoubleBracket, "]]") \
        X(openBracket,        "[")      X(closeBracket,       "]")  \
        X(doubleColon,        "::")     X(colon,              ":")  \
        X(question,           "?")      X(hash,               "#")  \
        X(equals,             "==")     X(assign,             "=")  \
        X(notEquals,          "!=")     X(logicalNot,         "!")  \
        X(rightArrow,         "->")     X(minus,              "-")  \
        X(plus,               "+")      X(times,              "*")  \
        X(divide,             "/")      X(modulo,             "%")  \
        X(bitwiseXor,         "^")      X(bitwiseNot,         "~")  \
        X(logicalAnd,         "&&")     X(bitwiseAnd,         "&")  \
        X(logicalOr,          "||")     X(bitwiseOr,          "|")  \
        X(leftShift,          "<<")     X(lessThanOrEqual,    "<=") \
        X(leftArrow,          "<-")     X(lessThan,           "<")  \
        X(rightShiftUnsigned, ">>>")    X(rightShift,         ">>") \
        X(greaterThanOrEqual, ">=")     X(greaterThan,        ">")

    SOUL_HEART_OPERATORS (SOUL_DECLARE_TOKEN)

    struct Matcher
    {
        static TokenType match (UTF8Reader& text) noexcept
        {
            #define SOUL_COMPARE_OPERATOR(name, str) if (text.advanceIfStartsWith (str)) return name;
            SOUL_HEART_OPERATORS (SOUL_COMPARE_OPERATOR)
            #undef SOUL_COMPARE_OPERATOR
            return {};
        }
    };
}

//==============================================================================
struct heart::Parser   : public Tokeniser<DummyKeywordMatcher,
                                          HEARTOperator::Matcher,
                                          IdentifierMatcher>
{
    static Program parse (const CodeLocation& code)
    {
        return heart::Parser (code).parse();
    }

    static Type parseType (const CodeLocation& code)
    {
        return heart::Parser (code).readValueType();
    }

    [[noreturn]] void throwError (const CompileMessage& message) const override
    {
        soul::throwError (message.withLocation (location));
    }

private:
    //==============================================================================
    struct ScannedTopLevelItem
    {
        ScannedTopLevelItem (Module& m) : module (m) {}

        Module& module;
        UTF8Reader moduleStartPos;
        std::vector<UTF8Reader> functionParamCode, functionBodyCode, structBodyCode, inputDecls, outputDecls, stateVariableDecls;
    };

    Program program;
    pool_ptr<Module> module;

    //==============================================================================
    Parser (const CodeLocation& text)  : Tokeniser (text) {}
    ~Parser() override = default;

    //==============================================================================
    Program parse()
    {
        checkVersionDeclaration();

        std::vector<ScannedTopLevelItem> scannedTopLevelItems;
        scannedTopLevelItems.reserve (128);

        while (! matches (Token::eof))
        {
            if (matchIf ("graph"))              scanTopLevelItem (scannedTopLevelItems, program.addGraph());
            else if (matchIf ("processor"))     scanTopLevelItem (scannedTopLevelItems, program.addProcessor());
            else if (matchIf ("namespace"))     scanTopLevelItem (scannedTopLevelItems, program.addNamespace());
            else                                throwError (Errors::expectedTopLevelDecl());
        }

        for (auto& item : scannedTopLevelItems)  parseModuleStructs (item);
        for (auto& item : scannedTopLevelItems)  parseFunctionDecls (item);
        for (auto& item : scannedTopLevelItems)  parseStateVariables (item);
        for (auto& item : scannedTopLevelItems)  parseModule (item);

        return program;
    }

    void scanTopLevelItem (std::vector<ScannedTopLevelItem>& scannedTopLevelItems, Module& newModule)
    {
        ScannedTopLevelItem newItem (newModule);
        module = newModule;
        newModule.fullName = readQualifiedIdentifier();
        newModule.originalFullName = newModule.fullName;
        newModule.shortName = TokenisedPathString (newModule.fullName).getLastPart();
        parseAnnotation (newModule.annotation);
        newItem.moduleStartPos = getCurrentTokeniserPosition();
        scanTopLevelItems (newItem);
        scannedTopLevelItems.push_back (std::move (newItem));
    }

    void prepareToRescan (ScannedTopLevelItem& item)
    {
        module = item.module;
        resetPosition (item.moduleStartPos);
    }

    void parseModuleStructs (ScannedTopLevelItem& item)
    {
        prepareToRescan (item);
        SOUL_ASSERT (module->structs.size() == item.structBodyCode.size());

        auto nextItemPos = getCurrentTokeniserPosition();

        for (size_t i = 0; i < item.structBodyCode.size(); ++i)
        {
            resetPosition (item.structBodyCode[i]);
            parseStructBody (*module->structs[i]);
        }

        resetPosition (nextItemPos);
        module.reset();
    }

    void parseFunctionDecls (ScannedTopLevelItem& item)
    {
        prepareToRescan (item);
        SOUL_ASSERT (module->functions.size() == item.functionParamCode.size());
        SOUL_ASSERT (module->functions.size() == item.functionBodyCode.size());

        for (size_t i = 0; i < item.functionParamCode.size(); ++i)
        {
            resetPosition (item.functionParamCode[i]);
            parseFunctionParams (module->functions[i]);
        }

        module.reset();
    }

    void parseStateVariables (ScannedTopLevelItem& item)
    {
        prepareToRescan (item);

        for (auto& g : item.stateVariableDecls)
        {
            resetPosition (g);
            parseStateVariable();
        }

        module.reset();
    }

    void parseModule (ScannedTopLevelItem& item)
    {
        prepareToRescan (item);
        SOUL_ASSERT (module->inputs.size()  == item.inputDecls.size());
        SOUL_ASSERT (module->outputs.size() == item.outputDecls.size());
        parseTopLevelItems (item);
        module.reset();
    }

    void scanTopLevelItems (ScannedTopLevelItem& item)
    {
        expect (HEARTOperator::openBrace);

        while (! matchIf (HEARTOperator::closeBrace))
            scanNextTopLevelItem (item);
    }

    void scanNextTopLevelItem (ScannedTopLevelItem& item)
    {
        if (module->isGraph() || module->isProcessor())
        {
            if (matchIf ("input"))       return scanInput (item);
            if (matchIf ("output"))      return scanOutput (item);
        }

        if (module->isGraph())
        {
            if (matchIf ("node"))        return parseUsing();
            if (matchIf ("connection"))  return parseConnection();
        }
        else
        {
            if (matchIf ("struct"))      return scanStruct (item);
            if (matchIf ("function"))    return scanFunction (item, false);
            if (matchIf ("var"))         return scanStateVariable (item);
        }

        if (module->isProcessor())
        {
            if (matchIf ("var"))         return scanStateVariable (item);
            if (matchIf ("event"))       return scanFunction (item, true);
        }

        throwError (Errors::expectedTopLevelDecl());
    }

    void parseTopLevelItems (ScannedTopLevelItem& item)
    {
        auto nextItemPos = getCurrentTokeniserPosition();

        for (size_t i = 0; i < item.inputDecls.size(); ++i)
        {
            resetPosition (item.inputDecls[i]);
            parseInput (module->inputs[i]);
        }

        for (size_t i = 0; i < item.outputDecls.size(); ++i)
        {
            resetPosition (item.outputDecls[i]);
            parseOutput (module->outputs[i]);
        }

        for (size_t i = 0; i < item.functionBodyCode.size(); ++i)
        {
            if (item.functionBodyCode[i] != UTF8Reader())
            {
                resetPosition (item.functionBodyCode[i]);
                parseFunctionBody (module->functions[i]);
            }
        }

        if (! module->isNamespace())
            if (module->outputs.empty())
                throwError (Errors::processorNeedsAnOutput());

        resetPosition (nextItemPos);
    }

    void scanInput (ScannedTopLevelItem& item)
    {
        item.inputDecls.push_back (getCurrentTokeniserPosition());
        auto& inputDeclaration = module->allocate<heart::InputDeclaration> (location);
        inputDeclaration.name = parseIdentifier();
        inputDeclaration.index = (uint32_t) module->inputs.size();

        if (module->findInput (inputDeclaration.name) != nullptr || module->findOutput (inputDeclaration.name) != nullptr)
            throwError (Errors::nameInUse (inputDeclaration.name));

        module->inputs.push_back (inputDeclaration);
        skipPastNextOccurrenceOf (HEARTOperator::semicolon);
    }

    void scanOutput (ScannedTopLevelItem& item)
    {
        item.outputDecls.push_back (getCurrentTokeniserPosition());
        auto& output = module->allocate<heart::OutputDeclaration> (location);
        output.name = parseIdentifier();

        if (module->findInput (output.name) != nullptr || module->findOutput (output.name) != nullptr)
            throwError (Errors::nameInUse (output.name));

        module->outputs.push_back (output);
        skipPastNextOccurrenceOf (HEARTOperator::semicolon);
    }

    uint32_t parseProcessorArraySize()
    {
        auto errorLocation = location;
        auto arraySize = parseInt32();

        if (arraySize <= 0 || (size_t) arraySize > AST::maxProcessorArraySize)
            errorLocation.throwError (Errors::illegalArraySize());

        return (uint32_t) arraySize;
    }

    void parseInput (heart::InputDeclaration& inputDeclaration)
    {
        inputDeclaration.name = parseIdentifier();

        if (matchIf (HEARTOperator::openBracket))
        {
            inputDeclaration.arraySize = parseProcessorArraySize();
            expect (HEARTOperator::closeBracket);
        }

        inputDeclaration.kind = parseEndpointKind (*this);

        if (inputDeclaration.isEventEndpoint())
            inputDeclaration.dataTypes = readEventTypeList();
        else
            inputDeclaration.dataTypes.emplace_back (readValueType());

        parseAnnotation (inputDeclaration.annotation);
        expectSemicolon();
    }

    void parseOutput (heart::OutputDeclaration& output)
    {
        output.name = parseIdentifier();

        if (matchIf (HEARTOperator::openBracket))
        {
            output.arraySize = parseProcessorArraySize();
            expect (HEARTOperator::closeBracket);
        }

        output.kind = parseEndpointKind (*this);

        if (output.isEventEndpoint())
            output.dataTypes = readEventTypeList();
        else
            output.dataTypes.emplace_back (readValueType());

        parseAnnotation (output.annotation);
        expectSemicolon();
    }

    void parseAnnotation (Annotation& annotation)
    {
        if (matchIf (HEARTOperator::openDoubleBracket))
        {
            if (matchIf (HEARTOperator::closeDoubleBracket))
                return;

            do
            {
                auto name  = matchIf (Token::literalString) ? currentStringValue : readIdentifier();
                auto value = matchIf (HEARTOperator::colon) ? parseAnnotationValue() : Value (true);

                annotation.set (name, value, program.getStringDictionary());
            }
            while (matchIf (HEARTOperator::comma));

            expect (HEARTOperator::closeDoubleBracket);
        }
    }

    Value parseAnnotationValue()
    {
        if (matches (Token::literalInt32))     { auto v = literalIntValue;    skip(); return Value::createInt32 (v); }
        if (matches (Token::literalInt64))     { auto v = literalIntValue;    skip(); return Value::createInt64 (v); }
        if (matches (Token::literalFloat32))   { auto v = literalDoubleValue; skip(); return Value ((float) v); }
        if (matches (Token::literalFloat64))   { auto v = literalDoubleValue; skip(); return Value (v); }
        if (matches (Token::literalString))    { auto v = program.getStringDictionary().getHandleForString (currentStringValue); skip(); return Value::createStringLiteral(v); }
        if (matchIf ("true"))                  return Value (true);
        if (matchIf ("false"))                 return Value (false);

        auto infOrNaN = parseNaNandInfinityTokens();

        if (infOrNaN.isValid())
            return infOrNaN;

        return {};
    }

    void parseUsing()
    {
        auto name = readQualifiedIdentifier();

        for (auto m : module->processorInstances)
            if (m->instanceName == name)
                location.throwError (Errors::duplicateProcessor (name));

        auto& mi = module->allocate<heart::ProcessorInstance>();
        module->processorInstances.push_back (mi);
        mi.instanceName = name;
        expect (HEARTOperator::assign);
        mi.sourceName = readQualifiedIdentifier();

        if (matchIf (HEARTOperator::openBracket))
        {
            mi.arraySize = parseProcessorArraySize();
            expect (HEARTOperator::closeBracket);
        }

        if (matchIf (HEARTOperator::times))
        {
            auto errorPos = location;
            mi.clockMultiplier = heart::getClockRatioFromValue (errorPos, parseInt32Value());
        }
        else if (matchIf (HEARTOperator::divide))
        {
            auto errorPos = location;
            mi.clockDivider = heart::getClockRatioFromValue (errorPos, parseInt32Value());
        }

        expectSemicolon();
    }

    void parseConnection()
    {
        auto& c = module->allocate<heart::Connection> (location);
        module->connections.push_back (c);

        c.interpolationType = parseInterpolationType (*this);
        auto src = readProcessorAndChannel();
        c.sourceProcessor = src.processor;
        c.sourceEndpoint = src.endpoint;
        c.sourceEndpointIndex = src.endpointIndex;
        expect (HEARTOperator::rightArrow);

        if (matchIf (HEARTOperator::openBracket))
        {
            c.delayLength = parseInt32();

            if (c.delayLength < 1)
                location.throwError (Errors::delayLineTooShort());

            if (c.delayLength > (int32_t) AST::maxDelayLineLength)
                location.throwError (Errors::delayLineTooLong());

            expect (HEARTOperator::closeBracket);
            expect (HEARTOperator::rightArrow);
        }

        auto dst = readProcessorAndChannel();
        c.destProcessor = dst.processor;
        c.destEndpoint = dst.endpoint;
        c.destEndpointIndex = dst.endpointIndex;

        expectSemicolon();
    }

    struct ProcessorAndChannel
    {
        pool_ptr<heart::ProcessorInstance> processor;
        std::string endpoint;
        int64_t endpointIndex = -1;
    };

    ProcessorAndChannel readProcessorAndChannel()
    {
        ProcessorAndChannel processorAndChannel;

        auto name = readQualifiedIdentifier();

        if (matchIf (HEARTOperator::dot))
        {
            processorAndChannel.processor = findProcessorInstance (name);
            processorAndChannel.endpoint   = readIdentifier();
        }
        else
        {
            processorAndChannel.endpoint = name;
        }

        if (matchIf (HEARTOperator::openBracket))
        {
            processorAndChannel.endpointIndex = parseInt32();

            expect (HEARTOperator::closeBracket);
        }

        return processorAndChannel;
    }

    pool_ptr<heart::ProcessorInstance> findProcessorInstance (const std::string& instanceName)
    {
        for (auto m : module->processorInstances)
            if (m->instanceName == instanceName)
                return m;

        throwError (Errors::cannotFindProcessor (instanceName));
        return {};
    }

    void scanStateVariable (ScannedTopLevelItem& item)
    {
        item.stateVariableDecls.push_back (getCurrentTokeniserPosition());
        skipPastNextOccurrenceOf (HEARTOperator::semicolon);
    }

    void parseStateVariable()
    {
        bool isExternal = matchIf ("external");
        auto type = readValueType();
        auto name = program.getAllocator().get (readIdentifier());

        for (auto& v : module->stateVariables)
            if (v->name == name)
                throwError (Errors::nameInUse (v->name));

        auto& v = module->allocate<heart::Variable> (location, type, name,
                                                     isExternal ? heart::Variable::Role::external
                                                                : heart::Variable::Role::state);
        parseAnnotation (v.annotation);
        module->stateVariables.push_back (v);
        expectSemicolon();
    }

    void scanStruct (ScannedTopLevelItem& item)
    {
        auto name = readQualifiedIdentifier();

        if (module->findStruct (name) != nullptr)
            throwError (Errors::nameInUse (name));

        module->addStruct (std::move (name));
        expect (HEARTOperator::openBrace);
        item.structBodyCode.push_back (getCurrentTokeniserPosition());
        skipPastNextOccurrenceOf (HEARTOperator::closeBrace);
    }

    void parseStructBody (Structure& s)
    {
        while (! matchIf (HEARTOperator::closeBrace))
        {
            auto type = readValueType();
            auto name = readIdentifier();
            expectSemicolon();

            if (s.hasMemberWithName (name))
                throwError (Errors::nameInUse (name));

            s.addMember (type, name);
        }
    }

    void scanFunction (ScannedTopLevelItem& item, bool isEventFunction)
    {
        auto& fn = module->allocate<heart::Function>();

        fn.name = parseIdentifier();

        if (isEventFunction)                              fn.functionType = FunctionType::event();
        else if (fn.name == getRunFunctionName())         fn.functionType = FunctionType::run();
        else if (fn.name == getUserInitFunctionName())    fn.functionType = FunctionType::userInit();

        if (module->findFunction (fn.name) != nullptr)
            throwError (Errors::nameInUse (fn.name));

        expect (HEARTOperator::openParen);
        item.functionParamCode.push_back (getCurrentTokeniserPosition());
        skipPastNextOccurrenceOf (HEARTOperator::closeParen);

        for (;;)
        {
            if (matchIf (HEARTOperator::openBrace))
            {
                item.functionBodyCode.push_back (getCurrentTokeniserPosition());
                skipPastNextOccurrenceOf (HEARTOperator::closeBrace);
                break;
            }

            if (matchIf (HEARTOperator::semicolon))
            {
                fn.hasNoBody = true;
                item.functionBodyCode.push_back ({});
                break;
            }

            if (matchIf (HEARTOperator::openDoubleBracket))
            {
                skipPastNextOccurrenceOf (HEARTOperator::closeDoubleBracket);
                continue;
            }

            if (matches (Token::eof))
                expect (HEARTOperator::openBrace);

            skip();
        }

        module->functions.push_back (fn);
    }

    struct FunctionParseState
    {
        FunctionParseState (heart::Function& f) : function (f) {}

        struct BlockCode
        {
            heart::Block& block;
            UTF8Reader code;
        };

        heart::Function& function;
        std::vector<BlockCode> blocks;
        std::vector<pool_ref<heart::Variable>> variables;
    };

    void parseFunctionParams (heart::Function& f)
    {
        FunctionBuilder builder (*module);
        FunctionParseState state (f);

        if (! matchIf (HEARTOperator::closeParen))
        {
            for (;;)
            {
                auto type = readValueOrRefType();
                auto paramLocation = location;
                auto name = parseIdentifier();
                f.parameters.push_back (module->allocate<heart::Variable> (std::move (paramLocation), type, name,
                                                                           heart::Variable::Role::parameter));

                if (matchIf (HEARTOperator::comma))
                    continue;

                expect (HEARTOperator::closeParen);
                break;
            }
        }

        if (! f.functionType.isEvent())
        {
            expect (HEARTOperator::rightArrow);
            f.returnType = readValueType();
        }

        parseAnnotation (f.annotation);

        if (! matchIf (HEARTOperator::semicolon))
            expect (HEARTOperator::openBrace);

        auto intrin = f.annotation.getString ("intrin");

        if (! intrin.empty())
        {
            f.intrinsicType = getIntrinsicTypeFromName (intrin);
            f.functionType = heart::FunctionType::intrinsic();
        }
    }

    void parseFunctionBody (heart::Function& f)
    {
        FunctionBuilder builder (*module);
        FunctionParseState state (f);

        if (! matchIf (HEARTOperator::closeBrace))
            scanBlocks (state, builder);

        builder.beginFunction (f);

        for (auto& b : state.blocks)
        {
            resetPosition (b.code);
            builder.beginBlock (b.block);

            while (! parseTerminator (state, builder))
                if (! parseStatement (state, builder))
                    throwError (Errors::expectedStatement());
        }

        builder.endFunction();
    }

    void scanBlocks (FunctionParseState& state, FunctionBuilder& builder)
    {
        for (;;)
        {
            auto name = readBlockName();
            expect (HEARTOperator::colon);
            state.blocks.push_back ({ builder.createBlock (name), getCurrentTokeniserPosition() });

            skipPastNextOccurrenceOf (HEARTOperator::semicolon);

            while (! matches (HEARTOperator::at))
            {
                if (matchIf (HEARTOperator::closeBrace))
                    return;

                skipPastNextOccurrenceOf (HEARTOperator::semicolon);
            }
        }
    }

    struct AssignmentTarget
    {
        pool_ptr<heart::Expression> existingVariable;
        std::string newVariableName;
        bool isConst, isNull;

        bool checkType (const Type& sourceType) const
        {
            return existingVariable == nullptr
                    || TypeRules::canPassAsArgumentTo (existingVariable->getType(), sourceType, true);
        }

        pool_ptr<heart::Expression> create (FunctionParseState& state, FunctionBuilder& builder, const Type& type) const
        {
            if (isNull)
                return {};

            if (existingVariable != nullptr)
                return *existingVariable;

            auto& newVar = builder.createVariable (type, newVariableName, isConst ? heart::Variable::Role::constant
                                                                                  : heart::Variable::Role::mutableLocal);
            state.variables.push_back (newVar);
            return newVar;
        }
    };

    bool parseStatement (FunctionParseState& state, FunctionBuilder& builder)
    {
        if (matchIf ("let"))
        {
            auto name = readIdentifier();

            if (findVariable (state, name, false) != nullptr)
                throwError (Errors::nameInUse (name));

            parseVariableAssignment (state, builder, { nullptr, name, true, false });
            return true;
        }

        if (matchIf ("write"))
            return parseWriteStream (state, builder);

        if (matchIf ("advance"))
        {
            expectSemicolon();
            builder.addAdvance (location);
            return true;
        }

        if (matchIf ("call"))
        {
            parseFunctionCall (state, builder, { nullptr, {}, false, true });
            return true;
        }

        if (matches (Token::identifier))
        {
            if (auto existingVariableTarget = parseVariableExpression (state))
            {
                parseVariableAssignment (state, builder, { existingVariableTarget, {}, false, false });
                return true;
            }

            auto newVariableName = readIdentifier();
            parseVariableAssignment (state, builder, { nullptr, newVariableName, false, false });
            return true;
        }

        return false;
    }

    void parseVariableAssignment (FunctionParseState& state, FunctionBuilder& builder, const AssignmentTarget& target)
    {
        expect (HEARTOperator::assign);

        if (matchIf ("call"))
            return parseFunctionCall (state, builder, target);

        if (matchIf ("read"))
            return parseReadStream (state, builder, target);

        auto errorLocation = location;
        auto& sourceValue = parseExpression (state);

        if (! target.checkType (sourceValue.getType()))
            errorLocation.throwError (Errors::incompatibleTargetType());

        expectSemicolon();

        if (auto v = target.create (state, builder, sourceValue.getType()))
            builder.addAssignment (*v, sourceValue);
    }

    template <typename ArgTypeArray, typename ArgArray>
    void parseFunctionArguments (const FunctionParseState& state, ArgTypeArray& argTypes, ArgArray& args)
    {
        expect (HEARTOperator::openParen);

        if (! matchIf (HEARTOperator::closeParen))
        {
            for (;;)
            {
                auto& arg = parseExpression (state);
                args.push_back (arg);
                argTypes.push_back (arg.getType());

                if (matchIf (HEARTOperator::comma))
                    continue;

                expect (HEARTOperator::closeParen);
                break;
            }
        }
    }

    void parseFunctionCall (FunctionParseState& state, FunctionBuilder& builder, const AssignmentTarget& target)
    {
        auto errorLocation = location;
        auto name = readQualifiedIdentifier();

        ArrayWithPreallocation<Type, 8> argTypes;
        heart::FunctionCall::ArgListType args;
        parseFunctionArguments (state, argTypes, args);

        expectSemicolon();

        if (auto fn = findFunction (name, argTypes))
            return builder.addFunctionCall (target.create (state, builder, fn->returnType), *fn, std::move (args));

        errorLocation.throwError (Errors::unknownFunction (name));
    }

    static bool functionArgTypesMatch (const heart::Function& fn, ArrayView<Type> argTypes)
    {
        auto numParams = fn.parameters.size();

        if (numParams != argTypes.size())
            return false;

        for (size_t i = 0; i < numParams; ++i)
            if (! TypeRules::canPassAsArgumentTo (fn.parameters[i]->getType(), argTypes[i], true))
                return false;

        return true;
    }

    pool_ptr<heart::Function> findFunction (const std::string& name, ArrayView<Type> argTypes)
    {
        if (! containsChar (name, ':'))
        {
            for (auto& fn : module->functions)
                if (fn->name == name && functionArgTypesMatch (fn, argTypes))
                    return fn;
        }
        else
        {
            for (auto& m : program.getModules())
                for (auto& fn : m->functions)
                    if (TokenisedPathString::join (m->fullName, fn->name) == name && functionArgTypesMatch (fn, argTypes))
                        return fn;
        }

        return {};
    }

    bool parseTerminator (FunctionParseState& state, FunctionBuilder& builder)
    {
        if (matchIf ("branch"))
        {
            auto& dest = readBlockNameAndFind (state);
            expectSemicolon();
            builder.addBranch (dest, {});
            return true;
        }

        if (matchIf ("branch_if"))
        {
            auto& condition = parseExpression (state, PrimitiveType::bool_);
            expect (HEARTOperator::question);
            auto& trueBranch = readBlockNameAndFind (state);
            expect (HEARTOperator::colon);
            auto& falseBranch = readBlockNameAndFind (state);
            expectSemicolon();
            builder.addBranchIf (condition, trueBranch, falseBranch, {});
            return true;
        }

        if (matchIf ("return"))
        {
            if (matchIf (HEARTOperator::semicolon))
            {
                builder.addReturn();
                return true;
            }

            auto& value = parseExpression (state, state.function.returnType);
            expectSemicolon();
            builder.addReturn (value);
            return true;
        }

        return false;
    }

    void parseReadStream (FunctionParseState& state, FunctionBuilder& builder, const AssignmentTarget& target)
    {
        if (state.function.functionType.isUserInit())
            throwError (Errors::streamsCannotBeUsedDuringInit());

        auto name = parseIdentifier();
        auto src = module->findInput (name);

        if (src == nullptr)
            throwError (Errors::cannotFindInput (name));

        builder.addReadStream (location, *target.create (state, builder, src->getSingleDataType()), *src);
        expectSemicolon();
    }

    bool parseWriteStream (FunctionParseState& state, FunctionBuilder& builder)
    {
        auto writeStreamLocation = location;
        auto name = parseIdentifier();
        auto target = module->findOutput (name);

        pool_ptr<heart::Expression> index;

        if (state.function.functionType.isUserInit())
            throwError (Errors::streamsCannotBeUsedDuringInit());

        if (target == nullptr)
            throwError (Errors::cannotFindOutput (name));

        if (matchIf (HEARTOperator::openBracket))
        {
            index = parseExpression (state, PrimitiveType::int32);
            expect (HEARTOperator::closeBracket);
        }

        auto& value = parseExpression (state);
        const auto& type = value.getType();

        // Check that we are using indexes for array types
        if (index == nullptr)
        {
            if (! target->canHandleType (type))
                throwError (Errors::wrongTypeForEndpoint());
        }
        else
        {
            if (target->arraySize == 0)
                throwError (Errors::wrongTypeForEndpoint());

            bool validElementType = index == nullptr ? target->canHandleType (type)
                                                     : target->canHandleElementType (type);

            if (! validElementType)
                throwError (Errors::wrongTypeForEndpoint());
        }

        if (! (state.function.functionType.isRun() || target->isEventEndpoint()))
            throwError (Errors::streamsCanOnlyBeUsedInRun());

        builder.addWriteStream (writeStreamLocation, *target, index, value);
        expectSemicolon();
        return true;
    }

    heart::Block& findBlock (const FunctionParseState& state, Identifier name)
    {
        for (auto& b : state.blocks)
            if (b.block.name == name)
                return b.block;

        throwError (Errors::cannotFind (name));
        return state.blocks.front().block;
    }

    heart::Block& readBlockNameAndFind (const FunctionParseState& state)
    {
        return findBlock (state, readBlockName());
    }

    pool_ptr<heart::Variable> findVariable (const FunctionParseState& state, const std::string& name, bool includeStateVariables)
    {
        if (containsChar (name, ':'))
        {
            SOUL_ASSERT (name[0] == '$');
            TokenisedPathString path (name.substr (1));
            auto variableName = path.getLastPart();
            return program.getVariableWithName (TokenisedPathString::join (path.getParentPath(), "$" + variableName));
        }

        for (auto& v : state.variables)
            if (v->name == name)
                return v;

        for (auto& parameter : state.function.parameters)
            if (parameter->name == name)
                return parameter;

        if (includeStateVariables)
            for (auto& v : module->stateVariables)
                if (v->name == name)
                    return v;

        return program.getVariableWithName (name);
    }

    heart::Expression& parseArraySlice (const FunctionParseState& state, heart::Expression& lhs, int64_t start, int64_t end)
    {
        if (! lhs.getType().isValidArrayOrVectorRange (start, end))
            throwError (Errors::illegalSliceSize());

        auto& s = module->allocate<heart::ArrayElement> (location, lhs, (size_t) start, (size_t) end);
        return parseVariableSuffixes (state, s);
    }

    heart::Expression& parseVariableSuffixes (const FunctionParseState& state, heart::Expression& lhs)
    {
        if (matchIf (HEARTOperator::dot))
        {
            auto member = readIdentifier();

            if (! lhs.getType().isStruct())
                throwError (Errors::invalidDotArguments());

            auto& structure = lhs.getType().getStructRef();

            if (structure.hasMemberWithName (member))
                return parseVariableSuffixes (state, module->allocate<heart::StructElement> (location, lhs, member));

            throwError (Errors::unknownMemberInStruct (member, structure.getName()));
        }

        if (matchIf (HEARTOperator::openBracket))
        {
            if (matchIf (HEARTOperator::colon))
            {
                auto endIndex = parseInt32();
                expect (HEARTOperator::closeBracket);
                return parseArraySlice (state, lhs, 0, endIndex);
            }

            const auto& arrayOrVectorType = lhs.getType();

            auto& startIndex = parseExpression (state);

            if (matchIf (HEARTOperator::colon))
            {
                auto constStart = startIndex.getAsConstant();

                if (! constStart.getType().isPrimitiveInteger())
                    throwError (Errors::nonConstArraySize());

                if (matchIf (Operator::closeBracket))
                    return parseArraySlice (state, lhs, constStart.getAsInt64(),
                                            (int64_t) arrayOrVectorType.getArrayOrVectorSize());

                auto& endIndex = parseExpression (state);
                expect (HEARTOperator::closeBracket);

                auto constEnd = endIndex.getAsConstant();

                if (! constEnd.getType().isPrimitiveInteger())
                    throwError (Errors::nonConstArraySize());

                return parseArraySlice (state, lhs, constStart.getAsInt64(), constEnd.getAsInt64());
            }

            if (! (startIndex.getType().isPrimitiveInteger() || startIndex.getType().isBoundedInt()))
                throwError (Errors::nonIntegerArrayIndex());

            if (matchAndReplaceIf (Operator::closeDoubleBracket, Operator::closeBracket))
                return parseVariableSuffixes (state, module->allocate<heart::ArrayElement> (location, lhs, startIndex));

            expect (HEARTOperator::closeBracket);
            return parseVariableSuffixes (state, module->allocate<heart::ArrayElement> (location, lhs, startIndex));
        }

        return lhs;
    }

    heart::UnaryOperator& parseUnaryOp (const FunctionParseState& state, UnaryOp::Op opType)
    {
        auto errorPos = location;
        expect (HEARTOperator::openParen);
        auto& source = parseExpression (state);
        expect (HEARTOperator::closeParen);

        if (! UnaryOp::isTypeSuitable (opType, source.getType()))
            throwError (Errors::wrongTypeForUnary());

        return module->allocate<heart::UnaryOperator> (location, source, opType);
    }

    heart::BinaryOperator& parseBinaryOp (const FunctionParseState& state, BinaryOp::Op opType)
    {
        auto pos = location;
        expect (HEARTOperator::openParen);
        auto& lhs = parseExpression (state);
        expect (HEARTOperator::comma);
        auto& rhs = parseExpression (state);
        expect (HEARTOperator::closeParen);
        const auto& lhsType = lhs.getType();

        if (! lhsType.isEqual (rhs.getType(), Type::ignoreReferences))
            pos.throwError (Errors::illegalTypesForBinaryOperator (BinaryOp::getSymbol (opType),
                                                                   lhs.getType().getDescription(),
                                                                   rhs.getType().getDescription()));

        const auto& operandType = lhsType;
        auto binOpTypes = BinaryOp::getTypes (opType, operandType, operandType);

        if (! binOpTypes.operandType.isEqual (operandType, Type::ignoreReferences))
            pos.throwError (Errors::illegalTypesForBinaryOperator (BinaryOp::getSymbol (opType),
                                                                   lhs.getType().getDescription(),
                                                                   rhs.getType().getDescription()));

        return module->allocate<heart::BinaryOperator> (pos, lhs, rhs, opType);
    }

    heart::TypeCast& parseCast (const FunctionParseState& state)
    {
        auto pos = location;
        auto destType = readValueType();
        expect (HEARTOperator::openParen);
        auto& source = parseExpression (state);
        expect (HEARTOperator::closeParen);

        return module->allocate<heart::TypeCast> (pos, source, destType);
    }

    heart::Expression& parseExpression (const FunctionParseState& state)
    {
        #define SOUL_MATCH_BINARY_OP_NAME(name, op) \
            if (matchIf (#name)) return parseBinaryOp (state, BinaryOp::Op::name);
        SOUL_BINARY_OPS (SOUL_MATCH_BINARY_OP_NAME)
        #undef SOUL_MATCH_BINARY_OP_NAME

        #define SOUL_COMPARE_UNARY_OP(name, op) \
            if (matchIf (#name)) return parseUnaryOp (state, UnaryOp::Op::name);
        SOUL_UNARY_OPS (SOUL_COMPARE_UNARY_OP)
        #undef SOUL_COMPARE_UNARY_OP

        if (matches (Token::identifier))
        {
            if (currentStringValue[0] == '$')
            {
                auto errorPos = location;
                auto name = readQualifiedIdentifier();

                if (auto v = findVariable (state, name, true))
                    return parseVariableSuffixes (state, *v);

                errorPos.throwError (Errors::unresolvedSymbol (name));
            }

            if (matchIf ("cast"))
                return parseCast (state);

            auto infOrNaN = parseNaNandInfinityTokens();

            if (infOrNaN.isValid())
                return program.getAllocator().allocateConstant (infOrNaN);

            if (matchIf ("processor"))
                return parseProcessorProperty();
        }

        if (matches (Token::literalInt32))   return parseConstantAsExpression (state, PrimitiveType::int32);
        if (matches (Token::literalInt64))   return parseConstantAsExpression (state, PrimitiveType::int64);
        if (matches (Token::literalFloat32)) return parseConstantAsExpression (state, PrimitiveType::float32);
        if (matches (Token::literalFloat64)) return parseConstantAsExpression (state, PrimitiveType::float64);
        if (matches (Token::literalString))  return parseConstantAsExpression (state, Type::createStringLiteral());

        return parseConstantAsExpression (state, readValueType());
    }

    Value parseNaNandInfinityTokens()
    {
        if (matchIf ("_inf32"))     return soul::Value ( std::numeric_limits<float>::infinity());
        if (matchIf ("_ninf32"))    return soul::Value (-std::numeric_limits<float>::infinity());
        if (matchIf ("_nan32"))     return soul::Value ( std::numeric_limits<float>::quiet_NaN());

        if (matchIf ("_inf64"))     return soul::Value ( std::numeric_limits<double>::infinity());
        if (matchIf ("_ninf64"))    return soul::Value (-std::numeric_limits<double>::infinity());
        if (matchIf ("_nan64"))     return soul::Value ( std::numeric_limits<double>::quiet_NaN());

        return {};
    }

    heart::Expression& parseExpression (const FunctionParseState& state, const Type& requiredType)
    {
        auto errorPos = location;
        return checkExpressionType (parseExpression (state), requiredType, errorPos);
    }

    heart::Expression& checkExpressionType (heart::Expression& r, const Type& requiredType, const CodeLocation& errorPos)
    {
        auto constValue = r.getAsConstant();

        if (constValue.isValid() && TypeRules::canSilentlyCastTo (requiredType, constValue))
            return r;

        if (! TypeRules::canPassAsArgumentTo (requiredType, r.getType(), true))
            errorPos.throwError (Errors::expectedExpressionOfType (getTypeDescription (requiredType)));

        return r;
    }

    pool_ptr<heart::Expression> parseVariableExpression (const FunctionParseState& state)
    {
        if (matches (Token::identifier))
        {
            if (auto v = findVariable (state, currentStringValue, true))
            {
                skip();
                return parseVariableSuffixes (state, *v);
            }
        }

        return {};
    }

    heart::ProcessorProperty& parseProcessorProperty()
    {
        expect (HEARTOperator::dot);
        auto pos = location;
        auto property = heart::ProcessorProperty::getPropertyFromName (readIdentifier());

        if (property == heart::ProcessorProperty::Property::none)
            pos.throwError (Errors::unknownProperty());

        if (module->isNamespace())
            pos.throwError (Errors::processorPropertyUsedOutsideDecl());

        return module->allocate<heart::ProcessorProperty> (pos, property);
    }

    Value negate (const Value& v)
    {
        if (! v.canNegate())
            throwError (Errors::cannotNegateConstant());

        return v.negated();
    }

    heart::Expression& parseConstantAsExpression (const FunctionParseState& state, const Type& requiredType)
    {
        auto c = parseConstant (requiredType, true);
        return parseVariableSuffixes (state, program.getAllocator().allocateConstant (c));
    }

    Value castValue (const Value& v, const Type& destType)
    {
        return v.castToTypeWithError (destType, location);
    }

    Value parseConstant (const Type& requiredType, bool throwOnError)
    {
        if (matchIf (HEARTOperator::openBrace))
        {
            if (matchIf (HEARTOperator::closeBrace))
                return Value::zeroInitialiser (requiredType);

            if (requiredType.isVector())
                return Value::createArrayOrVector (requiredType,
                                                   parseConstantList (requiredType.getVectorElementType(),
                                                                      requiredType.getVectorSize()));

            if (requiredType.isArray())
                return Value::createArrayOrVector (requiredType,
                                                   parseConstantList (requiredType.getArrayElementType(),
                                                                      requiredType.getArraySize()));

            if (requiredType.isStruct())
            {
                auto& s = requiredType.getStructRef();
                ArrayWithPreallocation<Value, 8> memberValues;
                memberValues.reserve (s.getNumMembers());

                for (size_t i = 0; i < s.getNumMembers(); ++i)
                {
                    memberValues.push_back (parseConstant (s.getMemberType (i), true));

                    if (i == s.getNumMembers() - 1)
                        expect (HEARTOperator::closeBrace);
                    else
                        expect (HEARTOperator::comma);
                }

                return Value::createStruct (s, memberValues);
            }
        }

        if (matchIf (HEARTOperator::minus))
            return negate (parseConstant (requiredType, throwOnError));

        if (requiredType.isBoundedInt())
        {
            auto val = parseLiteralInt();

            if (! requiredType.isValidBoundedIntIndex (val))
                throwError (Errors::indexOutOfRange());

            return castValue (Value (val), requiredType);
        }

        if (requiredType.isFloat64())
        {
            if (matchIf (Token::literalInt32) || matchIf (Token::literalInt64))
                return Value ((double) literalIntValue);

            auto val = literalDoubleValue;
            expect (Token::literalFloat64);
            return castValue (Value (val), requiredType);
        }

        if (requiredType.isFloat32())
        {
            if (matchIf (Token::literalInt32) || matchIf (Token::literalInt64))
                return Value ((float) literalIntValue);

            auto val = literalDoubleValue;
            expect (Token::literalFloat32);
            return castValue (Value (val), requiredType);
        }

        if (requiredType.isInteger32())
        {
            auto val = literalIntValue;
            expect (Token::literalInt32);
            return castValue (Value (val), requiredType);
        }

        if (requiredType.isInteger64())
        {
            auto val = literalIntValue;
            expect (Token::literalInt64);
            return castValue (Value (val), requiredType);
        }

        if (requiredType.isBool())
        {
            if (matchIf ("true"))  return Value (true);
            if (matchIf ("false")) return Value (false);
        }

        if (requiredType.isArrayOrVector())
        {
            auto singleValue = parseConstant (requiredType.getElementType(), false);

            if (singleValue != Value())
                return castValue (singleValue, requiredType);
        }

        if (requiredType.isStringLiteral())
        {
            auto n = currentStringValue;
            expect (Token::literalString);
            return Value::createStringLiteral (program.getStringDictionary().getHandleForString (n));
        }

        if (throwOnError)
            throwError (Errors::expectedExpressionOfType (getTypeDescription (requiredType)));

        return {};
    }

    std::vector<Value> parseConstantList (const Type& requiredType, size_t num)
    {
        std::vector<Value> elements;
        elements.reserve (num);

        for (;;)
        {
            if (elements.size() == num)
            {
                expect (HEARTOperator::closeBrace);
                return elements;
            }

            elements.push_back (parseConstant (requiredType, true));

            if (elements.size() < num)
                expect (HEARTOperator::comma);
        }
    }

    Value parseInt32Value()     { return parseConstant (PrimitiveType::int32, true); }
    int32_t parseInt32()        { return parseInt32Value().getAsInt32(); }

    //==============================================================================
    void expectSemicolon()
    {
        expect (HEARTOperator::semicolon);
    }

    void skipPastNextOccurrenceOf (TokenType token)
    {
        while (! matchIf (token))
        {
            if (matchIf (HEARTOperator::openBrace))
            {
                skipPastNextOccurrenceOf (HEARTOperator::closeBrace);
                continue;
            }

            if (matches (Token::eof))
                expect (token);

            skip();
        }
    }

    std::string readQualifiedIdentifier()
    {
        auto part1 = readIdentifier();

        if (matchIf (HEARTOperator::doubleColon))
            return TokenisedPathString::join (part1, readQualifiedIdentifier());

        return part1;
    }

    Identifier parseIdentifier()
    {
        return program.getAllocator().get (readIdentifier());
    }

    int64_t parseLiteralInt()
    {
        auto n = literalIntValue;
        expect (Token::literalInt32);
        return n;
    }

    void checkVersionDeclaration()
    {
        expect (HEARTOperator::hash);
        expect (getHEARTFormatVersionPrefix());

        auto version = parseLiteralInt();

        if (version <= 0)
            throwError (Errors::expectedVersionNumber());

        if (version > getHEARTFormatVersion())
            throwError (Errors::wrongAPIVersion());
    }

    uint32_t parseVersionElement()
    {
        auto v = (uint32_t) parseLiteralInt();

        if (! Version::isValidElementValue (v))
            throwError (Errors::expectedVersionNumber());

        return v;
    }

    Identifier readBlockName()
    {
        expect (HEARTOperator::at);
        return program.getAllocator().get ("@" + readIdentifier());
    }

    StructurePtr findStruct (const std::string& name)
    {
        if (auto s = module->findStruct (name))
            return s;

        for (auto& m : program.getModules())
            for (auto& s : m->structs)
                if (program.getFullyQualifiedStructName (*s) == name)
                    return s;

        return {};
    }

    Type readValueType()
    {
        if (matchIf ("float32"))  return parseVectorOrArrayTypeSuffixes (PrimitiveType::float32);
        if (matchIf ("float64"))  return parseVectorOrArrayTypeSuffixes (PrimitiveType::float64);
        if (matchIf ("fixed"))    return parseVectorOrArrayTypeSuffixes (PrimitiveType::fixed);
        if (matchIf ("void"))     return parseVectorOrArrayTypeSuffixes (PrimitiveType::void_);
        if (matchIf ("int32"))    return parseVectorOrArrayTypeSuffixes (PrimitiveType::int32);
        if (matchIf ("int64"))    return parseVectorOrArrayTypeSuffixes (PrimitiveType::int64);
        if (matchIf ("bool"))     return parseVectorOrArrayTypeSuffixes (PrimitiveType::bool_);
        if (matchIf ("string"))   return parseArrayTypeSuffixes (Type::createStringLiteral());
        if (matchIf ("wrap"))     return parseBoundedIntType (true);
        if (matchIf ("clamp"))    return parseBoundedIntType (false);

        auto errorPos = location;
        auto name = readQualifiedIdentifier();

        if (auto s = findStruct (name))
            return parseArrayTypeSuffixes (Type::createStruct (*s));

        errorPos.throwError (Errors::unresolvedType (name));
        return {};
    }

    Type readValueOrRefType()
    {
        auto t = readValueType();

        if (matchIf (HEARTOperator::bitwiseAnd))
            return t.createReference();

        return t;
    }

    std::vector<Type> readEventTypeList()
    {
        std::vector<Type> result;

        if (matchIf (HEARTOperator::openParen))
        {
            for (;;)
            {
                result.push_back (readValueType());

                if (! matchIf (HEARTOperator::comma))
                    break;
            }

            expect (HEARTOperator::closeParen);
        }
        else
        {
            result.push_back (readValueType());
        }

        return result;
    }

    Type parseVectorOrArrayTypeSuffixes (PrimitiveType elementType)
    {
        if (matchIf (HEARTOperator::lessThan))
        {
            if (! elementType.canBeVectorElementType())
                throwError (Errors::wrongTypeForArrayElement());

            auto size = parseLiteralInt();
            expect (HEARTOperator::greaterThan);

            if (! Type::isLegalVectorSize (size))
                throwError (Errors::illegalVectorSize());

            return parseArrayTypeSuffixes (Type::createVector (elementType, static_cast<Type::ArraySize> (size)));
        }

        return parseArrayTypeSuffixes (elementType);
    }

    Type parseArrayTypeSuffixes (Type elementType)
    {
        if (matchIf (HEARTOperator::openBracket))
        {
            if (matchIf (HEARTOperator::closeBracket))
                return parseArrayTypeSuffixes (elementType.createUnsizedArray());

            auto size = parseLiteralInt();

            if (! Type::canBeSafelyCastToArraySize (size))
                throwError (Errors::illegalSize());

            expect (HEARTOperator::closeBracket);
            return parseArrayTypeSuffixes (elementType.createArray (static_cast<Type::ArraySize> (size)));
        }

        return checkTypeSize (elementType);
    }

    Type checkTypeSize (Type type)
    {
        if (! type.isUnsizedArray() && type.isPackedSizeTooBig())
            throwError (Errors::typeTooBig (getReadableDescriptionOfByteSize (type.getPackedSizeInBytes()),
                                            getReadableDescriptionOfByteSize (Type::maxPackedObjectSize)));

        return type;
    }

    Type parseBoundedIntType (bool isWrap)
    {
        expect (Operator::lessThan);
        auto size = parseLiteralInt();
        expect (Operator::greaterThan);

        if (! Type::isLegalBoundedIntSize (size))
            throwError (Errors::illegalSize());

        auto boundedSize = static_cast<Type::BoundedIntSize> (size);
        return parseArrayTypeSuffixes (isWrap ? Type::createWrappedInt (boundedSize)
                                              : Type::createClampedInt (boundedSize));
    }

    std::string getTypeDescription (const Type& t) const
    {
        return program.getTypeDescriptionWithQualificationIfNeeded (module, t);
    }
};

} // namespace soul
