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
    return comment.isDoxygenStyle || ! comment.getText().empty();
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

static bool shouldShow (AST::ModuleBase& module, const SourceCodeModel::ModuleDesc& m)
{
    if (m.isProcessor)
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
    static SourceCodeModel::Expression create (AST::Expression& e)
    {
        if (auto s = cast<AST::SubscriptWithBrackets> (e))  return create (s->lhs) + createText ("[") + createIfNotNull (s->rhs) + createText ("]");
        if (auto s = cast<AST::SubscriptWithChevrons> (e))  return create (s->lhs) + createText ("<") + createIfNotNull (s->rhs) + createText (">");
        if (auto d = cast<AST::DotOperator> (e))            return create (d->lhs) + createText (".") + createText (d->rhs.identifier.toString());
        if (auto q = cast<AST::QualifiedIdentifier> (e))    return fromIdentifier (*q);
        if (auto c = cast<AST::Constant> (e))               return createText (c->value.getDescription());

        if (auto m = cast<AST::TypeMetaFunction> (e))
        {
            if (m->operation == AST::TypeMetaFunction::Op::makeReference)
                return create (m->source) + createText ("&");

            if (m->operation == AST::TypeMetaFunction::Op::makeConst)
                return createKeyword ("const ") + create (m->source);

            return create (m->source) + createText (".") + createText (AST::TypeMetaFunction::getNameForOperation (m->operation));
        }

        return create (e.resolveAsType());
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

    static SourceCodeModel::Expression forVariable (AST::VariableDeclaration& v)
    {
        if (v.declaredType != nullptr)
            return create (*v.declaredType);

        SOUL_ASSERT (v.initialValue != nullptr);

        if (v.initialValue->isResolved())
            return create (v.initialValue->getResultType());

        if (auto cc = cast<AST::CallOrCast> (v.initialValue))
            return create (cc->nameOrType);

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

    static SourceCodeModel::Expression createIfNotNull (pool_ptr<AST::Expression> e)    { return e != nullptr ? create (*e) : SourceCodeModel::Expression(); }

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
    auto equalsOp = SourceCodeUtilities::findNextOccurrence (name, '=');
    SOUL_ASSERT (! equalsOp.isEmpty());
    ++(equalsOp.location);

    auto endOfStatement = SourceCodeUtilities::findEndOfExpression (equalsOp);
    SOUL_ASSERT (! endOfStatement.isEmpty());

    return SourceCodeUtilities::getStringBetween (equalsOp, endOfStatement);
}

static std::string getInitialiserValue (AST::VariableDeclaration& v)
{
    if (v.initialValue == nullptr)
        return {};

    return getInitialiserValue (v.context.location);
}

static void buildSpecialisationParams (AST::ModuleBase& module, SourceCodeModel::ModuleDesc& m)
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
            desc.type = ExpressionHelpers::forVariable (*v);
            desc.name = v->name.toString();
            desc.UID = makeUID (*v);
            desc.defaultValue = getInitialiserValue (*v);
        }
        else
        {
            SOUL_ASSERT_FALSE;
        }

        m.specialisationParams.push_back (std::move (desc));
    }
}

static void buildEndpoints (AST::ModuleBase& module, SourceCodeModel::ModuleDesc& m)
{
    for (auto& e : module.getEndpoints())
    {
        SourceCodeModel::Endpoint desc;
        desc.comment = getComment (e->context);
        desc.endpointType = endpointTypeToString (e->details->endpointType);
        desc.name = e->name.toString();
        desc.UID = makeUID (e);

        for (auto& type : e->details->dataTypes)
            desc.dataTypes.push_back (ExpressionHelpers::create (type));

        if (e->isInput)
            m.inputs.push_back (std::move (desc));
        else
            m.outputs.push_back (std::move (desc));
    }
}

