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
    static TokenType match (int, choc::text::UTF8Pointer) noexcept  { return {}; }
};

//==============================================================================
namespace Token
{
    SOUL_DECLARE_TOKEN (variableIdentifier,  "$variableIdentifier")
    SOUL_DECLARE_TOKEN (blockIdentifier,     "$blockIdentifier")
}

//==============================================================================
struct IdentifierMatcher
{
    static constexpr bool isIdentifierAnywhere (UnicodeChar c) noexcept  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    static constexpr bool isIdentifierStart    (UnicodeChar c) noexcept  { return isIdentifierAnywhere (c) || c == '$' || c == '@'; }
    static constexpr bool isIdentifierBody     (UnicodeChar c) noexcept  { return isIdentifierAnywhere (c) || (c >= '0' && c <= '9'); }

    static TokenType categoriseIdentifier (const std::string& identifier) noexcept
    {
        switch (identifier.at (0))
        {
            case '$': return Token::variableIdentifier;
            case '@': return Token::blockIdentifier;
            default:  return Token::identifier;
        }
    }
};

//==============================================================================
namespace HEARTOperator
{
    // NB: declaration order matters here for operators of different lengths that start the same way
    #define SOUL_HEART_OPERATORS(X) \
        X(semicolon,          ";")      X(dot,                ".")  \
        X(comma,              ",") \
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
        static TokenType match (choc::text::UTF8Pointer& text) noexcept
        {
            #define SOUL_COMPARE_OPERATOR(name, str) if (text.skipIfStartsWith (str)) return name;
            SOUL_HEART_OPERATORS (SOUL_COMPARE_OPERATOR)
            #undef SOUL_COMPARE_OPERATOR
            return {};
        }
    };
}


//==============================================================================
struct FunctionParseState
{
    FunctionParseState (heart::Function& f) : function (f) {}

    struct BlockCode
    {
        heart::Block& block;
        choc::text::UTF8Pointer code;
    };

    void setCurrentBlock (BlockCode& b)
    {
        currentBlock = &b;
    }

    heart::Function& function;
    std::vector<BlockCode> blocks;
    std::vector<pool_ptr<heart::Variable>> variables;
    BlockCode* currentBlock = nullptr;

};

