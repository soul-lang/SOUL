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
namespace Keyword
{
    #define SOUL_KEYWORDS(X) \
        X(if_,        "if")         X(do_,        "do")         X(for_,       "for")        X(let,        "let") \
        X(var,        "var")        X(int_,       "int")        X(try_,       "try")        X(else_,      "else") \
        X(bool_,      "bool")       X(true_,      "true")       X(case_,      "case")       X(enum_,      "enum") \
        X(loop,       "loop")       X(void_,      "void")       X(while_,     "while")      X(break_,     "break") \
        X(const_,     "const")      X(int32,      "int32")      X(int64,      "int64")      X(float_,     "float") \
        X(false_,     "false")      X(using_,     "using")      X(fixed,      "fixed")      X(graph,      "graph") \
        X(input,      "input")      X(event,      "event")      X(class_,     "class")      X(catch_,     "catch") \
        X(throw_,     "throw")      X(output,     "output")     X(return_,    "return")     X(string,     "string") \
        X(struct_,    "struct")     X(import,     "import")     X(switch_,    "switch")     X(public_,    "public") \
        X(double_,    "double")     X(private_,   "private")    X(float32,    "float32")    X(float64,    "float64") \
        X(default_,   "default")    X(complex,    "complex")    X(continue_,  "continue")   X(external,   "external") \
        X(operator_,  "operator")   X(processor,  "processor")  X(namespace_, "namespace")  X(complex32,  "complex32") \
        X(complex64,  "complex64")  X(connection, "connection")

    SOUL_KEYWORDS (SOUL_DECLARE_TOKEN)

    struct Matcher
    {
        static TokenType match (int len, choc::text::UTF8Pointer p) noexcept
        {
            #define SOUL_COMPARE_KEYWORD(name, str) if (len == (int) sizeof (str) - 1 && p.startsWith (str)) return name;
            SOUL_KEYWORDS (SOUL_COMPARE_KEYWORD)
            #undef SOUL_COMPARE_KEYWORD
            return {};
        }
    };
}

//==============================================================================
struct StandardOperatorMatcher
{
    static TokenType match (choc::text::UTF8Pointer& text) noexcept
    {
        auto p = text;
        #define SOUL_COMPARE_OPERATOR(name, str) if (p.startsWith (str)) { text = p + (sizeof (str) - 1); return Operator::name; }
        SOUL_OPERATORS (SOUL_COMPARE_OPERATOR)
        #undef SOUL_COMPARE_OPERATOR
        return {};
    }
};

//==============================================================================
struct StandardIdentifierMatcher
{
    static constexpr bool isIdentifierStart (UnicodeChar c) noexcept              { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    static constexpr bool isIdentifierBody  (UnicodeChar c) noexcept              { return isIdentifierStart (c) || (c >= '0' && c <= '9') || c == '_'; }
    static constexpr TokenType categoriseIdentifier (const std::string&) noexcept { return Token::identifier; }
};

//==============================================================================
using SOULTokeniser = Tokeniser<Keyword::Matcher,
                                StandardOperatorMatcher,
                                StandardIdentifierMatcher>;

//==============================================================================
/** Creates a rough-and-ready AST from the tokenised source code, ready for
    refinement in later stages of the compilation process
*/
struct StructuralParser   : public SOULTokeniser
{
    static std::vector<pool_ref<AST::ModuleBase>> parseTopLevelDeclarations (AST::Allocator& allocator,
                                                                             CodeLocation code,
                                                                             AST::Namespace& parentNamespace)
    {
        StructuralParser p (allocator, code, parentNamespace);
        auto oldNumModules = static_cast<int> (parentNamespace.subModules.size());
        p.parseTopLevelDecls (parentNamespace);

        if (oldNumModules == 0)
            return parentNamespace.subModules;

        auto newModules = parentNamespace.subModules;
        newModules.erase (newModules.begin(), newModules.begin() + oldNumModules);
        return newModules;
    }

    static AST::Function& cloneFunction (AST::Allocator& allocator,
                                         const AST::Function& functionToClone)
    {
        auto parentModule = functionToClone.getParentScope()->getAsModule();
        SOUL_ASSERT (parentModule != nullptr);

        parentModule->isFullyResolved = false;
        StructuralParser p (allocator, functionToClone.context.location, *parentModule);
        auto functionList = parentModule->getFunctionList();
        SOUL_ASSERT (functionList != nullptr);
        auto oldSize = functionList->size();
        p.module = *parentModule;
        p.parseFunctionOrStateVariable();
        SOUL_ASSERT (functionList->size() == oldSize + 1);
        ignoreUnused (oldSize);
        return functionList->back();
    }

    [[noreturn]] void throwError (const CompileMessage& message) const override
    {
        getContext().throwError (message);
    }

private:
    AST::Allocator& allocator;
    pool_ptr<AST::ModuleBase> module;
    AST::Scope* currentScope;

    enum class ParseTypeContext
    {
        variableType,
        nameOrType,
        functionParameter,
        eventType,
        structMember,
        usingDeclTarget,
        processorParameter,
        metaFunctionArgument
    };

    pool_ptr<AST::NoopStatement> noop;
    AST::Statement& getNoop()     { if (noop == nullptr) noop = allocate<AST::NoopStatement> (AST::Context()); return *noop; }

    //==============================================================================
    StructuralParser (AST::Allocator& a, const CodeLocation& code, AST::ModuleBase& parentScope)
        : allocator (a), currentScope (std::addressof (parentScope))
    {
        initialise (code);
    }

    ~StructuralParser() override = default;

    template <typename Type, typename... Args>
    Type& allocate (Args&&... args) const    { return allocator.allocate<Type> (std::forward<Args> (args)...); }

    AST::Expression& matchCloseParen (AST::Expression& e)                  { expect (Operator::closeParen); return e; }
    template <typename ExpType> ExpType& matchEndOfStatement (ExpType& e)  { expect (Operator::semicolon);  return e; }

    AST::Context getContext() const             { return { location, currentScope }; }
    AST::Block& getCurrentBlock() const         { auto b = currentScope->getAsBlock(); SOUL_ASSERT (b != nullptr); return *b; }

    struct ScopedScope
    {
        ScopedScope (StructuralParser& p, AST::Scope& newScope) : parser (p), oldScope (p.currentScope)  { parser.currentScope = std::addressof (newScope); }
        ~ScopedScope()   { parser.currentScope = oldScope; }

        StructuralParser& parser;
        AST::Scope* const oldScope;
    };

    //==============================================================================
    void parseTopLevelDecls (AST::Namespace& parentNamespace)
    {
        while (! matchIf (Token::eof))
            parseTopLevelDecl (parentNamespace);
    }

    void parseTopLevelDecl (AST::Namespace& parentNamespace)
    {
        parseImports (parentNamespace);
        auto keywordLocation = location;

        if (matchIf (Keyword::processor))      { parseProcessorDecl (keywordLocation, parentNamespace); return; }
        if (matchIf (Keyword::graph))          { parseGraphDecl     (keywordLocation, parentNamespace); return; }
        if (matchIf (Keyword::namespace_))     { parseNamespaceDecl (keywordLocation, parentNamespace); return; }
        if (matches (Keyword::import))         throwError (Errors::importsMustBeAtStart());

        throwError (Errors::expectedTopLevelDecl());
    }

    pool_ptr<AST::Processor> parseProcessorDecl (CodeLocation keywordLocation, AST::Namespace&  ns)   { return parseTopLevelItem<AST::Processor> (keywordLocation, ns); }
    pool_ptr<AST::Graph>     parseGraphDecl     (CodeLocation keywordLocation, AST::Namespace&  ns)   { return parseTopLevelItem<AST::Graph>     (keywordLocation, ns); }
    pool_ptr<AST::Namespace> parseNamespaceDecl (CodeLocation keywordLocation, AST::ModuleBase& ns)   { return parseTopLevelItem<AST::Namespace> (keywordLocation, ns); }