static void buildFunctions (AST::ModuleBase& module, SourceCodeModel::ModuleDesc& m)
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

                desc.nameWithGenerics = simplifyWhitespace (SourceCodeUtilities::getStringBetween (f->nameLocation.location, openParen));

                if (auto ret = f->returnType.get())
                    desc.returnType = ExpressionHelpers::create (*ret);

                for (auto& p : f->parameters)
                {
                    SourceCodeModel::Variable param;
                    param.comment = getComment (p->context);
                    param.name = p->name.toString();
                    param.UID = makeUID (p);
                    param.type = ExpressionHelpers::forVariable (p);
                    param.initialiser = getInitialiserValue (p);

                    desc.parameters.push_back (std::move (param));
                }

                m.functions.push_back (std::move (desc));
            }
        }
    }
}

static void buildStructs (AST::ModuleBase& module, SourceCodeModel::ModuleDesc& m)
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
                member.type = ExpressionHelpers::create (sm.type);

                desc.members.push_back (std::move (member));
            }

            m.structs.push_back (std::move (desc));
        }
    }
}

static void buildVariables (AST::ModuleBase& module, SourceCodeModel::ModuleDesc& m)
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
            desc.type = ExpressionHelpers::forVariable (v);
            desc.initialiser = getInitialiserValue (v);

            m.variables.push_back (std::move (desc));
        }
    }
}

//==============================================================================
SourceCodeModel::TOCNode& SourceCodeModel::TOCNode::getNode (ArrayView<std::string> path)
{
    if (path.empty())
        return *this;

    auto& firstPart = path.front();

    if (path.size() == 1 && firstPart == name)
        return *this;

    for (auto& c : children)
        if (firstPart == c.name)
            return c.getNode (path.tail());

    children.push_back ({});
    auto& n = children.back();
    n.name = firstPart;
    return path.size() > 1 ? n.getNode (path.tail()) : n;
}

static void buildTOCNodes (ArrayView<SourceCodeModel::FileDesc> files,
                           SourceCodeModel::TOCNode& topLevelTOCNode)
{
    for (auto& f : files)
    {
        std::vector<std::string> filePath { f.title };
        topLevelTOCNode.getNode (filePath).file = std::addressof (f);

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

            topLevelTOCNode.getNode (modulePath).module = std::addressof (m);
        }
    }
}

//==============================================================================
static SourceCodeModel::ModuleDesc createModule (AST::ModuleBase& m)
{
    SourceCodeModel::ModuleDesc d;

    d.UID = makeUID (m);
    d.isNamespace = m.isNamespace();
    d.isProcessor = m.isProcessor();
    d.isGraph = m.isGraph();
    d.moduleTypeDescription = d.isNamespace ? "namespace" : (d.isGraph ? "graph" : "processor");
    d.fullyQualifiedName = Program::stripRootNamespaceFromQualifiedPath (m.getFullyQualifiedDisplayPath().toString());
    d.comment = SourceCodeUtilities::parseComment (SourceCodeUtilities::findStartOfPrecedingComment (m.processorKeywordLocation));

    return d;
}

static void recurseFindingModules (AST::ModuleBase& m, SourceCodeModel::FileDesc& desc)
{
    if (m.originalModule != nullptr)
        return;

    // if there's no keyword then it's an outer namespace that was parsed indirectly
    if (! m.processorKeywordLocation.isEmpty())
    {
        auto module = createModule (m);

        if (shouldShow (m, module))
        {
            desc.modules.push_back (std::move (module));

            buildSpecialisationParams (m, desc.modules.back());
            buildEndpoints (m, desc.modules.back());
            buildFunctions (m, desc.modules.back());
            buildVariables (m, desc.modules.back());
            buildStructs (m, desc.modules.back());
        }
    }

    for (auto& sub : m.getSubModules())
        recurseFindingModules (sub, desc);
}

bool SourceCodeModel::generate (CompileMessageList& errors, ArrayView<SourceCodeText::Ptr> filesToLoad)
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
                recurseFindingModules (m, desc);
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
    }

    buildTOCNodes (files, topLevelTOCNode);
    return true;
}

} // namespace soul
