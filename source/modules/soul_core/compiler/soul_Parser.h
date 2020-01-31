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
        X(if_,        "if")         X(do_,        "do")         X(for_,     "for")        X(let,        "let") \
        X(var,        "var")        X(int_,       "int")        X(try_,     "try")        X(else_,      "else") \
        X(bool_,      "bool")       X(true_,      "true")       X(case_,    "case")       X(enum_,      "enum") \
        X(loop,       "loop")       X(void_,      "void")       X(while_,   "while")      X(break_,     "break") \
        X(const_,     "const")      X(int32,      "int32")      X(int64,    "int64")      X(float_,     "float") \
        X(false_,     "false")      X(using_,     "using")      X(fixed,    "fixed")      X(graph,      "graph") \
        X(input,      "input")      X(event,      "event")      X(class_,   "class")      X(catch_,     "catch") \
        X(throw_,     "throw")      X(output,     "output")     X(return_,  "return")     X(string,     "string") \
        X(struct_,    "struct")     X(import,     "import")     X(switch_,  "switch")     X(public_,    "public") \
        X(double_,    "double")     X(private_,   "private")    X(float32,  "float32")    X(float64,    "float64") \
        X(default_,   "default")    X(continue_,  "continue")   X(external, "external")   X(operator_,  "operator") \
        X(processor,  "processor")  X(namespace_, "namespace")  X(connection, "connection")

    SOUL_KEYWORDS (SOUL_DECLARE_TOKEN)

    struct Matcher
    {
        static TokenType match (int len, UTF8Reader p) noexcept
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
    static TokenType match (UTF8Reader& text) noexcept
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
    static constexpr bool isIdentifierStart (UnicodeChar c) noexcept  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    static constexpr bool isIdentifierBody  (UnicodeChar c) noexcept  { return isIdentifierStart (c) || (c >= '0' && c <= '9') || c == '_'; }
};