    template <typename ModuleType>
    pool_ptr<ModuleType> parseTopLevelItem (CodeLocation processorKeywordLocation, AST::ModuleBase& parentModule)
    {
        auto context = getContext();
        auto name = parseIdentifierWithMaxLength (AST::maxIdentifierLength);

        if (matchIf (Operator::assign))
        {
            auto& identifier = parseQualifiedIdentifier();
            auto specialisationArgs = parseSpecialisationArgs();
            expect (Operator::semicolon);
            auto& alias = allocate<AST::NamespaceAliasDeclaration> (context, name, identifier, specialisationArgs);
            parentModule.namespaceAliases.push_back (alias);
            return {};
        }

        auto parentNamespace = cast<AST::Namespace> (parentModule);

        if (parentNamespace == nullptr)
            context.throwError (Errors::namespaceMustBeInsideNamespace());

        auto& newModule = allocate<ModuleType> (processorKeywordLocation, context, name);
        parentNamespace->subModules.push_back (newModule);

        auto newNamespace = cast<AST::Namespace> (newModule);
        ScopedScope scope (*this, newModule);
        auto oldModule = module;
        module = newModule;

        if (newNamespace != nullptr && matchIf (Operator::doubleColon))
        {
            newNamespace->processorKeywordLocation = {};
            parseTopLevelItem<ModuleType> (processorKeywordLocation, *newNamespace);
        }
        else
        {
            parseTopLevelDeclContent();
        }

        module = oldModule;

        newModule.createClone = [&newModule] (AST::Allocator& a, AST::Namespace& parentNS, const std::string& newName) -> AST::ModuleBase&
        {
            return cloneModuleWithNewName (a, parentNS, newModule, newName);
        };

        return newModule;
    }

    static AST::ModuleBase& cloneModuleWithNewName (AST::Allocator& allocator,
                                                    AST::Namespace& parentNamespace,
                                                    AST::ModuleBase& itemToClone,
                                                    const std::string& newName)
    {
        StructuralParser p (allocator, itemToClone.context.location, parentNamespace);

        pool_ptr<AST::ModuleBase> clonedModule;

        if (itemToClone.isProcessor())  clonedModule = p.parseProcessorDecl (itemToClone.processorKeywordLocation, parentNamespace);
        if (itemToClone.isGraph())      clonedModule = p.parseGraphDecl     (itemToClone.processorKeywordLocation, parentNamespace);
        if (itemToClone.isNamespace())  clonedModule = p.parseNamespaceDecl (itemToClone.processorKeywordLocation, parentNamespace);

        SOUL_ASSERT (clonedModule != nullptr);

        clonedModule->name = allocator.identifiers.get (newName);
        clonedModule->originalModule = itemToClone;

        return *clonedModule;
    }

    pool_ptr<AST::Expression> parseSpecialisationArgs()
    {
        if (! matchIf (Operator::openParen))
            return {};

        if (matchIf (Operator::closeParen))
            return {};

        return parseParenthesisedExpression();
    }

    void parseImports (AST::Namespace& parentNamespace)
    {
        while (matchIf (Keyword::import))
        {
            if (matches (Token::literalString))
            {
                parentNamespace.importsList.addIfNotAlreadyThere (currentStringValue);
                expect (Token::literalString);
            }
            else if (matches (Token::identifier))
            {
                auto name = readIdentifier();

                while (matchIf (Operator::dot))
                    name += "." + readIdentifier();

                parentNamespace.importsList.addIfNotAlreadyThere (name);
            }
            else
            {
                throwError (Errors::expectedModule());
            }

            expect (Operator::semicolon);
            throwError (Errors::notYetImplemented ("import statements"));
        }
    }

    void parseTopLevelDeclContent()
    {
        parseSpecialisationParameters();

        auto processor = cast<AST::ProcessorBase> (module);
        auto graph     = cast<AST::Graph> (module);
        auto ns        = cast<AST::Namespace> (module);

        if (processor != nullptr)
            parseAnnotation (processor->annotation);

        expect (Operator::openBrace);

        if (processor != nullptr)
        {
            while (parseEndpoint (*processor))
            {}
        }

        if (ns != nullptr)
            parseImports (*ns);

        while (! matchIf (Operator::closeBrace))
        {
            if (graph != nullptr)
            {
                if (parseProcessorInstanceList (*graph)) continue;
                if (parseConnectionList (*graph))        continue;
            }
            else
            {
                if (matchIf (Keyword::struct_))         { parseStructDeclaration();  continue; }

                if (matches (Keyword::graph))
                {
                    if (ns == nullptr)
                        throwError (Errors::graphMustBeInsideNamespace());

                    auto keywordLocation = location;
                    skip();

                    parseGraphDecl (keywordLocation, *ns);
                    continue;
                }
            }

            if (matches (Keyword::processor))
            {
                auto keywordLocation = location;
                skip();

                if (matches (Operator::dot))
                {
                    parseProcessorLatencyDeclaration();
                    continue;
                }

                if (ns == nullptr)
                    throwError (Errors::processorMustBeInsideNamespace());

                parseProcessorDecl (keywordLocation, *ns);
                continue;
            }

            if (matchIf (Keyword::using_))          { parseUsingDeclaration();             continue; }
            if (matchIf (Keyword::let))             { parseTopLevelLetOrVar (true);        continue; }
            if (matchIf (Keyword::var))             { parseTopLevelLetOrVar (false);       continue; }
            if (matchIf (Keyword::event))           { parseEventFunction();                continue; }

            if (matches (Keyword::namespace_))
            {
                auto keywordLocation = location;
                skip();
                parseNamespaceDecl (keywordLocation, *module);
                continue;
            }

            if (matchesAny (Keyword::input, Keyword::output))
                throwError (ns != nullptr ? Errors::namespaceCannotContainEndpoints()
                                          : Errors::endpointDeclsMustBeFirst());

            if (matches (Keyword::import))
                throwError (Errors::importsMustBeAtStart());

            parseFunctionOrStateVariable();
        }

        giveErrorOnSemicolon();
    }

    void parseFunctionOrStateVariable()
    {
        auto declarationContext = getContext();
        bool isExternal = matchIf (Keyword::external);

        if (matches ("static_assert"))
            return parseStaticAssert();

        auto type = tryParsingType (ParseTypeContext::variableType);

        if (type == nullptr)
            declarationContext.throwError (Errors::expectedFunctionOrVariable());

        auto context = getContext();
        auto name = parseIdentifier();

        std::vector<pool_ref<AST::UnqualifiedName>> genericWildcards;

        if (matchIf (Operator::lessThan))
            genericWildcards = parseGenericFunctionWildcardList();

        if (matchIf (Operator::openParen))
        {
            if (isExternal)
                declarationContext.throwError (Errors::functionCannotBeExternal());

            if (auto functions = module->getFunctionList())
                functions->push_back (parseFunctionDeclaration (declarationContext, *type, name, context, genericWildcards));
            else
                declarationContext.throwError (module->isGraph() ? Errors::graphCannotContainFunctions()
                                                                 : Errors::noFunctionInThisScope());
        }
        else
        {
            if (isExternal && type->getConstness() == AST::Constness::definitelyConst)
                declarationContext.throwError (Errors::noConstOnExternals());

            auto& stateVariables = module->getStateVariableList();

            parseVariableDeclaration (*type, name, isExternal, context,
                                      [&] (AST::VariableDeclaration& v)  { stateVariables.push_back (v); });
        }
    }

    void parseUsingDeclaration()
    {
        auto context = getContext();
        auto name = parseIdentifier();

        expect (Operator::assign);

        auto& type = parseType (ParseTypeContext::usingDeclTarget);
        module->usings.push_back (allocate<AST::UsingDeclaration> (context, name, type));

        expect (Operator::semicolon);
    }

    void parseStructDeclaration()
    {
        auto context = getContext();
        auto name = parseIdentifier();
        expect (Operator::openBrace);

        auto& newStruct = allocate<AST::StructDeclaration> (context, name);
        module->structures.push_back (newStruct);

        while (! matchIf (Operator::closeBrace))
        {
            auto& type = parseType (ParseTypeContext::structMember);

            for (;;)
            {
                newStruct.addMember (type, getContext(), parseIdentifier());

                if (matchIf (Operator::comma))
                    continue;

                expect (Operator::semicolon);
                break;
            }
        }

        giveErrorOnSemicolon();
    }

    void parseStaticAssert()
    {
        auto context = getContext();
        skip();
        expect (Operator::openParen);
        auto& args = parseCommaSeparatedListOfExpressions (false, false);
        expect (Operator::semicolon);
        module->staticAssertions.push_back (ASTUtilities::createStaticAssertion (context, allocator, args.items));
    }