//==============================================================================
struct heart::Parser   : public Tokeniser<DummyKeywordMatcher,
                                          HEARTOperator::Matcher,
                                          IdentifierMatcher>
{
    static Program parse (const CodeLocation& code)
    {
        return heart::Parser (code).parse();
    }

    static Type parsePrimitiveType (const CodeLocation& code)
    {
        return heart::Parser (code).readPrimitiveValueType();
    }

    [[noreturn]] void throwError (const CompileMessage& message) const override
    {
        soul::throwError (message.withLocation (location));
    }

private:
    using Operator = void; // Prevents anything in this class mistakenly using Operator instead of HEARTOperator

    //==============================================================================
    struct ScannedTopLevelItem
    {
        ScannedTopLevelItem (Module& m) : module (m) {}

        Module& module;
        choc::text::UTF8Pointer moduleStartPos;
        std::vector<choc::text::UTF8Pointer> functionParamCode, functionBodyCode, structBodyCode, inputDecls, outputDecls, stateVariableDecls;
    };

    Program program;
    pool_ptr<Module> module;

    //==============================================================================
    Parser (const CodeLocation& text)  { initialise (text); }
    ~Parser() override = default;

    //==============================================================================
    Program parse()
    {
        checkVersionDeclaration();

        std::vector<ScannedTopLevelItem> scannedTopLevelItems;
        scannedTopLevelItems.reserve (128);

        while (! matches (Token::eof))
        {
            auto moduleLocation = location;

            if (matchIf ("graph"))              scanTopLevelItem (moduleLocation, scannedTopLevelItems, program.addGraph());
            else if (matchIf ("processor"))     scanTopLevelItem (moduleLocation, scannedTopLevelItems, program.addProcessor());
            else if (matchIf ("namespace"))     scanTopLevelItem (moduleLocation, scannedTopLevelItems, program.addNamespace());
            else                                throwError (Errors::expectedTopLevelDecl());
        }

        for (auto& item : scannedTopLevelItems)  parseModuleStructs (item);
        for (auto& item : scannedTopLevelItems)  parseFunctionDecls (item);
        for (auto& item : scannedTopLevelItems)  parseStateVariables (item);
        for (auto& item : scannedTopLevelItems)  parseModule (item);

        return program;
    }

    void scanTopLevelItem (const CodeLocation& moduleLocation, std::vector<ScannedTopLevelItem>& scannedTopLevelItems, Module& newModule)
    {
        ScannedTopLevelItem newItem (newModule);
        module = newModule;

        newModule.location = moduleLocation;
        newModule.fullName = readQualifiedGeneralIdentifier();
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
            parseStructBody (*module->structs.get()[i]);
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
            parseFunctionParams (module->functions.at (i));
        }

        module.reset();
    }

    void parseStateVariables (ScannedTopLevelItem& item)
    {
        prepareToRescan (item);

        if (! item.stateVariableDecls.empty())
        {
            for (auto& g : item.stateVariableDecls)
            {
                resetPosition (g);
                parseStateVariable();
            }
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
            if (matchIf ("node"))        return parseNode();
            if (matchIf ("connection"))  return parseConnection();
            if (matchIf ("processor"))   return parseLatency();
        }

        if (matchIf ("struct"))      return scanStruct (item);
        if (matchIf ("function"))    return scanFunction (item, false);
        if (matchIf ("var"))         return scanStateVariable (item, false);

        if (module->isProcessor())
        {
            if (matchIf ("event"))       return scanFunction (item, true);
            if (matchIf ("processor"))   return parseLatency();
        }

        if (matchIf ("let"))
            return scanStateVariable (item, true);

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
            if (item.functionBodyCode[i] != choc::text::UTF8Pointer())
            {
                resetPosition (item.functionBodyCode[i]);
                parseFunctionBody (module->functions.at (i));
            }
        }

        resetPosition (nextItemPos);
    }

    void scanInput (ScannedTopLevelItem& item)
    {
        item.inputDecls.push_back (getCurrentTokeniserPosition());
        auto& inputDeclaration = module->allocate<heart::InputDeclaration> (location);
        inputDeclaration.name = parseGeneralIdentifier();

        auto errorLocation = location;
        if (isReservedFunctionName (inputDeclaration.name))
            errorLocation.throwError (Errors::invalidEndpointName (inputDeclaration.name));

        inputDeclaration.index = (uint32_t) module->inputs.size();

        module->inputs.push_back (inputDeclaration);
        skipPastNextOccurrenceOf (HEARTOperator::semicolon);
    }

    void scanOutput (ScannedTopLevelItem& item)
    {
        item.outputDecls.push_back (getCurrentTokeniserPosition());
        auto& outputDeclaration = module->allocate<heart::OutputDeclaration> (location);
        outputDeclaration.name = parseGeneralIdentifier();

        auto errorLocation = location;
        if (isReservedFunctionName (outputDeclaration.name))
            errorLocation.throwError (Errors::invalidEndpointName (outputDeclaration.name));

        module->outputs.push_back (outputDeclaration);
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
        inputDeclaration.name = parseGeneralIdentifier();

        if (matchIf (HEARTOperator::openBracket))
        {
            inputDeclaration.arraySize = parseProcessorArraySize();
            expect (HEARTOperator::closeBracket);
        }

        inputDeclaration.endpointType = parseEndpointType (*this);
        inputDeclaration.dataTypes = readEventTypeList();

        inputDeclaration.checkDataTypesValid (location);
        parseAnnotation (inputDeclaration.annotation);
        expectSemicolon();
    }

    void parseOutput (heart::OutputDeclaration& output)
    {
        output.name = parseGeneralIdentifier();

        if (matchIf (HEARTOperator::openBracket))
        {
            output.arraySize = parseProcessorArraySize();
            expect (HEARTOperator::closeBracket);
        }

        output.endpointType = parseEndpointType (*this);
        output.dataTypes = readEventTypeList();

        output.checkDataTypesValid (location);
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
                auto name  = matchIf (Token::literalString) ? currentStringValue : readGeneralIdentifier();
                auto value = matchIf (HEARTOperator::colon) ? parseAnnotationValue() : Value (true);

                annotation.set (name, value, program.getStringDictionary());
            }
            while (matchIf (HEARTOperator::comma));

            expect (HEARTOperator::closeDoubleBracket);
        }
    }

    Value parseAnnotationValue()
    {
        if (matches (Token::literalInt32))       { auto v = literalIntValue;    skip(); return Value::createInt32 (v); }
        if (matches (Token::literalInt64))       { auto v = literalIntValue;    skip(); return Value::createInt64 (v); }
        if (matches (Token::literalFloat32))     { auto v = literalDoubleValue; skip(); return Value ((float) v); }
        if (matches (Token::literalFloat64))     { auto v = literalDoubleValue; skip(); return Value (v); }
        if (matches (Token::literalString))      { auto v = program.getStringDictionary().getHandleForString (currentStringValue); skip(); return Value::createStringLiteral(v); }
        if (matchIf ("true"))                    { return Value (true); }
        if (matchIf ("false"))                   { return Value (false); }
        if (matchIf (HEARTOperator::minus))      { return negate (parseAnnotationValue()); }
        if (matches (Token::variableIdentifier)) { location.throwError (Errors::propertyMustBeConstant()); }

        auto infOrNaN = parseNaNandInfinityTokens();

        if (infOrNaN.isValid())
            return infOrNaN;

        location.throwError (Errors::missingAnnotationValue());
    }

    void parseNode()
    {
        auto name = readQualifiedGeneralIdentifier();

        auto& mi = module->allocate<heart::ProcessorInstance> (location);
        module->processorInstances.push_back (mi);
        mi.instanceName = name;
        expect (HEARTOperator::assign);
        mi.sourceName = readQualifiedGeneralIdentifier();

        if (matchIf (HEARTOperator::openBracket))
        {
            mi.arraySize = parseProcessorArraySize();
            expect (HEARTOperator::closeBracket);
        }

        if (matchIf (HEARTOperator::times))
        {
            auto errorPos = location;
            mi.clockMultiplier.setMultiplier (errorPos, parseInt32Value());
        }
        else if (matchIf (HEARTOperator::divide))
        {
            auto errorPos = location;
            mi.clockMultiplier.setDivider (errorPos, parseInt32Value());
        }

        expectSemicolon();
    }

    void parseConnection()
    {
        auto& c = module->allocate<heart::Connection> (location);
        module->connections.push_back (c);

        c.interpolationType = parseInterpolationType (*this);
        auto src = readProcessorAndChannel();
        c.source.processor = src.processor;
        c.source.endpointName = src.endpoint;
        c.source.endpointIndex = src.endpointIndex;
        expect (HEARTOperator::rightArrow);

        if (matchIf (HEARTOperator::openBracket))
        {
            c.delayLength = parseInt32();

            expect (HEARTOperator::closeBracket);
            expect (HEARTOperator::rightArrow);
        }

        auto dst = readProcessorAndChannel();
        c.dest.processor = dst.processor;
        c.dest.endpointName = dst.endpoint;
        c.dest.endpointIndex = dst.endpointIndex;

        expectSemicolon();
    }

    struct ProcessorAndChannel
    {
        pool_ptr<heart::ProcessorInstance> processor;
        std::string endpoint;
        std::optional<size_t> endpointIndex;
    };

    ProcessorAndChannel readProcessorAndChannel()
    {
        ProcessorAndChannel processorAndChannel;

        auto name = readQualifiedGeneralIdentifier();

        if (matchIf (HEARTOperator::dot))
        {
            processorAndChannel.processor = findProcessorInstance (name);
            processorAndChannel.endpoint   = readGeneralIdentifier();
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
        pool_ptr<heart::ProcessorInstance> result;

        for (auto m : module->processorInstances)
        {
            if (m->instanceName == instanceName)
            {
                result = m;
                break;
            }
        }

        if (result == nullptr)
            throwError (Errors::cannotFindProcessor (instanceName));

        return result;
    }

    void scanStateVariable (ScannedTopLevelItem& item, bool isConstant)
    {
        ignoreUnused (isConstant);

        item.stateVariableDecls.push_back (getCurrentTokeniserPosition());
        skipPastNextOccurrenceOf (HEARTOperator::semicolon);
    }

    void parseStateVariable()
    {
        bool isExternal = matchIf ("external");
        auto type = readValueType();
        auto name = program.getAllocator().get (readVariableIdentifier());

        if (module->stateVariables.find (name))
            throwError (Errors::nameInUse (name));

        auto& v = module->allocate<heart::Variable> (location, type, name,
                                                     isExternal ? heart::Variable::Role::external
                                                                : heart::Variable::Role::state);

        if (matchIf (HEARTOperator::assign))
        {
            FunctionParseState parseState (module->allocate<heart::Function>());

            if (matches (HEARTOperator::openParen))
                v.initialValue = parseInitialiserList (parseState, type);
            else
                v.initialValue = parseExpression (parseState);
        }

        parseAnnotation (v.annotation);

        module->stateVariables.add (v);
        expectSemicolon();
    }

    void scanStruct (ScannedTopLevelItem& item)
    {
        auto name = readQualifiedGeneralIdentifier();

        if (module->structs.find (name) != nullptr)
            throwError (Errors::nameInUse (name));

        module->structs.add (std::move (name));
        expect (HEARTOperator::openBrace);
        item.structBodyCode.push_back (getCurrentTokeniserPosition());
        skipPastNextOccurrenceOf (HEARTOperator::closeBrace);
    }

    void parseStructBody (Structure& s)
    {
        while (! matchIf (HEARTOperator::closeBrace))
        {
            auto typeLocation = location;

            auto type = readValueType();
            auto name = readGeneralIdentifier();
            expectSemicolon();

            if (s.hasMemberWithName (name))
                throwError (Errors::nameInUse (name));

            auto structType = Type::createStruct (s);

            if (type.isEqual (structType, Type::failOnAllDifferences))
                typeLocation.throwError (Errors::typeContainsItself (s.getName()));

            if (type.isStruct() && type.getStruct()->containsMemberOfType (Type::createStruct (s), true))
                typeLocation.throwError (Errors::typesReferToEachOther (s.getName(), type.getStruct()->getName()));

            s.addMember (type, name);
        }
    }

    void scanFunction (ScannedTopLevelItem& item, bool isEventFunction)
    {
        auto name = parseGeneralIdentifier();

        if (isEventFunction && heart::isReservedFunctionName (name))
            throwError (Errors::invalidEndpointName (name));

        if (module->functions.find (name) != nullptr)
            throwError (Errors::nameInUse (name));

        auto& fn = module->functions.add (name, isEventFunction);

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
    }

    void parseFunctionParams (heart::Function& f)
    {
        FunctionParseState state (f);

        if (! matchIf (HEARTOperator::closeParen))
        {
            for (;;)
            {
                auto type = readValueOrRefType();
                auto paramLocation = location;
                auto name = parseVariableIdentifier();
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
            f.returnType = readValueOrRefType();
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

        if (matchIf (HEARTOperator::closeBrace))
            f.location.throwError (Errors::emptyFunction (f.name));

        scanBlocks (state, builder);

        builder.beginFunction (f);

        while (true)
        {
            int blocksProcessed = 0;
            int errors = 0;

            CompileMessageGroup firstError;

            {
                CompileMessageHandler handler ([&] (const CompileMessageGroup& message)
                                               {
                                                   if (firstError.messages.empty())
                                                       firstError = message;
                                               });

                for (auto& b : state.blocks)
                {
                    if (b.block.processed)
                        continue;

                    resetPosition (b.code);
                    builder.beginBlock (b.block);
                    auto variableCount = state.variables.size();
                    state.setCurrentBlock (b);

                    try
                    {
                        while (! parseTerminator (state, builder))
                            if (! parseStatement (state, builder))
                                throwError (Errors::expectedStatement());

                        blocksProcessed++;
                        b.block.processed = true;
                    }
                    catch (const AbortCompilationException& message)
                    {
                        b.block.statements.clear();
                        state.variables.resize (variableCount);
                        errors++;
                    }
                }
            }

            if (errors == 0)
                break;

            if (blocksProcessed == 0)
                soul::throwError (firstError);
        }

        builder.endFunction();
    }

    void scanBlocks (FunctionParseState& state, FunctionBuilder& builder)
    {
        for (;;)
        {
            auto name = parseBlockName();

            for (auto& b : state.blocks)
                if (b.block.name == name)
                    throwError (Errors::nameInUse (name));

            auto& block = builder.createBlock (name);

            if (matchIf (HEARTOperator::openParen))
            {
                if (! matchIf (HEARTOperator::closeParen))
                {
                    for (;;)
                    {
                        auto paramType = readValueOrRefType();
                        auto paramLocation = location;
                        auto paramName = parseVariableIdentifier();
                        block.parameters.push_back (module->allocate<heart::Variable> (std::move (paramLocation), paramType, paramName, heart::Variable::Role::parameter));

                        if (matchIf (HEARTOperator::comma))
                            continue;

                        expect (HEARTOperator::closeParen);
                        break;
                    }
                }
            }

            expect (HEARTOperator::colon);
            state.blocks.push_back ({ block, getCurrentTokeniserPosition() });

            skipPastNextOccurrenceOf (HEARTOperator::semicolon);

            while (! matches (Token::blockIdentifier))
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
            auto name = readVariableIdentifier();

            if (findVariable (state, name) != nullptr)
                throwError (Errors::nameInUse (name));

            parseVariableAssignment (state, builder, { nullptr, name, true, false });
            return true;
        }

        if (matchIf ("write"))
            return parseWriteStream (state, builder);

        if (matchIf ("advance"))
        {
            if (! state.function.functionType.isRun())
                location.throwError (Errors::advanceCannotBeCalledHere());

            expectSemicolon();
            builder.addAdvance (location);
            return true;
        }

        if (matchIf ("call"))
        {
            parseFunctionCall (state, builder, { nullptr, {}, false, true });
            return true;
        }

        if (matchesAnyIdentifier())
        {
            if (auto existingVariableTarget = parseVariableExpression (state))
            {
                if (! existingVariableTarget->isMutable())
                    location.throwError (Errors::operatorNeedsAssignableTarget ("="));

                parseVariableAssignment (state, builder, { existingVariableTarget, {}, false, false });
                return true;
            }

            auto newVariableName = readVariableIdentifier();
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

    heart::Expression& parsePureFunctionCall (const FunctionParseState& state)
    {
        auto errorLocation = location;
        auto name = readQualifiedGeneralIdentifier();

        ArrayWithPreallocation<Type, 8> argTypes;
        heart::FunctionCall::ArgListType args;
        parseFunctionArguments (state, argTypes, args);

        if (auto fn = findFunction (name, argTypes))
        {
            auto& f =  module->allocate<heart::PureFunctionCall> (errorLocation, *fn);
            f.arguments = args;

            return f;
        }

        errorLocation.throwError (Errors::unknownFunction (name));
    }

    void parseFunctionCall (FunctionParseState& state, FunctionBuilder& builder, const AssignmentTarget& target)
    {
        auto errorLocation = location;
        auto name = readQualifiedGeneralIdentifier();

        ArrayWithPreallocation<Type, 8> argTypes;
        heart::FunctionCall::ArgListType args;
        parseFunctionArguments (state, argTypes, args);

        expectSemicolon();

        if (auto fn = findFunction (name, argTypes))
            return builder.addFunctionCall (target.create (state, builder, fn->returnType), *fn, std::move (args));

        errorLocation.throwError (Errors::unknownFunction (name));
    }

    static bool functionArgTypesMatch (const heart::Function& fn, choc::span<Type> argTypes)
    {
        auto numParams = fn.parameters.size();

        if (numParams != argTypes.size())
            return false;

        for (size_t i = 0; i < numParams; ++i)
            if (! TypeRules::canPassAsArgumentTo (fn.parameters[i]->getType(), argTypes[i], true))
                return false;

        return true;
    }

    pool_ptr<heart::Function> findFunction (const std::string& name, choc::span<Type> argTypes)
    {
        if (! containsChar (name, ':'))
        {
            for (auto& fn : module->functions.get())
                if (fn->name == name && functionArgTypesMatch (fn, argTypes))
                    return fn;
        }
        else
        {
            for (auto& m : program.getModules())
                for (auto& fn : m->functions.get())
                    if (TokenisedPathString::join (m->fullName, fn->name) == name && functionArgTypesMatch (fn, argTypes))
                        return fn;
        }

        return {};
    }

    bool parseTerminator (FunctionParseState& state, FunctionBuilder& builder)
    {
        if (matchIf ("branch"))
        {
            auto& dest = parseBlockNameAndFind (state);
            auto destArgs = parseOptionalBranchArgs<heart::Branch::ArgListType> (state);
            expectSemicolon();
            builder.addBranch (dest, std::move (destArgs), {});
            return true;
        }

        if (matchIf ("branch_if"))
        {
            auto& condition = parseExpression (state, PrimitiveType::bool_);
            expect (HEARTOperator::question);
            auto& trueBranch = parseBlockNameAndFind (state);
            auto trueBranchArgs = parseOptionalBranchArgs<heart::BranchIf::ArgListType> (state);
            expect (HEARTOperator::colon);
            auto& falseBranch = parseBlockNameAndFind (state);
            auto falseBranchArgs = parseOptionalBranchArgs<heart::BranchIf::ArgListType> (state);
            expectSemicolon();
            builder.addBranchIf (condition, trueBranch, std::move (trueBranchArgs), falseBranch, std::move (falseBranchArgs), {});
            return true;
        }

        if (matchIf ("return"))
        {
            if (matchIf (HEARTOperator::semicolon))
            {
                if (state.function.returnType.isValid() && (! state.function.returnType.isVoid()))
                    location.throwError (Errors::expectedExpressionOfType (getTypeDescription (state.function.returnType)));

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

    template <class ArgListType>
    ArgListType parseOptionalBranchArgs (const FunctionParseState& state)
    {
        heart::FunctionCall::ArgListType args;

        if (matchIf (HEARTOperator::openParen))
        {
            if (! matchIf (HEARTOperator::closeParen))
            {
                for (;;)
                {
                    auto& arg = parseExpression (state);
                    args.push_back (arg);

                    if (matchIf (HEARTOperator::comma))
                        continue;

                    expect (HEARTOperator::closeParen);
                    break;
                }
            }
        }

        return args;
    }

    void parseReadStream (FunctionParseState& state, FunctionBuilder& builder, const AssignmentTarget& target)
    {
        auto name = parseGeneralIdentifier();
        auto src = module->findInput (name);

        if (src == nullptr)
            throwError (Errors::cannotFindInput (name));

        builder.addReadStream (location, *target.create (state, builder, src->getSingleDataType()), *src);
        expectSemicolon();
    }

    bool parseWriteStream (FunctionParseState& state, FunctionBuilder& builder)
    {
        auto writeStreamLocation = location;
        auto name = parseGeneralIdentifier();
        auto target = module->findOutput (name);

        pool_ptr<heart::Expression> index;

        if (target == nullptr)
            throwError (Errors::cannotFindOutput (name));

        if (matchIf (HEARTOperator::openBracket))
        {
            index = parseExpression (state, PrimitiveType::int32);
            expect (HEARTOperator::closeBracket);
        }

        auto& value = parseExpression (state);

        builder.addWriteStream (writeStreamLocation, *target, index, value);
        expectSemicolon();
        return true;
    }

    void parseLatency()
    {
        expect (HEARTOperator::dot);
        expect ("latency");
        expect (HEARTOperator::assign);
        auto errorPos = location;
        auto latency = parseInt32Value().getAsInt64();
        expectSemicolon();

        if (latency < 0 || latency > AST::maxInternalLatency)
            errorPos.throwError (Errors::latencyOutOfRange());

        module->latency = static_cast<uint32_t> (latency);
    }

    heart::Block& getBlock (const FunctionParseState& state, Identifier name)
    {
        auto b = findBlock (state, name);

        if (b == nullptr)
            throwError (Errors::cannotFind (name));

        return *b;
    }

    pool_ptr<heart::Block> findBlock (const FunctionParseState& state, Identifier name)
    {
        for (auto& b : state.blocks)
            if (b.block.name == name)
                return b.block;

        return {};
    }

    heart::Block& parseBlockNameAndFind (const FunctionParseState& state)
    {
        return getBlock (state, parseBlockName());
    }

    pool_ptr<heart::Variable> findVariable (const FunctionParseState& state, const std::string& name)
    {
        if (containsChar (name, ':'))
        {
            TokenisedPathString path (name);
            auto variableName = path.getLastPart();
            return program.findVariableWithName (TokenisedPathString::join (path.getParentPath(), variableName));
        }

        for (auto& v : state.variables)
            if (v->name == name)
                return v;

        for (auto& parameter : state.function.parameters)
            if (parameter->name == name)
                return parameter;

        if (auto stateVariable = module->stateVariables.find (name))
            return stateVariable;

        if (state.currentBlock != nullptr)
        {
            for (auto& blockParameter : state.currentBlock->block.parameters)
                if (blockParameter->name == name)
                    return blockParameter;
        }

        return program.findVariableWithName (name);
    }

    heart::Expression& parseArraySlice (const FunctionParseState& state, heart::Expression& lhs,
                                        int64_t start, int64_t end, bool isRangeTrusted)
    {
        if (! lhs.getType().isArrayOrVector())
            throwError (Errors::targetIsNotAnArray());

        if (! lhs.getType().isValidArrayOrVectorRange (start, end))
            throwError (Errors::illegalSliceSize());

        auto& s = module->allocate<heart::ArrayElement> (location, lhs, (size_t) start, (size_t) end);
        s.isRangeTrusted = isRangeTrusted;
        return parseSuffixOperators (state, s);
    }

    heart::Expression& parseSuffixOperators (const FunctionParseState& state, heart::Expression& lhs)
    {
        if (matchIf (HEARTOperator::dot))
        {
            auto member = readGeneralIdentifier();

            if (! lhs.getType().isStruct())
                throwError (Errors::invalidDotArguments());

            auto& structure = lhs.getType().getStructRef();

            if (structure.hasMemberWithName (member))
                return parseSuffixOperators (state, module->allocate<heart::StructElement> (location, lhs, member));

            throwError (Errors::unknownMemberInStruct (member, structure.getName()));
        }

        if (matchIf (HEARTOperator::openBracket))
        {
            bool isRangeTrusted = matchIf ("trusted");
            auto pos = location;

            if (matchIf (HEARTOperator::colon))
            {
                auto endIndex = parseInt32();
                expect (HEARTOperator::closeBracket);
                return parseArraySlice (state, lhs, 0, endIndex, isRangeTrusted);
            }

            const auto& arrayOrVectorType = lhs.getType();

            auto& startIndex = parseExpression (state);

            if (matchIf (HEARTOperator::colon))
            {
                auto constStart = startIndex.getAsConstant();

                if (! constStart.getType().isPrimitiveInteger())
                    throwError (Errors::nonConstArraySize());

                if (matchIf (HEARTOperator::closeBracket))
                    return parseArraySlice (state, lhs, constStart.getAsInt64(),
                                            (int64_t) arrayOrVectorType.getArrayOrVectorSize(), isRangeTrusted);

                auto& endIndex = parseExpression (state);
                expect (HEARTOperator::closeBracket);

                auto constEnd = endIndex.getAsConstant();

                if (! constEnd.getType().isPrimitiveInteger())
                    throwError (Errors::nonConstArraySize());

                return parseArraySlice (state, lhs, constStart.getAsInt64(), constEnd.getAsInt64(), isRangeTrusted);
            }

            if (! (startIndex.getType().isPrimitiveInteger() || startIndex.getType().isBoundedInt()))
                throwError (Errors::nonIntegerArrayIndex());

            if (matchAndReplaceIf (HEARTOperator::closeDoubleBracket, HEARTOperator::closeBracket))
            {
                auto& element = module->allocate<heart::ArrayElement> (pos, lhs, startIndex);
                element.isRangeTrusted = isRangeTrusted;
                return parseSuffixOperators (state, element);
            }

            expect (HEARTOperator::closeBracket);

            if (! lhs.getType().isArrayOrVector())
                location.throwError (Errors::expectedArrayOrVector());

            auto& element = module->allocate<heart::ArrayElement> (pos, lhs, startIndex);
            element.isRangeTrusted = isRangeTrusted;
            return parseSuffixOperators (state, element);
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

        if (! lhsType.isEqual (rhs.getType(), Type::ignoreReferences | Type::ignoreConst))
            pos.throwError (Errors::illegalTypesForBinaryOperator (BinaryOp::getSymbol (opType),
                                                                   lhs.getType().getDescription(),
                                                                   rhs.getType().getDescription()));

        const auto& operandType = lhsType;
        auto binOpTypes = BinaryOp::getTypes (opType, operandType, operandType);

        if (! binOpTypes.operandType.isEqual (operandType, Type::ignoreReferences | Type::ignoreConst))
            pos.throwError (Errors::illegalTypesForBinaryOperator (BinaryOp::getSymbol (opType),
                                                                   lhs.getType().getDescription(),
                                                                   rhs.getType().getDescription()));

        return module->allocate<heart::BinaryOperator> (pos, lhs, rhs, opType);
    }

    heart::TypeCast& parseCast (const FunctionParseState& state)
    {
        auto pos = location;
        auto destType = readValueOrRefType();
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

        if (matches (Token::variableIdentifier))
        {
            auto errorPos = location;
            auto name = readQualifiedVariableIdentifier();

            if (name == "tmpVar")
                findVariable (state, name);
            
            if (auto v = findVariable (state, name))
                return parseSuffixOperators (state, *v);

            errorPos.throwError (Errors::unresolvedSymbol (name));
        }

        if (matches (Token::identifier))
        {
            if (matchIf ("cast"))
                return parseSuffixOperators (state, parseCast (state));

            auto infOrNaN = parseNaNandInfinityTokens();

            if (infOrNaN.isValid())
                return program.getAllocator().allocateConstant (infOrNaN);

            if (matchIf ("processor"))
                return parseProcessorProperty();

            if (matchIf ("purecall"))
                return parsePureFunctionCall (state);
        }

        if (matches (Token::literalInt32))       return parseConstantAsExpression (state, PrimitiveType::int32);
        if (matches (Token::literalInt64))       return parseConstantAsExpression (state, PrimitiveType::int64);
        if (matches (Token::literalFloat32))     return parseConstantAsExpression (state, PrimitiveType::float32);
        if (matches (Token::literalFloat64))     return parseConstantAsExpression (state, PrimitiveType::float64);
        if (matches (Token::literalString))      return parseConstantAsExpression (state, Type::createStringLiteral());

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
        if (matches (Token::variableIdentifier))
        {
            if (auto v = findVariable (state, getIdentifierAsVariableName()))
            {
                skip();
                return parseSuffixOperators (state, *v);
            }
        }

        return {};
    }

    heart::AggregateInitialiserList& parseInitialiserList (const FunctionParseState& state, const Type& type)
    {
        auto& list = module->allocate<heart::AggregateInitialiserList> (location, type);
        expect (HEARTOperator::openParen);

        auto getAggregateElementType = [] (const Type& t, uint32_t index)
        {
            if (t.isFixedSizeAggregate())
            {
                SOUL_ASSERT (index < t.getNumAggregateElements());
                return t.isStruct() ? t.getStructRef().getMemberType (index)
                                    : t.getElementType();
            }

            SOUL_ASSERT (index == 0);
            return t;
        };

        if (! matchIf (HEARTOperator::closeParen))
        {
            for (;;)
            {
                auto& arg = parseExpression (state, getAggregateElementType (type, static_cast<uint32_t> (list.items.size())));
                list.items.push_back (arg);

                if (matchIf (HEARTOperator::comma))
                    continue;

                expect (HEARTOperator::closeParen);
                break;
            }
        }

        return list;
    }

    heart::ProcessorProperty& parseProcessorProperty()
    {
        expect (HEARTOperator::dot);
        auto pos = location;
        auto property = heart::ProcessorProperty::getPropertyFromName (readGeneralIdentifier());

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
        return parseSuffixOperators (state, program.getAllocator().allocateConstant (c));
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

        if (requiredType.isPrimitive())
        {
            if (requiredType.isFloat64())
            {
                if (matchIf (Token::literalInt32) || matchIf (Token::literalInt64))
                    return Value ((double) literalIntValue);

                auto infOrNaN = parseNaNandInfinityTokens();

                if (infOrNaN.isValid() && infOrNaN.getType().isFloat64())
                    return infOrNaN;

                auto val = literalDoubleValue;
                expect (Token::literalFloat64);
                return castValue (Value (val), requiredType);
            }

            if (requiredType.isFloat32())
            {
                if (matchIf (Token::literalInt32) || matchIf (Token::literalInt64))
                    return Value ((float) literalIntValue);

                auto infOrNaN = parseNaNandInfinityTokens();

                if (infOrNaN.isValid() && infOrNaN.getType().isFloat32())
                    return infOrNaN;

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

    std::string readQualifiedGeneralIdentifier()
    {
        auto part1 = readGeneralIdentifier();

        if (matchIf (HEARTOperator::doubleColon))
            return TokenisedPathString::join (part1, readQualifiedGeneralIdentifier());

        return part1;
    }

    std::string readQualifiedVariableIdentifier()
    {
        auto part1 = readVariableIdentifier();

        if (matchIf (HEARTOperator::doubleColon))
            return TokenisedPathString::join (part1, readQualifiedGeneralIdentifier());

        return part1;
    }

    Identifier parseGeneralIdentifier()
    {
        return program.getAllocator().get (readGeneralIdentifier());
    }

    Identifier parseVariableIdentifier()
    {
        return program.getAllocator().get (readVariableIdentifier());
    }

    Identifier parseBlockName()
    {
        auto name = currentStringValue;
        expect (Token::blockIdentifier);

        if (name.length() < 2)
            throwError (Errors::invalidBlockName (name));

        return program.getAllocator().get (name);
    }

    std::string readVariableIdentifier()
    {
        auto name = getIdentifierAsVariableName();
        expect (Token::variableIdentifier);

        return name;
    }

    std::string getIdentifierAsVariableName()
    {
        if (matchesAnyIdentifier() && ! matches (Token::variableIdentifier))
            throwError (Errors::invalidVariableName (currentStringValue));

        if (currentStringValue.length() < 2 || currentStringValue[0] != '$')
            throwError (Errors::invalidVariableName (currentStringValue));

        // Strip leading $
        return currentStringValue.substr(1);
    }

    std::string readGeneralIdentifier()
    {
        auto name = currentStringValue;

        if (matchesAnyIdentifier() && ! matches (Token::identifier))
            throwError (Errors::invalidIdentifierName (name));

        expect (Token::identifier);

        return name;
    }

    bool matchesAnyIdentifier() const
    {
        return matches (Token::identifier) || matches (Token::variableIdentifier) || matches (Token::blockIdentifier);
    }

    int64_t parseLiteralInt()
    {
        auto n = literalIntValue;
        expect (Token::literalInt32);
        return n;
    }

    void checkVersionDeclaration()
    {
        auto errorContext = location;
        expect (HEARTOperator::hash);
        expect (getHEARTFormatVersionPrefix());

        if (! matches (Token::literalInt32))
            errorContext.throwError (Errors::expectedVersionNumber());

        errorContext = location;
        auto version = parseLiteralInt();

        if (version <= 0)
            errorContext.throwError (Errors::expectedVersionNumber());

        if (version > getHEARTFormatVersion())
            errorContext.throwError (Errors::wrongAPIVersion());
    }

    uint32_t parseVersionElement()
    {
        auto v = (uint32_t) parseLiteralInt();

        if (! Version::isValidElementValue (v))
            throwError (Errors::expectedVersionNumber());

        return v;
    }

    StructurePtr findStruct (const std::string& name)
    {
        if (auto s = module->structs.find (name))
            return s;

        for (auto& m : program.getModules())
            for (auto& s : m->structs.get())
                if (program.getFullyQualifiedStructName (*s) == name)
                    return s;

        return {};
    }

    Type readPrimitiveValueType()
    {
        if (matchIf ("float32"))    return parseVectorOrArrayTypeSuffixes (PrimitiveType::float32);
        if (matchIf ("float64"))    return parseVectorOrArrayTypeSuffixes (PrimitiveType::float64);
        if (matchIf ("fixed"))      return parseVectorOrArrayTypeSuffixes (PrimitiveType::fixed);
        if (matchIf ("complex32"))  return parseVectorOrArrayTypeSuffixes (PrimitiveType::complex32);
        if (matchIf ("complex64"))  return parseVectorOrArrayTypeSuffixes (PrimitiveType::complex64);
        if (matchIf ("void"))       return parseVectorOrArrayTypeSuffixes (PrimitiveType::void_);
        if (matchIf ("int32"))      return parseVectorOrArrayTypeSuffixes (PrimitiveType::int32);
        if (matchIf ("int64"))      return parseVectorOrArrayTypeSuffixes (PrimitiveType::int64);
        if (matchIf ("bool"))       return parseVectorOrArrayTypeSuffixes (PrimitiveType::bool_);
        if (matchIf ("string"))     return parseArrayTypeSuffixes (Type::createStringLiteral());
        if (matchIf ("wrap"))       return parseBoundedIntType (true);
        if (matchIf ("clamp"))      return parseBoundedIntType (false);

        return {};
    }

    Type readValueType()
    {
        auto errorPos = location;
        auto t = readPrimitiveValueType();

        if (t.isComplex())  errorPos.throwError (Errors::notYetImplemented ("complex"));
        if (t.isFixed())    errorPos.throwError (Errors::notYetImplemented ("fixed"));

        if (t.isValid())
            return t;

        auto name = readQualifiedGeneralIdentifier();

        if (auto s = findStruct (name))
            return parseArrayTypeSuffixes (Type::createStruct (*s));

        errorPos.throwError (Errors::unresolvedType (name));
    }

    Type readValueOrRefType()
    {
        auto errorPos = location;
        bool isConst = matchIf ("const");

        auto t = readValueType();

        if (matchIf (HEARTOperator::bitwiseAnd))
        {
            if (isConst)
                t = t.createConst();

            return t.createReference();
        }

        if (isConst)
            errorPos.throwError (Errors::notYetImplemented ("const"));
        
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
            if (! elementType.canBeArrayElementType())
                throwError (Errors::wrongTypeForArrayElement());

            if (matchIf (HEARTOperator::closeBracket))
                return parseArrayTypeSuffixes (elementType.createUnsizedArray());

            auto size = parseLiteralInt();

            if (! Type::canBeSafelyCastToArraySize (size))
                throwError (Errors::illegalSize());

            expect (HEARTOperator::closeBracket);
            return parseArrayTypeSuffixes (elementType.createArray (static_cast<Type::ArraySize> (size)));
        }

        return elementType;
    }

    Type parseBoundedIntType (bool isWrap)
    {
        expect (HEARTOperator::lessThan);
        auto size = parseLiteralInt();
        expect (HEARTOperator::greaterThan);

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
