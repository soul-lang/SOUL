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

template <typename ASTType>
static std::string getFullPathForASTObject (ASTType& o)
{
    if (auto scope = o.getParentScope())
    {
        IdentifierPath parentPath;

        if (auto fn = scope->getAsFunction())
            parentPath = IdentifierPath (fn->getParentScope()->getFullyQualifiedPath(), fn->name);
        else
            parentPath = scope->getFullyQualifiedPath();

        return Program::stripRootNamespaceFromQualifiedPath (IdentifierPath (parentPath, o.name).toString());
    }

    return o.name.toString();
}

static SourceCodeUtilities::Comment getComment (const AST::Context& context)
{
    return SourceCodeUtilities::findPrecedingComment (context.location);
}

static bool shouldIncludeComment (const SourceCodeUtilities::Comment& comment)
{
    return comment.isDoxygenStyle || ! comment.range.isEmpty();
}

static SourceCodeModel::Expression operator+ (SourceCodeModel::Expression a, SourceCodeModel::Expression&& b)
{
    a.sections.reserve (a.sections.size() + b.sections.size());

    for (auto& s : b.sections)
        a.sections.push_back (std::move (s));

    return a;
}

std::string SourceCodeModel::Expression::toString() const
{
    std::string result;

    for (auto& s : sections)
        result += s.text;

    return result;
}