    //==============================================================================
    void parseSpecialisationParameters()
    {
        if (matchIf (Operator::openParen))
        {
            if (matchIf (Operator::closeParen))
                return;

            do
            {
                if (matchIf (Keyword::using_))
                {
                    if (module->isGraph())
                        throwError (Errors::graphCannotHaveSpecialisations());

                    auto context = getContext();
                    auto name = parseIdentifier();

                    auto& usingDeclaration = allocate<AST::UsingDeclaration> (context, name, nullptr);

                    if (matchIf (Operator::assign))
                        usingDeclaration.targetType = parseType (ParseTypeContext::variableType);

                    module->addSpecialisationParameter (usingDeclaration);
                }
                else if (matchIf (Keyword::processor))
                {
                    if (! module->isGraph())
                        throwError (Errors::processorSpecialisationNotAllowed());

                    auto context = getContext();
                    auto& processorAliasDeclaration = allocate<AST::ProcessorAliasDeclaration> (context, parseIdentifier());

                    if (matchIf (Operator::assign))
                        processorAliasDeclaration.targetProcessor = parseType (ParseTypeContext::nameOrType);

                    module->addSpecialisationParameter (processorAliasDeclaration);
                }
                else if (matchIf (Keyword::namespace_))
                {
                    if (! module->isNamespace())
                        throwError (Errors::namespaceSpecialisationNotAllowed());

                    auto context = getContext();
                    auto& namespaceAliasDeclaration = allocate<AST::NamespaceAliasDeclaration> (context, parseIdentifier());

                    if (matchIf (Operator::assign))
                        namespaceAliasDeclaration.targetNamespace = parseType (ParseTypeContext::nameOrType);

                    module->addSpecialisationParameter (namespaceAliasDeclaration);
                }
                else
                {
                    giveErrorOnExternalKeyword();
                    auto& parameterType = parseType (ParseTypeContext::processorParameter);
                    auto& parameterVariable = allocate<AST::VariableDeclaration> (getContext(), parameterType, nullptr, true);
                    parameterVariable.isSpecialisation = true;
                    parameterVariable.name = parseIdentifier();

                    if (matchIf (Operator::assign))
                        parameterVariable.initialValue = parseExpression();

                    module->addSpecialisationParameter (parameterVariable);
                }
            }
            while (matchIf (Operator::comma));

            expect (Operator::closeParen);
        }
    }

    template <typename TokenName, typename ParserFn>
    bool parseOptionallyBracedList (TokenName type, bool allowCommaSeparator, ParserFn&& parserFn)
    {
        if (! matchIf (type))
            return false;

        if (matchIf (Operator::openBrace))
        {
            if (matchIf (Operator::closeBrace))
                return true;

            for (;;)
            {
                parserFn();

                if (allowCommaSeparator && matchIf (Operator::comma))
                    continue;

                expect (Operator::semicolon);

                if (matchIf (Operator::closeBrace))
                    break;
            }
        }
        else
        {
            for (;;)
            {
                parserFn();

                if (allowCommaSeparator && matchIf (Operator::comma))
                    continue;

                expect (Operator::semicolon);
                break;
            }
        }

        return true;
    }

    bool parseConnectionList (AST::Graph& g)        { return parseOptionallyBracedList (Keyword::connection, false, [this, &g] { parseConnection (g); }); }
    bool parseProcessorInstanceList (AST::Graph& g) { return parseOptionallyBracedList (Keyword::let,        true,  [this, &g] { parseProcessorInstance (g); }); }

    //==============================================================================
    void parseConnection (AST::Graph& graph)
    {
        auto interpolationType = parseOptionalInterpolationType();
        auto context = getContext();
        ArrayWithPreallocation<pool_ref<AST::Connection::SharedEndpoint>, 8> sources, dests;
        pool_ptr<AST::Expression> delayLength;

        for (;;)
        {
            sources.push_back (allocator.allocate<AST::Connection::SharedEndpoint> (parseExpression()));

            if (! matchIf (Operator::comma))
                break;
        }

        for (;;)
        {
            context = getContext();
            expect (Operator::rightArrow);
            delayLength = parseDelayLength();

            for (;;)
            {
                dests.push_back (allocator.allocate<AST::Connection::SharedEndpoint> (parseConnectionPoint()));

                if (! matchIf (Operator::comma))
                    break;
            }

            if (sources.size() > 1 && dests.size() > 1)
                context.throwError (Errors::notYetImplemented ("Many-to-many connections are not currently supported"));

            for (auto& source : sources)
                for (auto& dest : dests)
                    graph.connections.push_back (allocate<AST::Connection> (context, interpolationType, source, dest, delayLength));

            if (matches (Operator::rightArrow))
            {
                if (dests.size() != 1)
                    dests.back()->endpoint->context.throwError (Errors::cannotChainConnectionWithMultiple());

                if (auto dot = cast<AST::DotOperator> (dests.back()->endpoint))
                    dot->rhs.context.throwError (Errors::cannotNameEndpointInChain());

                sources = dests;
                dests.clear();
                continue;
            }

            break;
        }
    }

    AST::Expression& parseConnectionPoint()
    {
        auto errorPos = getContext();
        auto startPos = getCurrentTokeniserPosition();

        if (auto e = tryToParseExpressionIgnoringErrors())
            return *e;

        resetPosition (startPos);

        if (! matches (Token::identifier))
            errorPos.throwError (Errors::expectedProcessorOrEndpoint());

        auto& processorName = parseQualifiedIdentifier();
        pool_ptr<AST::CommaSeparatedList> args;

        if (auto specialisationArgs = parseSpecialisationArgs())
        {
            args = cast<AST::CommaSeparatedList> (specialisationArgs);

            if (args == nullptr)
            {
                args = allocate<AST::CommaSeparatedList> (specialisationArgs->context);
                args->items.push_back (*specialisationArgs);
            }
        }

        return allocate<AST::CallOrCast> (processorName, args, false);
    }

    InterpolationType parseOptionalInterpolationType()
    {
        if (! matchIf (Operator::openBracket))
            return InterpolationType::none;

        auto type = parseInterpolationType (*this);
        expect (Operator::closeBracket);
        return type;
    }

    pool_ptr<AST::Expression> parseDelayLength()
    {
        if (matchIf (Operator::openBracket))
        {
            auto& e = parseExpression();
            expect (Operator::closeBracket);
            expect (Operator::rightArrow);
            return e;
        }

        return {};
    }

    void parseProcessorInstance (AST::Graph& graph)
    {
        auto& u = allocate<AST::ProcessorInstance> (getContext());
        u.instanceName = parseUnqualifiedName();
        graph.addProcessorInstance (u);

        expect (Operator::assign);

        if (! matches (Token::identifier))
            throwError (Errors::expectedProcessorName());

        u.targetProcessor = parseQualifiedIdentifier();

        // Array of processors
        if (matchIf (Operator::openBracket))
        {
            u.arraySize = parseExpression();
            expect (Operator::closeBracket);
        }

        // Parameterised
        u.specialisationArgs = parseSpecialisationArgs();

        // Clocked
        if (matchIf (Operator::times))
            u.clockMultiplierRatio = parseExpression();
        else if (matchIf (Operator::divide))
            u.clockDividerRatio = parseExpression();
    }

    AST::Expression& parseSpecialisationValueOrType()
    {
        auto startPos = getCurrentTokeniserPosition();

        if (auto type = tryParsingType (ParseTypeContext::usingDeclTarget))
            if (! matches (Operator::openParen))
                return *type;

        resetPosition (startPos);
        return parseExpression();
    }

    //==============================================================================
    bool parseEndpoint (AST::ProcessorBase& p)
    {
        if (matchIf (Keyword::input))  { parseEndpoint (p, true);  return true; }
        if (matchIf (Keyword::output)) { parseEndpoint (p, false); return true; }

        return false;
    }

    void parseEndpoint (AST::ProcessorBase& p, bool isInput, bool alreadyInsideBracedExpression = false)
    {
        if (! alreadyInsideBracedExpression && matchIf (Operator::openBrace))
        {
            while (! matchIf (Operator::closeBrace))
                parseEndpoint (p, isInput, true);
        }
        else
        {
            if (p.isGraph() && matches (Token::identifier) && ! isNextTokenEndpointType (*this))
                return parseChildEndpoint (p, isInput);

            auto endpointType = parseEndpointType (*this);

            if (matchIf (Operator::openBrace))
            {
                while (! matchIf (Operator::closeBrace))
                    parseEndpoint (p, isInput, endpointType);
            }
            else
            {
                parseEndpoint (p, isInput, endpointType);
            }
        }
    }

    void parseEndpoint (AST::ProcessorBase& p, bool isInput, EndpointType endpointType)
    {
        auto& first = allocate<AST::EndpointDeclaration> (allocator, getContext(), isInput, endpointType);
        first.getDetails().dataTypes = parseEndpointTypeList (endpointType);
        parseInputOrOutputName (first);
        p.endpoints.push_back (first);

        while (matchIf (Operator::comma))
        {
            auto& e = allocate<AST::EndpointDeclaration> (allocator, getContext(), isInput, endpointType);
            e.getDetails().dataTypes = first.getDetails().dataTypes;
            parseInputOrOutputName (e);
            p.endpoints.push_back (e);
        }

        expect (Operator::semicolon);
    }