//==============================================================================
/** Creates a rough-and-ready AST from the tokenised source code, ready for
    refinement in later stages of the compilation process
*/
struct StructuralParser   : public Tokeniser<Keyword::Matcher,
                                             StandardOperatorMatcher,
                                             StandardIdentifierMatcher>
{
    static std::vector<pool_ref<AST::ModuleBase>> parseTopLevelDeclarations (AST::Allocator& allocator, CodeLocation code,
                                                                             AST::Namespace& parentNamespace)
    {
        StructuralParser p (allocator, code, parentNamespace);
        p.parseTopLevelDecls (parentNamespace);
        return parentNamespace.subModules;
    }

    static pool_ptr<AST::ProcessorBase> cloneProcessorWithNewName (AST::Allocator& allocator,
                                                                   AST::Namespace& parentNamespace,
                                                                   const AST::ProcessorBase& itemToClone,
                                                                   const std::string& newName)
    {
        StructuralParser p (allocator, itemToClone.context.location, parentNamespace);
        auto newNameID = allocator.identifiers.get (newName);
        p.newNameForFirstDecl = &newNameID;

        if (itemToClone.isProcessor())  return p.parseProcessorDecl (parentNamespace);
        if (itemToClone.isGraph())      return p.parseGraphDecl (parentNamespace);

        SOUL_ASSERT_FALSE;
        return {};
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
    Identifier* newNameForFirstDecl = nullptr;

    // Bit of a bodge here as a simple way to parse things like float<2 + 2>, this
    // just forces the parser to ignore any > tokens when parsing an expression. Could be
    // done more elegantly in future...
    int ignoreGreaterThanToken = 0;

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
        : Tokeniser (code), allocator (a), currentScope (std::addressof (parentScope))
    {
    }

    ~StructuralParser() override = default;

    template <typename Type, typename... Args>
    Type& allocate (Args&&... args) const    { return allocator.allocate<Type> (std::forward<Args> (args)...); }

    AST::Expression& matchCloseParen (AST::Expression& e)                 { expect (Operator::closeParen); return e; }
    template<typename ExpType> ExpType& matchEndOfStatement (ExpType& e)  { expect (Operator::semicolon);  return e; }

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

        if (matchIf (Keyword::processor))      { parseProcessorDecl (parentNamespace); return; }
        if (matchIf (Keyword::graph))          { parseGraphDecl     (parentNamespace); return; }
        if (matchIf (Keyword::namespace_))     { parseNamespaceDecl (parentNamespace); return; }
        if (matches (Keyword::import))         throwError (Errors::importsMustBeAtStart());

        throwError (Errors::expectedTopLevelDecl());
    }

    AST::Processor&  parseProcessorDecl (AST::Namespace& ns)   { return parseTopLevelItem<AST::Processor> (ns); }
    AST::Graph&      parseGraphDecl     (AST::Namespace& ns)   { return parseTopLevelItem<AST::Graph>     (ns); }
    AST::Namespace&  parseNamespaceDecl (AST::Namespace& ns)   { return parseTopLevelItem<AST::Namespace> (ns); }

    template <typename ModuleType>
    ModuleType& parseTopLevelItem (AST::Namespace& parentNamespace)
    {
        auto context = getContext();
        auto name = parseIdentifierWithMaxLength (AST::maxIdentifierLength);

        if (newNameForFirstDecl != nullptr)
        {
            name = *newNameForFirstDecl;
            newNameForFirstDecl = nullptr;
        }

        auto& newModule = allocate<ModuleType> (context, name);
        parentNamespace.subModules.push_back (newModule);

        auto newNamespace = cast<AST::Namespace> (newModule);
        ScopedScope scope (*this, newModule);
        auto oldModule = module;
        module = newModule;

        if (newNamespace != nullptr && matchIf (Operator::doubleColon))
            parseTopLevelItem<ModuleType> (*newNamespace);
        else
            parseTopLevelDeclContent();

        module = oldModule;
        return newModule;
    }

    void parseImports (AST::Namespace& parentNamespace)
    {
        while (matchIf (Keyword::import))
        {
            if (matches (Token::literalString))
            {
                parentNamespace.importsList.addIfNotAlreadyThere (currentStringValue);
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
        if (auto p = cast<AST::ProcessorBase> (module))
        {
            parseSpecialisationParameters (*p);
            parseAnnotation (p->annotation);
            expect (Operator::openBrace);

            if (auto g = cast<AST::Graph> (p))
            {
                while (parseEndpoint (*g)
                        || parseProcessorInstanceList (*g)
                        || parseConnectionList (*g))
                {}

                expect (Operator::closeBrace);
                return;
            }

            while (parseEndpoint (*p))
            {}
        }
        else
        {
            expect (Operator::openBrace);
        }

        if (auto ns = cast<AST::Namespace> (module))
            parseImports (*ns);

        while (! matchIf (Operator::closeBrace))
        {
            if (matchIf (Keyword::struct_))
            {
                parseStructDeclaration();
            }
            else if (matchIf (Keyword::using_))
            {
                parseUsingDeclaration();
            }
            else if (matchIf (Keyword::namespace_))
            {
                if (auto ns = cast<AST::Namespace> (module))
                    parseNamespaceDecl (*ns);
                else
                    throwError (Errors::namespaceMustBeInsideNamespace());
            }
            else if (matchIf (Keyword::processor))
            {
                if (auto ns = cast<AST::Namespace> (module))
                    parseProcessorDecl (*ns);
                else
                    throwError (Errors::processorMustBeInsideNamespace());
            }
            else if (matchIf (Keyword::graph))
            {
                if (auto ns = cast<AST::Namespace> (module))
                    parseGraphDecl (*ns);
                else
                    throwError (Errors::graphMustBeInsideNamespace());
            }
            else if (matchIf (Keyword::let))
            {
                parseTopLevelLetOrVar (true);
            }
            else if (matchIf (Keyword::var))
            {
                parseTopLevelLetOrVar (false);
            }
            else if (matchIf (Keyword::event))
            {
                parseEventFunction();
            }
            else if (matchesAny (Keyword::input, Keyword::output))
            {
                if (module->isNamespace())
                    throwError (Errors::namespaceCannotContainEndpoints());
                else
                    throwError (Errors::endpointDeclsMustBeFirst());
            }
            else if (matches (Keyword::import))
            {
                throwError (Errors::importsMustBeAtStart());
            }
            else
            {
                parseFunctionOrStateVariable();
            }
        }

        giveErrorOnSemicolon();
    }

    void parseFunctionOrStateVariable()
    {
        auto declarationContext = getContext();
        bool isExternal = matchIf (Keyword::external);

        if (matches ("static_assert"))
            declarationContext.throwError (Errors::staticAssertionNotAllowed());

        auto type = tryParsingType (ParseTypeContext::variableType);

        if (type == nullptr)
            declarationContext.throwError (Errors::expectedFunctionOrVariable());

        auto context = getContext();
        auto name = parseIdentifier();

        std::vector<pool_ref<AST::QualifiedIdentifier>> genericWildcards;

        if (matchIf (Operator::lessThan))
            genericWildcards = parseGenericFunctionWildcardList();

        if (matchIf (Operator::openParen))
        {
            if (isExternal)
                declarationContext.throwError (Errors::functionCannotBeExternal());

            if (auto functions = module->getFunctionList())
                functions->push_back (parseFunctionDeclaration (declarationContext, *type, name, context, genericWildcards));
            else
                declarationContext.throwError (Errors::noFunctionInThisScope());
        }
        else
        {
            if (isExternal && type->getConstness() == AST::Constness::definitelyConst)
                declarationContext.throwError (Errors::noConstOnExternals());

            if (auto stateVariables = module->getStateVariableList())
            {
                parseVariableDeclaration (*type, name, isExternal, context,
                                          [&] (AST::VariableDeclaration& v)  { stateVariables->push_back (v); });
            }
            else
            {
                throwError (Errors::noVariableInThisScope());
            }
        }
    }

    void parseUsingDeclaration()
    {
        auto usingList = module->getUsingList();

        if (usingList == nullptr)
            throwError (Errors::usingDeclNotAllowed());

        auto context = getContext();
        auto name = parseIdentifier();
        expect (Operator::assign);
        auto& type = parseType (ParseTypeContext::usingDeclTarget);
        expect (Operator::semicolon);
        usingList->push_back (allocate<AST::UsingDeclaration> (context, name, type));
    }

    void parseStructDeclaration()
    {
        auto structs = module->getStructList();

        if (structs == nullptr)
            throwError (Errors::structDeclNotAllowed());

        auto context = getContext();
        auto name = parseIdentifier();
        expect (Operator::openBrace);

        auto& newStruct = allocate<AST::StructDeclaration> (context, name);
        structs->push_back (newStruct);

        while (! matchIf (Operator::closeBrace))
        {
            auto& type = parseType (ParseTypeContext::structMember);

            for (;;)
            {
                newStruct.addMember (type, parseIdentifier());

                if (matchIf (Operator::comma))
                    continue;

                expect (Operator::semicolon);
                break;
            }
        }

        giveErrorOnSemicolon();
    }

    //==============================================================================
    void parseSpecialisationParameters (AST::ProcessorBase& p)
    {
        if (matchIf (Operator::openParen))
        {
            if (matchIf (Operator::closeParen))
                return;

            do
            {
                if (matchIf (Keyword::using_))
                {
                    if (p.isGraph())
                        throwError (Errors::graphCannotHaveSpecialisations());

                    auto context = getContext();
                    auto name = parseIdentifier();
                    p.addSpecialisationParameter (allocate<AST::UsingDeclaration> (context, name, nullptr));
                }
                else if (matchIf (Keyword::processor))
                {
                    if (! p.isGraph())
                        throwError (Errors::processorSpecialisationNotAllowed());

                    auto context = getContext();
                    p.addSpecialisationParameter (allocate<AST::ProcessorAliasDeclaration> (context, parseIdentifier()));
                }
                else
                {
                    giveErrorOnExternalKeyword();
                    auto& parameterType = parseType (ParseTypeContext::processorParameter);
                    auto& parameterVariable = allocate<AST::VariableDeclaration> (getContext(), parameterType, nullptr, true);
                    parameterVariable.name = parseIdentifier();
                    p.addSpecialisationParameter (parameterVariable);
                }
            }
            while (matchIf (Operator::comma));

            expect (Operator::closeParen);
        }
    }

    template <typename TokenName, typename ParserFn>
    bool parseOptionallyBracedList (TokenName type, ParserFn&& parserFn)
    {
        if (! matchIf (type))
            return false;

        if (matchIf (Operator::openBrace))
        {
            while (! matchIf (Operator::closeBrace))
                parserFn();
        }
        else
        {
            parserFn();
        }

        return true;
    }

    bool parseConnectionList (AST::Graph& g)        { return parseOptionallyBracedList (Keyword::connection, [this, &g] { parseConnection (g); }); }
    bool parseProcessorInstanceList (AST::Graph& g) { return parseOptionallyBracedList (Keyword::let,        [this, &g] { parseProcessorInstance (g); }); }

    //==============================================================================
    void parseConnection (AST::Graph& graph)
    {
        auto interpolationType = parseOptionalInterpolationType();
        auto context = getContext();
        ArrayWithPreallocation<AST::Connection::NameAndEndpoint, 8> sources, dests;
        pool_ptr<AST::Expression> delayLength;

        for (;;)
        {
            sources.push_back (parseConnectionIdentifier());

            if (matchIf (Operator::comma))
                continue;

            context = getContext();
            expect (Operator::rightArrow);
            delayLength = parseDelayLength();
            break;
        }

        for (;;)
        {
            dests.push_back (parseConnectionIdentifier());

            if (matchIf (Operator::comma))
                continue;

            expect (Operator::semicolon);
            break;
        }

        if (sources.size() > 1 && dests.size() > 1)
            context.throwError (Errors::notYetImplemented ("Many-to-many connections are not currently supported"));

        for (auto& source : sources)
            for (auto& dest : dests)
                graph.connections.push_back (allocate<AST::Connection> (context, interpolationType, source, dest, delayLength));
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

    AST::Connection::NameAndEndpoint parseConnectionIdentifier()
    {
        if (! matches (Token::identifier))
            getContext().throwError (Errors::expectedProcessorOrEndpoint());

        AST::Connection::NameAndEndpoint result;
        result.processorName = parseQualifiedIdentifier();

        if (matchIf (Operator::openBracket))
        {
            result.processorIndex = parseExpression();
            expect (Operator::closeBracket);
        }

        if (result.processorIndex)
            throwError (Errors::notYetImplemented ("Processor indexes"));

        if (matchIf (Operator::dot))
        {
            result.endpoint = parseIdentifier();
        }
        else
        {
            if (! result.processorName->path.isUnqualified())
                result.processorName->context.throwError (Errors::expectedUnqualifiedName());

            result.endpoint = result.processorName->path.getFirstPart();
            result.processorName->path = {};
        }

        if (matchIf (Operator::openBracket))
        {
            result.endpointIndex = parseExpression();
            expect (Operator::closeBracket);
        }

        if (result.endpointIndex)
            throwError (Errors::notYetImplemented ("Channel indexes"));

        return result;
    }

    void parseProcessorInstance (AST::Graph& graph)
    {
        auto& u = allocate<AST::ProcessorInstance> (getContext());
        auto nameLocation = getContext();
        u.instanceName = parseQualifiedIdentifier();

        if (! u.instanceName->path.isUnqualified())
            u.instanceName->context.throwError (Errors::expectedUnqualifiedName());

        for (auto& i : graph.processorInstances)
            if (*i->instanceName == *u.instanceName)
                nameLocation.throwError (Errors::nameInUse (u.instanceName->path));

        graph.processorInstances.push_back (u);

        expect (Operator::assign);
        u.targetProcessor = parseQualifiedIdentifier();

        // Array of processors
        if (matchIf (Operator::openBracket))
        {
            u.arraySize = parseExpression();
            expect (Operator::closeBracket);
        }

        // Parameterised
        if (matchIf (Operator::openParen))
        {
            if (! matchIf (Operator::closeParen))
            {
                for (;;)
                {
                    auto context = getContext();
                    u.specialisationArgs.push_back (parseProcessorSpecialisationValueOrType());

                    if (matchIf (Operator::closeParen))
                        break;

                    expect (Operator::comma);
                }
            }
        }

        // Clocked
        if (matchIf (Operator::times))
            u.clockMultiplierRatio = parseExpression();
        else if (matchIf (Operator::divide))
            u.clockDividerRatio = parseExpression();

        expect (Operator::semicolon);
    }

    AST::Expression& parseProcessorSpecialisationValueOrType()
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
            if (p.isGraph() && matches (Token::identifier) && ! isNextTokenEndpointKind (*this))
                return parseChildEndpoint (p, isInput);

            auto kind = parseEndpointKind (*this);

            if (matchIf (Operator::openBrace))
            {
                while (! matchIf (Operator::closeBrace))
                    parseEndpoint (p, isInput, kind);
            }
            else
            {
                parseEndpoint (p, isInput, kind);
            }
        }
    }

    void parseEndpoint (AST::ProcessorBase& p, bool isInput, EndpointKind kind)
    {
        auto& first = allocate<AST::EndpointDeclaration> (getContext(), isInput);
        first.details = std::make_unique<AST::EndpointDetails> (kind);
        first.details->sampleTypes = parseEndpointTypeList (kind);
        parseInputOrOutputName (first);
        p.endpoints.push_back (first);

        while (matchIf (Operator::comma))
        {
            auto& e = allocate<AST::EndpointDeclaration> (getContext(), isInput);
            e.details = std::make_unique<AST::EndpointDetails> (kind);
            e.details->sampleTypes = first.details->sampleTypes;
            parseInputOrOutputName (e);
            p.endpoints.push_back (e);
        }

        expect (Operator::semicolon);
    }

    void parseInputOrOutputName (AST::EndpointDeclaration& e)
    {
        e.name = parseIdentifierWithMaxLength (AST::maxIdentifierLength);

        if (matchIf (Operator::openBracket))
        {
            e.details->arraySize = parseExpression();
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
            e.childPath = std::make_unique<AST::ChildEndpointPath>();
            bool canParseName = true;

            for (;;)
            {
                AST::ChildEndpointPath::PathSection path;

                if (matchIf (Operator::times))
                {
                    throwError (Errors::notYetImplemented ("Wildcard child endpoint references"));
                    path.isWildcard = true;
                }
                else
                {
                    path.name = parseQualifiedIdentifier();

                    if (path.name->path.isQualified())
                        path.name->context.throwError (Errors::expectedUnqualifiedName());
                }

                if (matchIf (Operator::openBracket))
                {
                    path.index = parseExpression();
                    expect (Operator::closeBracket);
                }

                e.childPath->sections.push_back (path);

                if (path.isWildcard)
                {
                    canParseName = false;
                    break;
                }

                if (matchIf (Operator::dot))
                    continue;

                break;
            }

            if (canParseName)
            {
                if (matches (Token::identifier))
                    e.name = parseIdentifier();
                else
                    e.name = e.childPath->sections.back().name->path.getLastPart();

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

                auto& key = allocate<AST::QualifiedIdentifier> (context, IdentifierPath (allocator.get (name)));

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
        f.block = parseBlock (f);
    }

    //==============================================================================
    std::vector<pool_ref<AST::QualifiedIdentifier>> parseGenericFunctionWildcardList()
    {
        std::vector<pool_ref<AST::QualifiedIdentifier>> wildcards;

        for (;;)
        {
            if (! matches (Token::identifier))
                throwError (Errors::expectedGenericWildcardName());

            auto& wildcard = parseQualifiedIdentifier();

            if (wildcard.path.isQualified())
                wildcard.context.throwError (Errors::qualifierOnGeneric());

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
                                             std::vector<pool_ref<AST::QualifiedIdentifier>> genericWildcards)
    {
        if (AST::isResolvedAsType (returnType) && returnType.getConstness() == AST::Constness::definitelyConst)
            throwError (Errors::functionReturnTypeCannotBeConst());

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
            f.block = parseBlock (f);

        return f;
    }

    AST::Block& parseBlock (pool_ptr<AST::Function> ownerFunction)
    {
        expect (Operator::openBrace);
        auto& newBlock = allocate<AST::Block> (getContext(), ownerFunction);
        ScopedScope scope (*this, newBlock);

        while (! matchIf (Operator::closeBrace))
            newBlock.addStatement (parseStatement());

        return newBlock;
    }

    AST::Statement& parseStatement()
    {
        if (matches (Operator::openBrace))     return parseBlock ({});
        if (matchIf (Keyword::if_))            return parseIf();
        if (matchIf (Keyword::while_))         return parseDoOrWhileLoop (false);
        if (matchIf (Keyword::do_))            return parseDoOrWhileLoop (true);
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

        if (matchesAny (Token::literalInt32, Token::literalInt64, Token::literalFloat64, Token::literalFloat32,
                        Token::literalString, Operator::minus))
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
                    parseVariableDeclaration (*type, parseIdentifier(), false, context,
                                              [this] (AST::VariableDeclaration& v) { getCurrentBlock().addStatement (v); });
                    return getNoop();
                }
            }

            resetPosition (oldPos);
        }

        if (! matches (Token::identifier))
        {
            giveErrorOnExternalKeyword();
            throwError (Errors::expectedStatement());
        }

        return parseExpressionAsStatement (true);
    }

    pool_ptr<AST::Expression> tryToParseExpressionIgnoringErrors()
    {
        pool_ptr<AST::Expression> result;
        catchParseErrors ([this, &result] { result = parseExpression(); });
        return result;
    }

    AST::Expression& parseExpression (bool allowAssignment = false)
    {
        auto& lhs = parseTernaryOperator();

        // Re-wite equals operators as binary operators
        // e.g. X += n -> X = X + n
        if (matchIf (Operator::plusEquals))                 return parseInPlaceOpExpression (lhs, BinaryOp::Op::add);
        if (matchIf (Operator::minusEquals))                return parseInPlaceOpExpression (lhs, BinaryOp::Op::subtract);
        if (matchIf (Operator::timesEquals))                return parseInPlaceOpExpression (lhs, BinaryOp::Op::multiply);
        if (matchIf (Operator::divideEquals))               return parseInPlaceOpExpression (lhs, BinaryOp::Op::divide);
        if (matchIf (Operator::moduloEquals))               return parseInPlaceOpExpression (lhs, BinaryOp::Op::modulo);
        if (matchIf (Operator::leftShiftEquals))            return parseInPlaceOpExpression (lhs, BinaryOp::Op::leftShift);
        if (matchIf (Operator::rightShiftEquals))           return parseInPlaceOpExpression (lhs, BinaryOp::Op::rightShift);
        if (matchIf (Operator::rightShiftUnsignedEquals))   return parseInPlaceOpExpression (lhs, BinaryOp::Op::rightShiftUnsigned);
        if (matchIf (Operator::xorEquals))                  return parseInPlaceOpExpression (lhs, BinaryOp::Op::bitwiseXor);
        if (matchIf (Operator::andEquals))                  return parseInPlaceOpExpression (lhs, BinaryOp::Op::bitwiseAnd);
        if (matchIf (Operator::orEquals))                   return parseInPlaceOpExpression (lhs, BinaryOp::Op::bitwiseOr);

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
        auto& e = matchEndOfStatement (parseExpression (allowAssignment));

        if (e.isCompileTimeConstant())
            e.context.throwError (Errors::expressionHasNoEffect());

        return e;
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
            if (! (matchesAny (Operator::lessThan, Operator::lessThanOrEqual, Operator::greaterThanOrEqual)
                    || (matches (Operator::greaterThan) && ignoreGreaterThanToken == 0)))
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
                     || (matchesAny (Token::literalFloat64, Token::literalFloat32) && literalDoubleValue < 0))
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
        if (matches (Token::literalFloat32))   return createLiteral (Value ((float) literalDoubleValue));
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

    AST::Expression& parseProcessorProperty()
    {
        expect (Operator::dot);
        auto context = getContext();
        auto& propertyName = parseQualifiedIdentifier();

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

        for (;;)
        {
            if (matchIf (Operator::closeParen))
                break;

            auto& e = parseExpression();

            if (list.items.size() >= AST::maxInitialiserListLength)
                e.context.throwError (Errors::tooManyInitialisers());

            if (! canContainTypes && e.kind == AST::ExpressionKind::type)
                e.context.throwError (Errors::typeReferenceNotAllowed());

            if (! canContainProcessors && e.kind == AST::ExpressionKind::processor)
                e.context.throwError (Errors::processorReferenceNotAllowed());

            list.items.push_back (e);

            if (matchIf (Operator::comma))
                continue;
        }

        return list;
    }

    AST::Expression& parseDotOperator (AST::Expression& expression)
    {
        auto context = getContext();
        expect (Operator::dot);
        auto& propertyOrMethodName = parseQualifiedIdentifier();

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
                return parseSuffixes (allocate<AST::CallOrCast> (dot->rhs, args, true));
            }

            return parseSuffixes (allocate<AST::CallOrCast> (expression, args, false));
        }

        if (matchIf (Operator::openBracket))  return parseSubscriptWithBrackets (expression);
        if (matchIf (Operator::plusplus))     return parsePostIncDec (expression, true);
        if (matchIf (Operator::minusminus))   return parsePostIncDec (expression, false);

        return expression;
    }

    AST::Expression& parseInPlaceOpExpression (AST::Expression& lhs, BinaryOp::Op opType)
    {
        auto context = getContext();
        auto& rhs = parseExpression();
        return allocate<AST::Assignment> (context, lhs, createBinaryOperator (context, lhs, rhs, opType));
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
        auto& trueBranch = parseStatement();
        pool_ptr<AST::Statement> falseBranch;

        if (matchIf (Keyword::else_))
            falseBranch = parseStatement();

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
        r.returnValue = castExpressionToTargetType (*returnType, e);
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

        ++ignoreGreaterThanToken;
        auto size = tryToParseExpressionIgnoringErrors();
        --ignoreGreaterThanToken;

        if (size == nullptr || ! matchIf (Operator::greaterThan))
        {
            resetPosition (startPos);
            return elementType;
        }

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

        if (matchIf (Keyword::float_))   return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::float32),  parseContext);
        if (matchIf (Keyword::float32))  return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::float32),  parseContext);
        if (matchIf (Keyword::float64))  return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::float64),  parseContext);
        if (matchIf (Keyword::void_))    return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::void_),    parseContext);
        if (matchIf (Keyword::int_))     return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::int32),    parseContext);
        if (matchIf (Keyword::int32))    return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::int32),    parseContext);
        if (matchIf (Keyword::int64))    return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::int64),    parseContext);
        if (matchIf (Keyword::bool_))    return parseVectorOrArrayTypeSuffixes (createConcreteType (context, PrimitiveType::bool_),    parseContext);
        if (matchIf (Keyword::string))   return parseArrayTypeSuffixes (createConcreteType (context, Type::createStringLiteral()), parseContext);

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
        if (qi.path.isUnqualified())
            return AST::TypeMetaFunction::getOperationForName (qi.path.getFirstPart());

        return AST::TypeMetaFunction::Op::none;
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

    std::vector<pool_ref<AST::Expression>> parseEndpointTypeList (EndpointKind kind)
    {
        std::vector<pool_ref<AST::Expression>> result;
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

        if (! isEvent (kind) && result.size() > 1)
            loc.throwError (Errors::noMultipleTypesOnEndpoint());

        return result;
    }

    template <typename AddToNamespaceFn>
    void parseVariableDeclaration (AST::Expression& declaredType, Identifier name, bool isExternal,
                                   const AST::Context& context, AddToNamespaceFn&& addToNamespace)
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

            if (matchIf (Operator::semicolon))
                break;

            expect (Operator::comma);
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
        parseLetOrVarDeclaration (isConst, [this] (AST::VariableDeclaration& v) { getCurrentBlock().addStatement (v); });
        return getNoop();
    }

    void parseTopLevelLetOrVar (bool isLet)
    {
        if (auto stateVariables = module->getStateVariableList())
            parseLetOrVarDeclaration (isLet, [&] (AST::VariableDeclaration& v) { stateVariables->push_back (v); });
        else
            throwError (Errors::noVariableInThisScope());
    }

    AST::Expression& castExpressionToTargetType (AST::Expression& targetType, AST::Expression& source)
    {
        auto list = cast<AST::CommaSeparatedList> (source);

        if (list == nullptr)
        {
            if (AST::isResolvedAsType (targetType) && AST::isResolvedAsValue (source))
            {
                auto type = targetType.resolveAsType();

                if (source.getResultType().isIdentical (type))
                    return source;

                return allocate<AST::TypeCast> (source.context, type, source);
            }

            list = allocate<AST::CommaSeparatedList> (source.context);
            list->items.push_back (source);
        }

        return allocate<AST::CallOrCast> (targetType, *list, false);
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
        auto& loopStatement = allocate<AST::LoopStatement> (getContext(), false);
        block.addStatement (parseStatement());
        block.addStatement (loopStatement);

        if (matchIf (Operator::semicolon))
            loopStatement.condition = allocate<AST::Constant> (getContext(), Value (true));
        else
            loopStatement.condition = matchEndOfStatement (parseExpression());

        if (! matchIf (Operator::closeParen))
            loopStatement.iterator = matchCloseParen (parseExpression (true));

        loopStatement.body = parseStatement();
        return block;
    }

    AST::Statement& parseLoopStatement()
    {
        auto& loopStatement = allocate<AST::LoopStatement> (getContext(), false);

        if (matchIf (Operator::openParen))
            loopStatement.numIterations = matchCloseParen (parseExpression());

        loopStatement.body = parseStatement();
        return loopStatement;
    }

    AST::Statement& parseDoOrWhileLoop (bool isDoLoop)
    {
        auto& loopStatement = allocate<AST::LoopStatement> (getContext(), isDoLoop);

        if (isDoLoop)
        {
            loopStatement.body = parseBlock ({});
            expect (Keyword::while_);
        }

        expect (Operator::openParen);
        loopStatement.condition = matchCloseParen (parseExpression());

        if (! isDoLoop)
            loopStatement.body = parseStatement();

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
        auto context = getContext();
        IdentifierPath path (parseIdentifier());

        while (matchIf (Operator::doubleColon))
            path.addSuffix (parseIdentifier());

        return allocate<AST::QualifiedIdentifier> (context, path);
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
};

} // namespace soul