//==============================================================================
static std::string makeUID (std::string_view name)
{
    return retainCharacters (choc::text::replace (name, " ", "_", "::", "_"),
                             "_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
}

static std::string makeUID (AST::ModuleBase& m)             { return makeUID ("mod_" + Program::stripRootNamespaceFromQualifiedPath (m.getFullyQualifiedDisplayPath().toString())); }
static std::string makeUID (AST::TypeDeclarationBase& t)    { return makeUID ("type_" + getFullPathForASTObject (t)); }
static std::string makeUID (AST::VariableDeclaration& v)    { return makeUID ("var_" + getFullPathForASTObject (v)); }
static std::string makeUID (AST::EndpointDeclaration& e)    { return makeUID ("endpoint_" + getFullPathForASTObject (e)); }
static std::string makeUID (AST::Function& f)               { return makeUID ("fn_" + getFullPathForASTObject (f)); }
static std::string makeUID (AST::ProcessorInstance& p)      { return makeUID ("procinst_" + IdentifierPath (p.getParentScope()->getFullyQualifiedPath(), p.instanceName->identifier).toString()); }

//==============================================================================
static bool shouldShow (const AST::Function& f)
{
    return shouldIncludeComment (getComment (f.context));
}

static bool shouldShow (const AST::VariableDeclaration& v)
{
    return ! v.isSpecialisation;
}

static bool shouldShow (const AST::StructDeclaration&)
{
    return true; // TODO
}

static bool shouldShow (AST::ModuleBase& module, const SourceCodeModel::Module& m)
{
    if (m.isProcessor || m.isGraph)
        return true;

    if (shouldIncludeComment (m.comment))
        return true;

    if (auto functions = module.getFunctionList())
        for (auto& f : *functions)
            if (shouldShow (f))
                return true;

    for (auto& v : module.getStateVariableList())
        if (shouldShow (v))
            return true;

    for (auto& s : module.getStructDeclarations())
        if (shouldShow (s))
            return true;

    return false;
}

//==============================================================================
struct ExpressionHelpers
{
    static SourceCodeModel::Expression create (AST::Expression& e, const StringDictionary& dictionary)
    {
        if (auto s = cast<AST::SubscriptWithBrackets> (e))  return create (s->lhs, dictionary) + createText ("[") + createIfNotNull (s->rhs, dictionary) + createText ("]");
        if (auto s = cast<AST::SubscriptWithChevrons> (e))  return create (s->lhs, dictionary) + createText ("<") + createIfNotNull (s->rhs, dictionary) + createText (">");
        if (auto d = cast<AST::DotOperator> (e))            return create (d->lhs, dictionary) + createText (".") + createText (d->rhs.identifier.toString());
        if (auto q = cast<AST::QualifiedIdentifier> (e))    return fromIdentifier (*q);
        if (auto c = cast<AST::Constant> (e))               return createText (c->value.getDescription (std::addressof (dictionary)));

        if (auto m = cast<AST::TypeMetaFunction> (e))
        {
            if (m->operation == AST::TypeMetaFunction::Op::makeReference)
                return create (m->source, dictionary) + createText ("&");

            if (m->operation == AST::TypeMetaFunction::Op::makeConst)
                return createKeyword ("const ") + create (m->source, dictionary);

            return create (m->source, dictionary) + createText (".") + createText (AST::TypeMetaFunction::getNameForOperation (m->operation));
        }

        SourceCodeModel::Expression result;
        catchParseErrors ([&] { result = create (e.resolveAsType()); });

        if (! result.sections.empty())
            return result;

        return createText (choc::text::trim (SourceCodeUtilities::findRangeOfASTObject (e).toString()));
    }

    static SourceCodeModel::Expression create (const Type& t)
    {
        if (t.isConst())          return createKeyword ("const ") + create (t.removeConst());
        if (t.isReference())      return create (t.removeReference()) + createText ("&");
        if (t.isVector())         return create (t.getPrimitiveType()) + createText ("<" + std::to_string (t.getVectorSize()) + ">");
        if (t.isUnsizedArray())   return create (t.getArrayElementType()) + createText ("[]");
        if (t.isArray())          return create (t.getArrayElementType()) + createText ("[" + std::to_string (t.getArraySize()) + "]");
        if (t.isWrapped())        return createKeyword ("wrap") + createText ("<" + std::to_string (t.getBoundedIntLimit()) + ">");
        if (t.isClamped())        return createKeyword ("clamp") + createText ("<" + std::to_string (t.getBoundedIntLimit()) + ">");
        if (t.isStruct())         return createStruct (t.getStructRef());
        if (t.isStringLiteral())  return createPrimitive ("string");

        return createPrimitive (t.getPrimitiveType().getDescription());
    }

    static SourceCodeModel::Expression forVariable (AST::VariableDeclaration& v, const StringDictionary& dictionary)
    {
        if (v.declaredType != nullptr)
            return create (*v.declaredType, dictionary);

        SOUL_ASSERT (v.initialValue != nullptr);

        if (v.initialValue->isResolved())
            return create (v.initialValue->getResultType());

        if (auto cc = cast<AST::CallOrCast> (v.initialValue))
            return create (cc->nameOrType, dictionary);

        return {};
    }

    static SourceCodeModel::Expression fromSection (SourceCodeModel::Expression::Section&& s)
    {
        SourceCodeModel::Expression d;
        d.sections.push_back (std::move (s));
        return d;
    }

    static SourceCodeModel::Expression fromIdentifier (AST::QualifiedIdentifier& q)
    {
        auto text = q.toString();

        if (text == "wrap" || text == "clamp")
            return createPrimitive (text);

        if (q.isSimplePath())
        {
            if (auto parentModule = q.getParentScope()->findModule())
            {
                auto resolvedUID = resolvePartialNameAsUID (*parentModule, q.getPath());

                if (! resolvedUID.empty())
                    return createStruct (text, resolvedUID);
            }
        }

        return createText (text);
    }

    static SourceCodeModel::Expression createIfNotNull (pool_ptr<AST::Expression> e, const StringDictionary& dictionary)
    {
        return e != nullptr ? create (*e, dictionary) : SourceCodeModel::Expression();
    }

    static SourceCodeModel::Expression createKeyword      (std::string s) { return fromSection ({ SourceCodeModel::Expression::Section::Type::keyword,    std::move (s) }); }
    static SourceCodeModel::Expression createText         (std::string s) { return fromSection ({ SourceCodeModel::Expression::Section::Type::text,       std::move (s) }); }
    static SourceCodeModel::Expression createPrimitive    (std::string s) { return fromSection ({ SourceCodeModel::Expression::Section::Type::primitive,  std::move (s) }); }

    static SourceCodeModel::Expression createStruct (std::string s, std::string uid)
    {
        return fromSection ({ SourceCodeModel::Expression::Section::Type::structure, std::move (s), std::move (uid) });
    }

    static SourceCodeModel::Expression createStruct (Structure& s)
    {
        if (auto structDecl = reinterpret_cast<AST::StructDeclaration*> (s.backlinkToASTObject))
            return createStruct (s.getName(), makeUID (*structDecl));

        return createStruct (s.getName(), {});
    }

    static std::string resolvePartialNameAsUID (AST::ModuleBase& module, const IdentifierPath& partialName)
    {
        AST::Scope::NameSearch search;
        search.partiallyQualifiedPath = partialName;
        search.stopAtFirstScopeWithResults = true;
        search.findVariables = true;
        search.findTypes = true;
        search.findFunctions = true;
        search.findNamespaces = true;
        search.findProcessors = true;
        search.findProcessorInstances = false;
        search.findEndpoints = true;

        module.performFullNameSearch (search, nullptr);

        if (search.itemsFound.size() != 0)
        {
            auto item = search.itemsFound.front();

            if (auto mb = cast<AST::ModuleBase> (item))            return makeUID (*mb);
            if (auto t = cast<AST::TypeDeclarationBase> (item))    return makeUID (*t);
            if (auto v = cast<AST::VariableDeclaration> (item))    return makeUID (*v);
            if (auto e = cast<AST::EndpointDeclaration> (item))    return makeUID (*e);
            if (auto f = cast<AST::Function> (item))               return makeUID (*f);
        }

        return {};
    }
};

//==============================================================================
static std::string getInitialiserValue (CodeLocation name)
{
    CodeLocationRange range;
    range.start = SourceCodeUtilities::findNextOccurrence (name, '=');
    SOUL_ASSERT (! range.start.isEmpty());
    ++(range.start.location);
    range.end = SourceCodeUtilities::findEndOfExpression (range.start);
    return choc::text::trim (range.toString());
}

static std::string getInitialiserValue (AST::VariableDeclaration& v)
{
    if (v.initialValue == nullptr)
        return {};

    return getInitialiserValue (v.context.location);
}

static SourceCodeModel::Annotation createAnnotation (const AST::Annotation& a, const StringDictionary& dictionary)
{
    SourceCodeModel::Annotation result;

    for (auto& p : a.properties)
        result.properties[p.name->toString()] = ExpressionHelpers::create (p.value, dictionary);

    return result;
}

static void buildSpecialisationParams (AST::ModuleBase& module, SourceCodeModel::Module& m, const StringDictionary& dictionary)
{
    for (auto& p : module.getSpecialisationParameters())
    {
        SourceCodeModel::SpecialisationParameter desc;

        if (auto u = cast<AST::UsingDeclaration> (p))
        {
            desc.type = ExpressionHelpers::createKeyword ("using");
            desc.name = u->name.toString();
            desc.UID = makeUID (*u);

            if (u->targetType != nullptr)
                desc.defaultValue = getInitialiserValue (u->context.location);
        }
        else if (auto pa = cast<AST::ProcessorAliasDeclaration> (p))
        {
            desc.type = ExpressionHelpers::createKeyword ("processor");
            desc.name = pa->name.toString();

            if (pa->targetProcessor != nullptr)
                desc.defaultValue = getInitialiserValue (pa->context.location);
        }
        else if (auto na = cast<AST::NamespaceAliasDeclaration> (p))
        {
            desc.type = ExpressionHelpers::createKeyword ("namespace");
            desc.name = na->name.toString();

            if (na->targetNamespace != nullptr)
                desc.defaultValue = getInitialiserValue (na->context.location);
        }
        else if (auto v = cast<AST::VariableDeclaration> (p))
        {
            desc.type = ExpressionHelpers::forVariable (*v, dictionary);
            desc.name = v->name.toString();
            desc.UID = makeUID (*v);
            desc.defaultValue = getInitialiserValue (*v);
            desc.annotation = createAnnotation (v->annotation, dictionary);
        }
        else
        {
            SOUL_ASSERT_FALSE;
        }

        m.specialisationParams.push_back (std::move (desc));
    }
}

static void buildEndpoints (AST::ModuleBase& module, SourceCodeModel::Module& m, const StringDictionary& dictionary)
{
    for (auto& e : module.getEndpoints())
    {
        SourceCodeModel::Endpoint desc;
        desc.comment = getComment (e->context);

        desc.name = e->name.toString();
        desc.UID = makeUID (e);
        desc.annotation = createAnnotation (e->annotation, dictionary);

        if (e->details != nullptr)
        {
            desc.endpointType = endpointTypeToString (e->details->endpointType);

            for (auto& type : e->details->dataTypes)
                desc.dataTypes.push_back (ExpressionHelpers::create (type, dictionary));
        }
        
        if (e->isInput)
            m.inputs.push_back (std::move (desc));
        else
            m.outputs.push_back (std::move (desc));
    }
}

static void buildFunctions (AST::ModuleBase& module, SourceCodeModel::Module& m, const StringDictionary& dictionary)
{
    if (auto functions = module.getFunctionList())
    {
        for (auto& f : *functions)
        {
            if (shouldShow (f))
            {
                SourceCodeModel::Function desc;
                desc.comment = getComment (f->context);
                desc.bareName = f->name.toString();
                desc.fullyQualifiedName = TokenisedPathString::join (m.fullyQualifiedName, desc.bareName);
                desc.UID = makeUID (f);

                auto openParen = SourceCodeUtilities::findNextOccurrence (f->nameLocation.location, '(');
                SOUL_ASSERT (! openParen.isEmpty());

                CodeLocationRange nameWithGenerics { f->nameLocation.location, openParen };
                desc.nameWithGenerics = simplifyWhitespace (nameWithGenerics.toString());

                if (auto ret = f->returnType.get())
                    desc.returnType = ExpressionHelpers::create (*ret, dictionary);

                for (auto& p : f->parameters)
                {
                    SourceCodeModel::Variable param;
                    param.comment = getComment (p->context);
                    param.name = p->name.toString();
                    param.UID = makeUID (p);
                    param.type = ExpressionHelpers::forVariable (p, dictionary);
                    param.initialiser = getInitialiserValue (p);

                    desc.parameters.push_back (std::move (param));
                }

                desc.annotation = createAnnotation (f->annotation, dictionary);

                m.functions.push_back (std::move (desc));
            }
        }
    }
}

static void buildStructs (AST::ModuleBase& module, SourceCodeModel::Module& m, const StringDictionary& dictionary)
{
    for (auto& s : module.getStructDeclarations())
    {
        if (shouldShow (s))
        {
            SourceCodeModel::Struct desc;
            desc.comment = getComment (s->context);
            desc.shortName = s->name.toString();
            desc.fullName = TokenisedPathString::join (m.fullyQualifiedName, desc.shortName);
            desc.UID = makeUID (s);

            for (auto& sm : s->getMembers())
            {
                SourceCodeModel::Struct::Member member;
                member.name = sm.name.toString();
                member.comment = getComment (sm.nameLocation);
                member.type = ExpressionHelpers::create (sm.type, dictionary);

                desc.members.push_back (std::move (member));
            }

            m.structs.push_back (std::move (desc));
        }
    }
}

static void buildVariables (AST::ModuleBase& module, SourceCodeModel::Module& m, const StringDictionary& dictionary)
{
    for (auto& v : module.getStateVariableList())
    {
        if (shouldShow (v))
        {
            SourceCodeModel::Variable desc;
            desc.comment = getComment (v->context);
            desc.name = v->name.toString();
            desc.UID = makeUID (v);
            desc.isExternal = v->isExternal;
            desc.type = ExpressionHelpers::forVariable (v, dictionary);
            desc.initialiser = getInitialiserValue (v);

            m.variables.push_back (std::move (desc));
        }
    }
}

static void buildProcessorInstances (AST::ModuleBase& module, SourceCodeModel::Module& m, const StringDictionary& dictionary)
{
    for (auto& i : module.getProcessorInstances())
    {
        if (! i->isImplicitlyCreated())
        {
            SourceCodeModel::ProcessorInstance desc;

            if (i->instanceName != nullptr)
                desc.UID = makeUID (i);

            desc.name = i->instanceName->toString();
            desc.targetProcessor      = ExpressionHelpers::createIfNotNull (i->targetProcessor, dictionary);
            desc.specialisationArgs   = ExpressionHelpers::createIfNotNull (i->specialisationArgs, dictionary);
            desc.clockMultiplierRatio = ExpressionHelpers::createIfNotNull (i->clockMultiplierRatio, dictionary);
            desc.clockDividerRatio    = ExpressionHelpers::createIfNotNull (i->clockDividerRatio, dictionary);
            desc.arraySize            = ExpressionHelpers::createIfNotNull (i->arraySize, dictionary);

            m.processorInstances.push_back (std::move (desc));
        }
    }
}

static void buildConnections (AST::ModuleBase& module, SourceCodeModel::Module& m, const StringDictionary& dictionary)
{
    if (auto graph = cast<AST::Graph> (module))
    {
        for (auto& c : graph->connections)
        {
            SourceCodeModel::Connection desc;
            desc.sourceEndpoint    = ExpressionHelpers::createIfNotNull (c->source.endpoint, dictionary);
            desc.destEndpoint      = ExpressionHelpers::createIfNotNull (c->dest.endpoint, dictionary);
            desc.interpolationType = getInterpolationDescription (c->interpolationType);
            desc.delayLength       = ExpressionHelpers::createIfNotNull (c->delayLength, dictionary);

            m.connections.push_back (std::move (desc));
        }
    }
}

//==============================================================================
static SourceCodeModel::Module createModule (AST::ModuleBase& m, const StringDictionary& dictionary)
{
    SourceCodeModel::Module d;

    d.UID = makeUID (m);
    d.isNamespace = m.isNamespace();
    d.isProcessor = m.isProcessor();
    d.isGraph = m.isGraph();
    d.moduleTypeDescription = d.isNamespace ? "namespace" : (d.isGraph ? "graph" : "processor");
    d.fullyQualifiedName = Program::stripRootNamespaceFromQualifiedPath (m.getFullyQualifiedDisplayPath().toString());
    d.comment = SourceCodeUtilities::parseComment (SourceCodeUtilities::findStartOfPrecedingComment (m.processorKeywordLocation));

    if (auto p = cast<AST::ProcessorBase> (m))
        d.annotation = createAnnotation (p->annotation, dictionary);

    return d;
}

static void recurseFindingModules (AST::ModuleBase& m, SourceCodeModel::File& desc, const StringDictionary& dictionary)
{
    if (m.originalModule != nullptr)
        return;

    // if there's no keyword then it's an outer namespace that was parsed indirectly
    if (! m.processorKeywordLocation.isEmpty())
    {
        auto module = createModule (m, dictionary);

        if (shouldShow (m, module))
        {
            desc.modules.push_back (std::move (module));
            auto& newModule = desc.modules.back();

            buildSpecialisationParams (m, newModule, dictionary);
            buildEndpoints (m, newModule, dictionary);
            buildFunctions (m, newModule, dictionary);
            buildVariables (m, newModule, dictionary);
            buildStructs (m, newModule, dictionary);
            buildProcessorInstances (m, newModule, dictionary);
            buildConnections (m, newModule, dictionary);
        }
    }

    for (auto& sub : m.getSubModules())
        recurseFindingModules (sub, desc, dictionary);
}

bool SourceCodeModel::rebuild (CompileMessageList& errors, ArrayView<SourceCodeText::Ptr> filesToLoad)
{
    files.clear();
    AST::Allocator allocator;
    auto& topLevelNamespace = AST::createRootNamespace (allocator);

    files.resize (filesToLoad.size());

    for (size_t i = 0; i < filesToLoad.size(); ++i)
    {
        auto& f = filesToLoad[i];
        auto& desc = files[i];

        try
        {
            CompileMessageHandler handler (errors);

            for (auto& m : Compiler::parseTopLevelDeclarations (allocator, f, topLevelNamespace))
            {
                ASTUtilities::mergeDuplicateNamespaces (topLevelNamespace);
                recurseFindingModules (m, desc, allocator.stringDictionary);
            }
        }
        catch (AbortCompilationException) {}

        if (errors.hasErrors())
            return false;

        desc.source = f;
        desc.filename = f->filename;
        desc.UID = makeUID ("lib_" + choc::text::replace (desc.filename, ".soul", ""));
        desc.fileComment = SourceCodeUtilities::getFileSummaryComment (f);
        desc.title = SourceCodeUtilities::getFileSummaryTitle (desc.fileComment);
        desc.summary = SourceCodeUtilities::getFileSummaryBody (desc.fileComment);

        if (desc.title.empty())
            desc.title = desc.filename;
    }

    return true;
}

//==============================================================================
static SourceCodeModel::TableOfContentsNode& findOrCreateNode (SourceCodeModel::TableOfContentsNode& node,
                                                               ArrayView<std::string> path)
{
    if (path.empty())
        return node;

    auto& firstPart = path.front();

    if (path.size() == 1 && firstPart == node.name)
        return node;

    for (auto& c : node.children)
        if (firstPart == c.name)
            return findOrCreateNode (c, path.tail());

    node.children.push_back ({});
    auto& n = node.children.back();
    n.name = firstPart;
    return path.size() > 1 ? findOrCreateNode (n, path.tail()) : n;
}

SourceCodeModel::TableOfContentsNode SourceCodeModel::createTableOfContentsRoot() const
{
    TableOfContentsNode root;

    for (auto& f : files)
    {
        std::vector<std::string> filePath { f.title };
        findOrCreateNode (root, filePath).file = std::addressof (f);

        for (auto& m : f.modules)
        {
            TokenisedPathString path (m.fullyQualifiedName);
            auto modulePath = filePath;

            if (path.sections.size() > 1 && path.getSection(0) == "soul")
            {
                modulePath.push_back ("soul::" + path.getSection (1));
                path.sections.erase (path.sections.begin(), path.sections.begin() + 2);
            }

            for (size_t i = 0; i < path.sections.size(); ++i)
                modulePath.push_back (path.getSection (i));

            findOrCreateNode (root, modulePath).module = std::addressof (m);
        }
    }

    return root;
}

} // namespace soul