    void parseInputOrOutputName (AST::EndpointDeclaration& e)
    {
        e.context = getContext();
        e.name = parseIdentifierWithMaxLength (AST::maxIdentifierLength);

        if (matchIf (Operator::openBracket))
        {
            e.getDetails().arraySize = parseExpression();
            expect (Operator::closeBracket);
        }

        parseAnnotation (e.annotation);
    }

    void parseChildEndpoint (AST::ProcessorBase& p, bool isInput)
    {
        for (;;)
        {
            auto& e = allocate<AST::EndpointDeclaration> (getContext(), isInput);
            p.endpoints.push_back (e);
            e.childPath = allocate<AST::ChildEndpointPath>();
            bool canParseName = true;

            for (;;)
            {
                AST::ChildEndpointPath::PathSection path;

                if (matchIf (Operator::times))
                {
                    throwError (Errors::notYetImplemented ("Wildcard child endpoint references"));
                }
                else
                {
                    path.name = parseUnqualifiedName();
                }

                if (matchIf (Operator::openBracket))
                {
                    path.index = parseExpression();
                    expect (Operator::closeBracket);
                }

                e.childPath->sections.push_back (path);

                if (matchIf (Operator::dot))
                    continue;

                break;
            }

            if (canParseName)
            {
                if (matches (Token::identifier))
                    e.name = parseIdentifier();
                else
                    e.name = e.childPath->sections.back().name->identifier;

                parseAnnotation (e.annotation);

                if (matchIf (Operator::comma))
                    continue;
            }

            expect (Operator::semicolon);

            if (e.childPath->sections.size() == 1)
                e.context.throwError (Errors::expectedStreamType());

            break;
        }
    }

    void parseAnnotation (AST::Annotation& annotation)
    {
        annotation.properties.clear();

        if (matchIf (Operator::openDoubleBracket))
        {
            if (matchIf (Operator::closeDoubleBracket))
                return;

            do
            {
                auto context = getContext();
                auto name = parseAnnotationKey();
                checkLength (name, AST::maxIdentifierLength);
                skip();

                if (annotation.findProperty (name) != nullptr)
                    context.throwError (Errors::nameInUse (name));

                auto& key = allocate<AST::UnqualifiedName> (context, allocator.get (name));

                if (matchIf (Operator::colon))
                    annotation.addProperty ({ key, parseExpression() });
                else
                    annotation.addProperty ({ key, allocate<AST::Constant> (getContext(), Value (true)) });
            }
            while (matchIf (Operator::comma));

            expect (Operator::closeDoubleBracket);
        }
    }

    std::string parseAnnotationKey()
    {
        if (matchesAny (Token::identifier, Token::literalString))
            return currentStringValue;

        #define SOUL_CHECK_KEYWORD(name, str) if (matches (Keyword::name)) return str;
        SOUL_KEYWORDS (SOUL_CHECK_KEYWORD)
        #undef SOUL_CHECK_KEYWORD

        expect (Token::identifier);
        return {};
    }

    //==============================================================================
    void parseEventFunction()
    {
        auto functions = module->getFunctionList();

        if (functions == nullptr || ! module->isProcessor())
            throwError (Errors::noEventFunctionsAllowed());

        auto context = getContext();
        auto name = parseIdentifierWithMaxLength (AST::maxIdentifierLength);

        expect (Operator::openParen);
        auto& f = allocate<AST::Function> (context);
        ScopedScope scope (*this, f);

        functions->push_back (f);
        f.returnType = allocate<AST::ConcreteType> (context, PrimitiveType::void_);
        f.name = name;
        f.nameLocation = context;
        f.eventFunction = true;

        // Event functions have either 1 argument (the type of the event) or two arguments
        // (an index followed by the type of the event) if the input is an event array

        {
            auto& type = parseType (ParseTypeContext::functionParameter);
            auto& v = allocate<AST::VariableDeclaration> (getContext(), type, nullptr, false);
            f.parameters.push_back (v);
            v.isFunctionParameter = true;
            v.name = parseIdentifier();
        }

        if (matchIf (Operator::comma))
        {
            auto& type = parseType (ParseTypeContext::functionParameter);
            auto& v = allocate<AST::VariableDeclaration> (getContext(), type, nullptr, false);
            f.parameters.push_back (v);
            v.isFunctionParameter = true;
            v.name = parseIdentifier();
        }

        expect (Operator::closeParen);
        f.block = parseBracedBlock (f);
    }

    //==============================================================================
    std::vector<pool_ref<AST::UnqualifiedName>> parseGenericFunctionWildcardList()
    {
        std::vector<pool_ref<AST::UnqualifiedName>> wildcards;

        for (;;)
        {
            if (! matches (Token::identifier))
                throwError (Errors::expectedGenericWildcardName());

            auto& wildcard = parseUnqualifiedName();

            wildcards.push_back (wildcard);

            if (matchIf (Operator::greaterThan))
                break;

            expect (Operator::comma);
        }

        return wildcards;
    }

    static void recursivelyReplaceParentScope (AST::Expression& target, AST::Scope& newScope)
    {
        struct ScopeReplacingVisitor  : public ASTVisitor
        {
            void visitObject (AST::Expression& e) override
            {
                ASTVisitor::visitObject (e);

                if (e.context.parentScope == oldScope)
                    e.context.parentScope = newScope;
            }

            AST::Scope* oldScope;
            AST::Scope* newScope;
        };

        ScopeReplacingVisitor v;
        v.oldScope = target.context.parentScope;
        v.newScope = std::addressof (newScope);
        v.visitObject (target);
    }

    AST::Function& parseFunctionDeclaration (const AST::Context& context, AST::Expression& returnType,
                                             Identifier name, const AST::Context& nameLocation,
                                             std::vector<pool_ref<AST::UnqualifiedName>> genericWildcards)
    {
        auto& f = allocate<AST::Function> (context);
        ScopedScope scope (*this, f);

        f.name = name;
        f.nameLocation = nameLocation;
        f.returnType = returnType;
        f.genericWildcards = std::move (genericWildcards);

        if (f.returnType != nullptr)
            recursivelyReplaceParentScope (*f.returnType, f);

        for (auto& w : f.genericWildcards)
            recursivelyReplaceParentScope (w, f);

        if (! matchIf (Operator::closeParen))
        {
            for (;;)
            {
                giveErrorOnExternalKeyword();
                auto typeLocation = getContext();
                auto& type = parseType (ParseTypeContext::functionParameter);

                if (auto t = type.getConcreteType())
                    if (t->isVoid())
                        typeLocation.throwError (Errors::parameterCannotBeVoid());

                if (f.parameters.size() > 127)
                    typeLocation.throwError (Errors::tooManyParameters());

                auto& v = allocate<AST::VariableDeclaration> (getContext(), type, nullptr, false);
                f.parameters.push_back (v);
                v.isFunctionParameter = true;
                v.name = parseIdentifier();

                if (matchIf (Operator::closeParen))
                    break;

                expect (Operator::comma);
            }
        }

        parseAnnotation (f.annotation);

        if (auto intrin = f.annotation.findProperty ("intrin"))
        {
            if (auto c = intrin->value->getAsConstant())
            {
                if (c->value.getType().isStringLiteral())
                {
                    f.intrinsic = getIntrinsicTypeFromName (allocator.stringDictionary.getStringForHandle (c->value.getStringLiteral()));
                    SOUL_ASSERT (f.intrinsic != IntrinsicType::none);
                }
            }
        }

        if (! matchIf (Operator::semicolon))
            f.block = parseBracedBlock (f);

        return f;
    }

    AST::Block& parseBracedBlock (pool_ptr<AST::Function> ownerFunction)
    {
        expect (Operator::openBrace);
        auto& newBlock = allocate<AST::Block> (getContext(), ownerFunction);
        ScopedScope scope (*this, newBlock);

        while (! matchIf (Operator::closeBrace))
            newBlock.addStatement (parseStatement());

        return newBlock;
    }

    AST::Block& parseStatementAsNewBlock()
    {
        if (matches (Operator::openBrace))
            return parseBracedBlock ({});

        auto& newBlock = allocate<AST::Block> (getContext(), nullptr);
        ScopedScope scope (*this, newBlock);
        newBlock.addStatement (parseStatement());
        return newBlock;
    }

    AST::Statement& parseStatement()
    {
        if (matches (Operator::openBrace))     return parseBracedBlock ({});
        if (matchIf (Keyword::if_))            return parseIf();
        if (matchIf (Keyword::while_))         return parseWhileLoop();
        if (matchIf (Keyword::for_))           return parseForLoop();
        if (matchIf (Keyword::loop))           return parseLoopStatement();
        if (matchIf (Keyword::return_))        return parseReturn();
        if (matchIf (Keyword::break_))         return matchEndOfStatement (allocate<AST::BreakStatement> (getContext()));
        if (matchIf (Keyword::continue_))      return matchEndOfStatement (allocate<AST::ContinueStatement> (getContext()));
        if (matchIf (Keyword::let))            return parseLocalLetOrVar (true);
        if (matchIf (Keyword::var))            return parseLocalLetOrVar (false);
        if (matches (Keyword::external))       throwError (Errors::externalNotAllowedInFunction());
        if (matchIf (Operator::semicolon))     return getNoop();
        if (matchIf (Operator::plusplus))      return matchEndOfStatement (parsePreIncDec (true));
        if (matchIf (Operator::minusminus))    return matchEndOfStatement (parsePreIncDec (false));
        if (matches (Operator::openParen))     return matchEndOfStatement (parseFactor());

        if (matchesAny (Token::literalInt32, Token::literalInt64, Token::literalFloat64,
                        Token::literalFloat32, Token::literalString, Operator::minus,
                        Token::literalImag32, Token::literalImag64))
            return parseExpressionAsStatement (false);

        {
            auto oldPos = getCurrentTokeniserPosition();

            if (auto type = tryParsingType (ParseTypeContext::variableType))
            {
                if (matches (Token::identifier))
                {
                    if (matches (Operator::openParen))
                        throwError (Errors::expectedVariableDecl());

                    auto context = getContext();
                    ArrayWithPreallocation<pool_ref<AST::VariableDeclaration>, 8> variablesCreated;

                    parseVariableDeclaration (*type, parseIdentifier(), false, context,
                                              [&] (AST::VariableDeclaration& v) { variablesCreated.push_back (v); });

                    if (variablesCreated.size() == 1)
                        return variablesCreated.front();

                    for (auto& v : variablesCreated)
                        getCurrentBlock().addStatement (v);

                    return getNoop();
                }
            }

            resetPosition (oldPos);
        }

        if (! matches (Token::identifier))
        {
            giveErrorOnExternalKeyword();
            giveErrorOnAssignmentToProcessorProperty();
        }

        return parseExpressionAsStatement (true);
    }

    pool_ptr<AST::Expression> tryToParseExpressionIgnoringErrors()
    {
        pool_ptr<AST::Expression> result;
        catchParseErrors ([this, &result] { result = parseExpression(); });
        return result;
    }

    pool_ptr<AST::Expression> tryToParseChvronExpressionIgnoringErrors()
    {
        pool_ptr<AST::Expression> result;

        catchParseErrors ([this, &result] { result = parseShiftOperator(); });

        return result;
    }

    AST::Expression& parseExpression (bool allowAssignment = false)
    {
        auto& lhs = parseTernaryOperator();

        if (matches (Operator::plusEquals))                 return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::add);
        if (matches (Operator::minusEquals))                return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::subtract);
        if (matches (Operator::timesEquals))                return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::multiply);
        if (matches (Operator::divideEquals))               return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::divide);
        if (matches (Operator::moduloEquals))               return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::modulo);
        if (matches (Operator::leftShiftEquals))            return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::leftShift);
        if (matches (Operator::rightShiftEquals))           return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::rightShift);
        if (matches (Operator::rightShiftUnsignedEquals))   return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::rightShiftUnsigned);
        if (matches (Operator::xorEquals))                  return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::bitwiseXor);
        if (matches (Operator::bitwiseAndEquals))           return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::bitwiseAnd);
        if (matches (Operator::bitwiseOrEquals))            return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::bitwiseOr);
        if (matches (Operator::logicalAndEquals))           return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::logicalAnd);
        if (matches (Operator::logicalOrEquals))            return parseInPlaceOpExpression (allowAssignment, lhs, BinaryOp::Op::logicalOr);

        if (matchIf (Operator::assign))
        {
            if (! allowAssignment)
                throwError (Errors::assignmentInsideExpression());

            auto context = getContext();
            return allocate<AST::Assignment> (context, lhs, parseExpression());
        }

        return lhs;
    }

    AST::Expression& parseExpressionAsStatement (bool allowAssignment)
    {
        return matchEndOfStatement (parseExpression (allowAssignment));
    }

    //==============================================================================
    static inline BinaryOp::Op getBinaryOpForToken (TokenType token)
    {
        #define SOUL_COMPARE_BINARY_OP(name, op)  if (token == #op) return BinaryOp::Op::name;
        SOUL_BINARY_OPS (SOUL_COMPARE_BINARY_OP)
        #undef SOUL_COMPARE_BINARY_OP
        SOUL_ASSERT_FALSE;
        return {};
    }

    static inline UnaryOp::Op getUnaryOpForToken (TokenType token)
    {
        #define SOUL_COMPARE_UNARY_OP(name, op)  if (token == #op) return UnaryOp::Op::name;
        SOUL_UNARY_OPS (SOUL_COMPARE_UNARY_OP)
        #undef SOUL_COMPARE_UNARY_OP
        SOUL_ASSERT_FALSE;
        return {};
    }

    AST::Expression& createBinaryOperator (const AST::Context& c, AST::Expression& a, AST::Expression& b, BinaryOp::Op op)
    {
        if (! AST::isPossiblyValue (a))  a.context.throwError (Errors::expectedValueOrEndpoint());
        if (! AST::isPossiblyValue (b))  b.context.throwError (Errors::expectedValueOrEndpoint());

        return allocate<AST::BinaryOperator> (c, a, b, op);
    }

    AST::Expression& parseTernaryOperator()
    {
        auto& a = parseLogicalOr();

        if (! matches (Operator::question))
            return a;

        auto context = getContext();
        skip();
        auto& trueBranch = parseTernaryOperator();
        expect (Operator::colon);
        auto& falseBranch = parseTernaryOperator();
        return allocate<AST::TernaryOp> (context, a, trueBranch, falseBranch);
    }

    AST::Expression& parseLogicalOr()
    {
        for (pool_ref<AST::Expression> a = parseLogicalAnd();;)
        {
            if (! matches (Operator::logicalOr))
                return a;

            auto context = getContext();
            skip();
            auto& trueBranch = allocate<AST::Constant> (context, Value (true));
            auto& falseBranch = parseLogicalAnd();
            a = allocate<AST::TernaryOp> (context, a, trueBranch, falseBranch);
        }
    }

    AST::Expression& parseLogicalAnd()
    {
        for (pool_ref<AST::Expression> a = parseBitwiseOr();;)
        {
            if (! matches (Operator::logicalAnd))
                return a;

            auto context = getContext();
            skip();
            auto& trueBranch = parseBitwiseOr();
            auto& falseBranch = allocate<AST::Constant> (context, Value (false));
            a = allocate<AST::TernaryOp> (context, a, trueBranch, falseBranch);
        }
    }

    AST::Expression& parseBitwiseOr()
    {
        for (pool_ref<AST::Expression> a = parseBitwiseXor();;)
        {
            if (! matches (Operator::bitwiseOr))
                return a;

            auto context = getContext();
            skip();
            a = createBinaryOperator (context, a, parseBitwiseXor(), BinaryOp::Op::bitwiseOr);
        }
    }

    AST::Expression& parseBitwiseXor()
    {
        for (pool_ref<AST::Expression> a = parseBitwiseAnd();;)
        {
            if (! matches (Operator::bitwiseXor))
                return a;

            auto context = getContext();
            skip();
            a = createBinaryOperator (context, a, parseBitwiseAnd(), BinaryOp::Op::bitwiseXor);
        }
    }

    AST::Expression& parseBitwiseAnd()
    {
        for (pool_ref<AST::Expression> a = parseEqualityOperator();;)
        {
            if (! matches (Operator::bitwiseAnd))
                return a;

            auto context = getContext();
            skip();
            a = createBinaryOperator (context, a, parseEqualityOperator(), BinaryOp::Op::bitwiseAnd);
        }
    }

    AST::Expression& parseEqualityOperator()
    {
        for (pool_ref<AST::Expression> a = parseComparisonOperator();;)
        {
            if (! matchesAny (Operator::equals, Operator::notEquals))
                return a;

            auto context = getContext();
            auto type = getBinaryOpForToken (skip());
            a = createBinaryOperator (context, a, parseComparisonOperator(), type);
        }
    }

    AST::Expression& parseComparisonOperator()
    {
        for (pool_ref<AST::Expression> a = parseShiftOperator();;)
        {
            if (! matchesAny (Operator::lessThan,
                              Operator::lessThanOrEqual,
                              Operator::greaterThanOrEqual,
                              Operator::greaterThan))
                return a;

            auto context = getContext();
            auto type = getBinaryOpForToken (skip());
            a = createBinaryOperator (context, a, parseShiftOperator(), type);
        }
    }

    AST::Expression& parseShiftOperator()
    {
        for (pool_ref<AST::Expression> a = parseAdditionSubtraction();;)
        {
            if (! matchesAny (Operator::leftShift, Operator::rightShift, Operator::rightShiftUnsigned))
                return a;

            auto context = getContext();
            auto type = getBinaryOpForToken (skip());
            a = createBinaryOperator (context, a, parseAdditionSubtraction(), type);
        }
    }

    AST::Expression& parseAdditionSubtraction()
    {
        for (pool_ref<AST::Expression> a = parseMultiplyDivide();;)
        {
            if (! matchesAny (Operator::plus, Operator::minus))
            {
                // Handle the annoying case where some sloppy coder has written a
                // minus sign without a space after it, e.g. (x -1)
                if ((matchesAny (Token::literalInt32, Token::literalInt64) && literalIntValue < 0)
                     || (matchesAny (Token::literalFloat64, Token::literalFloat32, Token::literalImag32, Token::literalImag64) && literalDoubleValue < 0))
                {
                    auto context = getContext();
                    a = createBinaryOperator (context, a, parseMultiplyDivide(), BinaryOp::Op::add);
                    continue;
                }

                return a;
            }

            auto context = getContext();
            auto type = getBinaryOpForToken (skip());
            a = createBinaryOperator (context, a, parseMultiplyDivide(), type);
        }
    }

    AST::Expression& parseMultiplyDivide()
    {
        for (pool_ref<AST::Expression> a = parseUnary();;)
        {
            if (! matchesAny (Operator::times, Operator::divide, Operator::modulo))
                return a;

            auto context = getContext();
            auto type = getBinaryOpForToken (skip());
            a = createBinaryOperator (context, a, parseUnary(), type);
        }
    }

    AST::Expression& parseUnary()
    {
        if (matchIf (Operator::plusplus))    return parsePreIncDec (true);
        if (matchIf (Operator::minusminus))  return parsePreIncDec (false);

        if (matchesAny (Operator::minus, Operator::logicalNot, Operator::bitwiseNot))
        {
            auto context = getContext();
            auto type = getUnaryOpForToken (skip());
            return allocate<AST::UnaryOperator> (context, parseUnary(), type);
        }

        return parseFactor();
    }

    AST::Expression& parseFactor()
    {
        if (matchIf (Operator::openParen))     return parseParenthesisedExpression();
        if (matches (Token::literalInt32))     return createLiteral (Value::createInt32 (literalIntValue));
        if (matches (Token::literalInt64))     return createLiteral (Value::createInt64 (literalIntValue));
        if (matches (Token::literalFloat64))   return createLiteral (Value (literalDoubleValue));
        if (matches (Token::literalFloat32))   return createLiteral (Value (static_cast<float> (literalDoubleValue)));
        if (matches (Token::literalImag32))    return createLiteral (Value (std::complex (0.0f, static_cast<float> (literalDoubleValue))));
        if (matches (Token::literalImag64))    return createLiteral (Value (std::complex (0.0, static_cast<double> (literalDoubleValue))));
        if (matches (Token::literalString))    return createLiteral (Value::createStringLiteral (allocator.stringDictionary.getHandleForString (currentStringValue)));
        if (matches (Keyword::true_))          return createLiteral (Value (true));
        if (matches (Keyword::false_))         return createLiteral (Value (false));
        if (matchIf (Keyword::processor))      return parseProcessorProperty();

        if (auto type = tryParsingType (ParseTypeContext::nameOrType))
            return parseSuffixes (*type);

        return parseSuffixes (parseQualifiedIdentifier());
    }

    AST::Expression& parseParenthesisedExpression()
    {
        auto& e = parseExpression();

        if (matchIf (Operator::closeParen))
            return parseSuffixes (e);

        if (matchIf (Operator::comma))
        {
            auto& list = allocate<AST::CommaSeparatedList> (e.context);
            list.items.push_back (e);

            for (;;)
            {
                list.items.push_back (parseExpression());

                if (list.items.size() > AST::maxInitialiserListLength)
                    e.context.throwError (Errors::tooManyInitialisers());

                if (matchIf (Operator::comma))
                    continue;

                expect (Operator::closeParen);
                break;
            }

            return list;
        }

        expect (Operator::closeParen);
        return e;
    }

    AST::Expression& createLiteral (Value v)
    {
        auto& lit = allocate<AST::Constant> (getContext(), v);
        skip();
        return parseSuffixes (lit);
    }

    AST::ProcessorProperty& parseProcessorProperty()
    {
        expect (Operator::dot);
        auto context = getContext();
        auto& propertyName = parseUnqualifiedName();

        auto property = heart::ProcessorProperty::getPropertyFromName (propertyName.toString());

        if (property == heart::ProcessorProperty::Property::none)
            propertyName.context.throwError (Errors::unknownProperty());

        if (! (module->isProcessor() || module->isGraph()))
            context.throwError (Errors::propertiesOutsideProcessor());

        return allocate<AST::ProcessorProperty> (context, property);
    }

    AST::CommaSeparatedList& parseCommaSeparatedListOfExpressions (bool canContainTypes, bool canContainProcessors)
    {
        auto& list = allocate<AST::CommaSeparatedList> (getContext());

        if (! matchIf (Operator::closeParen))
        {
            for (;;)
            {
                auto& e = parseExpression();

                if (list.items.size() >= AST::maxInitialiserListLength)
                    e.context.throwError (Errors::tooManyInitialisers());

                if (! canContainTypes && e.kind == AST::ExpressionKind::type)
                    e.context.throwError (Errors::typeReferenceNotAllowed());

                if (! canContainProcessors && e.kind == AST::ExpressionKind::processor)
                    e.context.throwError (Errors::processorReferenceNotAllowed());

                list.items.push_back (e);

                if (matchIf (Operator::closeParen))
                    break;

                expect (Operator::comma);
            }
        }

        return list;
    }

    AST::Expression& parseDotOperator (AST::Expression& expression)
    {
        auto context = getContext();
        expect (Operator::dot);
        auto& propertyOrMethodName = parseUnqualifiedName();

        auto metaTypeOp = getOpForTypeMetaFunctionName (propertyOrMethodName);

        if (metaTypeOp != AST::TypeMetaFunction::Op::none)
            return parseVectorOrArrayTypeSuffixes (allocate<AST::TypeMetaFunction> (propertyOrMethodName.context, expression, metaTypeOp),
                                                   ParseTypeContext::metaFunctionArgument);

        return parseSuffixes (allocate<AST::DotOperator> (context, expression, propertyOrMethodName));
    }

    AST::Expression& parseSuffixes (AST::Expression& expression)
    {
        auto context = getContext();

        if (matches (Operator::dot))
            return parseDotOperator (expression);

        if (matchIf (Operator::openParen))
        {
            auto& args = parseCommaSeparatedListOfExpressions (false, false);

            if (auto dot = cast<AST::DotOperator> (expression))
            {
                args.items.insert (args.items.begin(), dot->lhs);
                auto& fn = allocate<AST::QualifiedIdentifier> (dot->rhs.context, IdentifierPath (dot->rhs.identifier));
                return parseSuffixes (allocate<AST::CallOrCast> (fn, args, true));
            }

            return parseSuffixes (allocate<AST::CallOrCast> (expression, args, false));
        }

        if (matchIf (Operator::openBracket))  return parseSubscriptWithBrackets (expression);
        if (matchIf (Operator::plusplus))     return parsePostIncDec (expression, true);
        if (matchIf (Operator::minusminus))   return parsePostIncDec (expression, false);

        return expression;
    }

    AST::Expression& parseInPlaceOpExpression (bool allowAssignment, AST::Expression& lhs, BinaryOp::Op opType)
    {
        auto context = getContext();

        if (! allowAssignment)
            context.throwError (Errors::inPlaceOperatorMustBeStatement (currentType));

        skip();
        auto& rhs = parseExpression();
        return allocate<AST::InPlaceOperator> (getContext(), lhs, rhs, opType);
    }

    AST::Expression& parsePreIncDec (bool isIncrement)
    {
        auto context = getContext();
        auto& lhs = parseFactor();
        return allocate<AST::PreOrPostIncOrDec> (context, lhs, isIncrement, false);
    }

    AST::Expression& parsePostIncDec (AST::Expression& lhs, bool isIncrement)
    {
        return allocate<AST::PreOrPostIncOrDec> (getContext(), lhs, isIncrement, true);
    }

    AST::Statement& parseIf()
    {
        auto context = getContext();
        bool isConst = matchIf (Keyword::const_);
        expect (Operator::openParen);
        auto& condition = matchCloseParen (parseExpression());
        auto& trueBranch = parseStatementAsNewBlock();
        pool_ptr<AST::Statement> falseBranch;

        if (matchIf (Keyword::else_))
            falseBranch = parseStatementAsNewBlock();

        return allocate<AST::IfStatement> (context, isConst, condition, trueBranch, falseBranch);
    }

    AST::Statement& parseReturn()
    {
        auto& r = allocate<AST::ReturnStatement> (getContext());

        if (matchIf (Operator::semicolon))
            return r;

        auto returnType = getCurrentBlock().getParentFunction()->returnType;
        SOUL_ASSERT (returnType != nullptr);

        auto& e = parseSuffixes (parseExpression());
        expect (Operator::semicolon);
        r.returnValue = e;
        return r;
    }

    AST::Expression& checkAndCreateArrayElementRef (const AST::Context& c, AST::Expression& lhs,
                                                    pool_ptr<AST::Expression> start,
                                                    pool_ptr<AST::Expression> end)
    {
        if (! (AST::isPossiblyValue (lhs) || AST::isPossiblyEndpoint (lhs)))
            lhs.context.throwError (Errors::expectedValueOrEndpoint());

        if (start != nullptr && ! AST::isPossiblyValue (start))   start->context.throwError (Errors::expectedValue());
        if (end != nullptr   && ! AST::isPossiblyValue (end))     end->context.throwError (Errors::expectedValue());

        if (AST::isResolvedAsConstant (start) && AST::isResolvedAsConstant (end))
        {
            auto startConst = start->getAsConstant();
            auto endConst = end->getAsConstant();

            if (! startConst->value.getType().isInteger())  start->context.throwError (Errors::expectedInteger());
            if (! endConst->value.getType().isInteger())    end->context.throwError (Errors::expectedInteger());

            auto s = startConst->value.getAsInt64();
            auto e = endConst->value.getAsInt64();

            if ((s >= 0 && e >= 0 && s >= e) || (s < 0 && e < 0 && s >= e))
                end->context.throwError (Errors::illegalSliceSize());
        }

        return allocate<AST::ArrayElementRef> (c, lhs, start, end, true);
    }

    AST::Expression& parseSubscriptWithBrackets (AST::Expression& lhs)
    {
        auto context = getContext();
        pool_ptr<AST::Expression> e, end;

        if (matchIf (Operator::colon))
        {
            auto& start = allocate<AST::Constant> (context, Value::createArrayIndex (0));

            if (! matches (Operator::closeBracket))
                end = parseExpression();

            e = checkAndCreateArrayElementRef (context, lhs, start, end);
        }
        else if (matches (Operator::closeBracket))
        {
            e = allocate<AST::SubscriptWithBrackets> (context, lhs, nullptr);
        }
        else
        {
            auto& start = parseExpression();

            if (matchIf (Operator::colon))
            {
                if (! matches (Operator::closeBracket))
                    end = parseExpression();

                e = checkAndCreateArrayElementRef (context, lhs, start, end);
            }
            else
            {
                e = allocate<AST::SubscriptWithBrackets> (context, lhs, start);
            }
        }

        if (matchAndReplaceIf (Operator::closeDoubleBracket, Operator::closeBracket))
            return parseSuffixes (*e);

        expect (Operator::closeBracket);
        return parseSuffixes (*e);
    }

    AST::Expression& parseVectorOrArrayTypeSuffixes (AST::Expression& elementType, ParseTypeContext parseContext)
    {
        auto context = getContext();
        auto startPos = getCurrentTokeniserPosition();

        if (! matchIf (Operator::lessThan))
            return parseArrayTypeSuffixes (elementType, parseContext);

        auto size = tryToParseChvronExpressionIgnoringErrors();

        if (size == nullptr || ! matchIf (Operator::greaterThan))
        {
            resetPosition (startPos);
            return elementType;
        }

        if (matches (Operator::lessThan))
            throwError (Errors::wrongTypeForVectorElement());

        auto& e = allocate<AST::SubscriptWithChevrons> (context, elementType, *size);
        return parseArrayTypeSuffixes (e, parseContext);
    }

    AST::Expression& parseArrayTypeSuffixes (AST::Expression& t, ParseTypeContext parseContext)
    {
        if (matchIf (Operator::openBracket))
            return parseArrayTypeSuffixes (parseSubscriptWithBrackets (t), parseContext);

        if (matches (Operator::bitwiseAnd))
        {
            switch (parseContext)
            {
                case ParseTypeContext::variableType:         throwError (Errors::typeCannotBeReference()); break;
                case ParseTypeContext::eventType:            throwError (Errors::eventTypeCannotBeReference()); break;
                case ParseTypeContext::structMember:         throwError (Errors::memberCannotBeReference()); break;
                case ParseTypeContext::usingDeclTarget:      throwError (Errors::usingCannotBeReference()); break;
                case ParseTypeContext::processorParameter:   throwError (Errors::processorParamsCannotBeReference()); break;
                case ParseTypeContext::metaFunctionArgument: break;
                case ParseTypeContext::nameOrType:           break;

                default:
                    skip();
                    return allocate<AST::TypeMetaFunction> (t.context, t, AST::TypeMetaFunction::Op::makeReference);
            }
        }

        if (matches (Operator::dot))
            return parseDotOperator (t);

        return t;
    }

    pool_ptr<AST::Expression> tryParsingType (ParseTypeContext parseContext)
    {
        auto context = getContext();

        if (matchIf (Keyword::float_))    return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::float32),  parseContext);
        if (matchIf (Keyword::float32))   return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::float32),  parseContext);
        if (matchIf (Keyword::float64))   return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::float64),  parseContext);
        if (matchIf (Keyword::void_))     return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::void_),    parseContext);
        if (matchIf (Keyword::int_))      return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::int32),    parseContext);
        if (matchIf (Keyword::int32))     return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::int32),    parseContext);
        if (matchIf (Keyword::int64))     return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::int64),    parseContext);
        if (matchIf (Keyword::bool_))     return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::bool_),    parseContext);
        if (matchIf (Keyword::complex))   return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::complex32),  parseContext);
        if (matchIf (Keyword::complex32)) return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::complex32),  parseContext);
        if (matchIf (Keyword::complex64)) return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::complex64),  parseContext);
        if (matchIf (Keyword::string))    return parseArrayTypeSuffixes (createConcreteType (context, Type::createStringLiteral()), parseContext);

        if (matchIf (Keyword::const_))
        {
            if (parseContext == ParseTypeContext::structMember)
                throwError (Errors::memberCannotBeConst());

            auto& type = parseType (parseContext);
            return allocate<AST::TypeMetaFunction> (context, type, AST::TypeMetaFunction::Op::makeConst);
        }

        if (matchIf (Keyword::fixed))
        {
            context.throwError (Errors::notYetImplemented ("Fixed point type support"));
            return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::fixed), parseContext);
        }

        if (matches (Token::identifier))
        {
            auto& qi = parseQualifiedIdentifier();
            auto metaTypeOp = getOpForTypeMetaFunctionName (qi);

            if (metaTypeOp != AST::TypeMetaFunction::Op::none && matches (Operator::openParen))
                return parseVectorOrArrayTypeSuffixes (parseTypeMetaFunction (context, metaTypeOp), parseContext);

            return parseVectorOrArrayTypeSuffixes (qi, parseContext);
        }

        return {};
    }


    AST::ConcreteType& createConcreteType (const AST::Context& context, Type t)
    {
        return allocate<AST::ConcreteType> (context, std::move (t));
    }

    static AST::TypeMetaFunction::Op getOpForTypeMetaFunctionName (const AST::QualifiedIdentifier& qi)
    {
        if (qi.getPath().isUnqualified())
            return AST::TypeMetaFunction::getOperationForName (qi.getPath().getFirstPart());

        return AST::TypeMetaFunction::Op::none;
    }

    static AST::TypeMetaFunction::Op getOpForTypeMetaFunctionName (const AST::UnqualifiedName& name)
    {
        return AST::TypeMetaFunction::getOperationForName (name.identifier);
    }

    AST::TypeMetaFunction& parseTypeMetaFunction (const AST::Context& context, AST::TypeMetaFunction::Op op)
    {
        expect (Operator::openParen);
        auto& t = parseType (ParseTypeContext::metaFunctionArgument);
        expect (Operator::closeParen);
        return allocate<AST::TypeMetaFunction> (context, t, op);
    }

    AST::Expression& parseType (ParseTypeContext parseContext)
    {
        auto type = tryParsingType (parseContext);

        if (type == nullptr)
            throwError (Errors::expectedType());

        return *type;
    }

    decltype (AST::EndpointDetails::dataTypes) parseEndpointTypeList (EndpointType endpointType)
    {
        decltype (AST::EndpointDetails::dataTypes) result;
        auto loc = location;

        if (matchIf (Operator::openParen))
        {
            for (;;)
            {
                result.push_back (parseType (ParseTypeContext::eventType));

                if (matchIf (Operator::closeParen))
                    break;

                expect (Operator::comma);
            }
        }
        else
        {
            result.push_back (parseType (ParseTypeContext::eventType));
        }

        if (! isEvent (endpointType) && result.size() > 1)
            loc.throwError (Errors::noMultipleTypesOnEndpoint());

        return result;
    }

    template <typename AddToNamespaceFn>
    void parseVariableDeclaration (AST::Expression& declaredType, Identifier name, bool isExternal,
                                   AST::Context context, AddToNamespaceFn&& addToNamespace)
    {
        for (;;)
        {
            if (AST::isResolvedAsType (declaredType) && declaredType.resolveAsType().isVoid())
                declaredType.context.throwError (Errors::variableCannotBeVoid());

            pool_ptr<AST::Expression> initialValue;
            bool isConst = declaredType.getConstness() == AST::Constness::definitelyConst;

            if (matchIf (Operator::assign))
            {
                if (isExternal)
                    throwError (Errors::externalNeedsInitialiser());

                initialValue = parseSuffixes (parseExpression());

                if (! AST::isPossiblyValue (initialValue))
                    initialValue->context.throwError (Errors::expectedValue());
            }
            else
            {
                isConst = isConst || isExternal;
            }

            auto& v = allocate<AST::VariableDeclaration> (context, declaredType, initialValue, isConst);
            v.isExternal = isExternal;
            addToNamespace (v);

            v.name = name;
            parseAnnotation (v.annotation);

            if (matchIf (Operator::semicolon) || matches (Operator::closeParen))
                break;

            expect (Operator::comma);
            context = getContext();
            name = parseIdentifier();
        }
    }

    template <typename AddToNamespaceFn>
    void parseLetOrVarDeclaration (bool isConst, AddToNamespaceFn&& addToNamespace)
    {
        for (;;)
        {
            auto context = getContext();
            auto name = parseIdentifier();
            expect (Operator::assign);
            auto& initialValue = parseExpression();

            if (! AST::isPossiblyValue (initialValue))
                initialValue.context.throwError (Errors::expectedValue());

            auto& v = allocate<AST::VariableDeclaration> (context, nullptr, initialValue, isConst);
            v.name = name;
            addToNamespace (v);

            if (matchIf (Operator::semicolon))
                break;

            expect (Operator::comma);
        }
    }

    AST::Statement& parseLocalLetOrVar (bool isConst)
    {
        parseLetOrVarDeclaration (isConst, [this] (AST::VariableDeclaration& v)
        {
            AST::Scope::NameSearch search;
            search.partiallyQualifiedPath = IdentifierPath (v.name);
            search.stopAtFirstScopeWithResults = true;
            search.findTypes = false;
            search.findFunctions = false;
            search.findNamespaces = false;
            search.findProcessors = false;
            search.findProcessorInstances = false;
            search.findEndpoints = false;
            search.onlyFindLocalVariables = true;

            auto& currentBlock = getCurrentBlock();
            currentBlock.performFullNameSearch (search, nullptr);
            currentBlock.addStatement (v);

            if (! search.itemsFound.empty())
                v.context.location.emitMessage (Warnings::localVariableShadow (v.name));
        });

        return getNoop();
    }

    void parseTopLevelLetOrVar (bool isLet)
    {
        auto& stateVariables = module->getStateVariableList();

        parseLetOrVarDeclaration (isLet, [&] (AST::VariableDeclaration& v) { stateVariables.push_back (v); });
    }

    void parseProcessorLatencyDeclaration()
    {
        auto& pp = parseProcessorProperty();

        if (pp.property != heart::ProcessorProperty::Property::latency)
            pp.context.throwError (Errors::expectedFunctionOrVariable());

        expect (Operator::assign);
        auto& value = parseExpression();
        expect (Operator::semicolon);

        if (auto p = cast<AST::Processor> (module))
        {
            if (p->latency != nullptr)
                pp.context.throwError (Errors::latencyAlreadyDeclared());

            p->latency = value;
            return;
        }

        pp.context.throwError (Errors::latencyOnlyForProcessor());
    }

    static size_t getMaxNumElements (const Type& arrayOrVectorType)
    {
        if (arrayOrVectorType.isUnsizedArray())
            return (size_t) Type::maxArraySize;

        return (size_t) arrayOrVectorType.getArrayOrVectorSize();
    }

    AST::Statement& parseForLoop()
    {
        expect (Operator::openParen);
        auto& block = allocate<AST::Block> (getContext(), nullptr);
        ScopedScope scope (*this, block);
        auto& loopStatement = allocate<AST::LoopStatement> (getContext());
        auto& loopInitialiser = parseStatement();
        block.addStatement (loopInitialiser);
        block.addStatement (loopStatement);

        if (matches (Operator::closeParen))
        {
            if (auto v = cast<AST::VariableDeclaration> (loopInitialiser))
            {
                loopStatement.rangeLoopInitialiser = v;
                v->doNotConstantFold = true;
                skip();
            }
            else
            {
                expect (Operator::semicolon);
            }
        }
        else
        {
            if (matchIf (Operator::semicolon))
                loopStatement.condition = allocate<AST::Constant> (AST::Context(), Value (true));
            else
                loopStatement.condition = matchEndOfStatement (parseExpression());

            if (! matchIf (Operator::closeParen))
                loopStatement.iterator = matchCloseParen (parseExpression (true));
        }

        loopStatement.body = parseStatement();
        return block;
    }

    AST::Statement& parseLoopStatement()
    {
        auto& loopStatement = allocate<AST::LoopStatement> (getContext());

        if (matchIf (Operator::openParen))
            loopStatement.numIterations = matchCloseParen (parseExpression());

        loopStatement.body = parseStatementAsNewBlock();
        return loopStatement;
    }

    AST::Statement& parseWhileLoop()
    {
        auto& loopStatement = allocate<AST::LoopStatement> (getContext());

        expect (Operator::openParen);
        loopStatement.condition = matchCloseParen (parseExpression());
        loopStatement.body = parseStatementAsNewBlock();

        return loopStatement;
    }

    Identifier parseIdentifier()
    {
        return allocator.identifiers.get (readIdentifier());
    }

    Identifier parseIdentifierWithMaxLength (size_t maxLength)
    {
        if (matches (Token::identifier))
            checkLength (currentStringValue, maxLength);

        return parseIdentifier();
    }

    void checkLength (const std::string& name, size_t maxLength)
    {
        if (name.length() > maxLength)
            throwError (Errors::nameTooLong (name));
    }

    AST::QualifiedIdentifier& parseQualifiedIdentifier()
    {
        auto& qi = allocate<AST::QualifiedIdentifier> (getContext());

        for (;;)
        {
            IdentifierPath path (parseIdentifier());

            while (matchIf (Operator::doubleColon))
                path.addSuffix (parseIdentifier());

            auto p = getCurrentTokeniserPosition();
            auto s = parseSpecialisationArgs();

            if (matchIf (Operator::doubleColon))
            {
                qi.addToPath (path, s);
                continue;
            }

            resetPosition (p);
            qi.addToPath (path, nullptr);
            return qi;
        }
    }

    AST::UnqualifiedName& parseUnqualifiedName()
    {
        auto context = getContext();
        auto identifier = parseIdentifierWithMaxLength (AST::maxIdentifierLength);

        if (matches (Operator::doubleColon))
            throwError (Errors::identifierMustBeUnqualified());

        return allocate<AST::UnqualifiedName> (context, identifier);
    }

    void giveErrorOnSemicolon()
    {
        if (matches (Operator::semicolon))
            throwError (Errors::semicolonAfterBrace());
    }

    void giveErrorOnExternalKeyword()
    {
        if (matches (Keyword::external))
            throwError (Errors::externalOnlyAllowedOnStateVars());
    }

    void giveErrorOnAssignmentToProcessorProperty()
    {
        auto context = getContext();

        if (matchIf (Keyword::processor) && matches (Operator::dot))
        {
            ignoreUnused (parseProcessorProperty());

            if (matches (Operator::assign))
                context.throwError (Errors::cannotAssignToProcessorProperties());

            context.throwError (Errors::expectedStatement());
        }
    }
};

} // namespace soul
