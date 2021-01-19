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

static DocumentationModel::ModuleDesc createModule (AST::ModuleBase& m, AST::Allocator& allocator)
{
    DocumentationModel::ModuleDesc d { m, allocator };

    d.typeOfModule = m.isNamespace() ? "namespace"
                                     : (m.isGraph() ? "graph" : "processor");

    d.fullyQualifiedName = Program::stripRootNamespaceFromQualifiedPath (m.getFullyQualifiedDisplayPath().toString());
    d.comment = SourceCodeOperations::parseComment (SourceCodeOperations::findStartOfPrecedingComment (m.processorKeywordLocation));

    return d;
}

static void recurseFindingModules (AST::ModuleBase& m, AST::Allocator& allocator,
                                   std::vector<DocumentationModel::ModuleDesc>& results)
{
    if (m.originalModule != nullptr)
        return;

    // if there's no keyword then it's an outer namespace that was parsed indirectly
    if (! m.processorKeywordLocation.isEmpty())
        results.push_back (createModule (m, allocator));

    for (auto& sub : m.getSubModules())
        recurseFindingModules (sub, allocator, results);
}


bool DocumentationModel::generate (CompileMessageList& errors, ArrayView<SourceCodeText::Ptr> filesToLoad)
{
    files.clear();
    allocator.clear();
    topLevelNamespace = AST::createRootNamespace (allocator);

    files.resize (filesToLoad.size());

    for (size_t i = 0; i < filesToLoad.size(); ++i)
    {
        auto& f = filesToLoad[i];
        auto& desc = files[i];

        try
        {
            CompileMessageHandler handler (errors);

            for (auto& m : Compiler::parseTopLevelDeclarations (allocator, f, *topLevelNamespace))
            {
                ASTUtilities::mergeDuplicateNamespaces (*topLevelNamespace);
                recurseFindingModules (m, allocator, desc.modules);
            }
        }
        catch (AbortCompilationException) {}

        if (errors.hasErrors())
            return false;

        desc.source = f;
        desc.filename = f->filename;

        auto summary = SourceCodeOperations::getFileSummaryComment (f);
        desc.title = SourceCodeOperations::getFileSummaryTitle (summary);
        desc.summary = SourceCodeOperations::getFileSummaryBody (summary);
    }

    buildSpecialisationParams();
    buildEndpoints();
    buildFunctions();
    buildVariables();
    buildStructs();
    buildTOCNodes();
    return true;
}

static DocumentationModel::TypeDesc operator+ (DocumentationModel::TypeDesc a, DocumentationModel::TypeDesc&& b)
{
    a.sections.reserve (a.sections.size() + b.sections.size());

    for (auto& s : b.sections)
        a.sections.push_back (std::move (s));

    return a;
}

struct TypeDescHelpers
{
    static DocumentationModel::TypeDesc create (AST::Expression& e)
    {
        if (auto s = cast<AST::SubscriptWithBrackets> (e))  return create (s->lhs) + createText ("[") + createIfNotNull (s->rhs) + createText ("]");
        if (auto s = cast<AST::SubscriptWithChevrons> (e))  return create (s->lhs) + createText ("<") + createIfNotNull (s->rhs) + createText (">");
        if (auto d = cast<AST::DotOperator> (e))            return create (d->lhs) + createText (".") + createText (d->rhs.identifier.toString());
        if (auto q = cast<AST::QualifiedIdentifier> (e))    return createStruct (q->toString());
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

    static DocumentationModel::TypeDesc create (const Type& t)
    {
        if (t.isConst())          return createKeyword ("const ") + create (t.removeConst());
        if (t.isReference())      return create (t.removeReference()) + createText ("&");
        if (t.isVector())         return create (t.getPrimitiveType()) + createText ("<" + std::to_string (t.getVectorSize()) + ">");
        if (t.isUnsizedArray())   return create (t.getArrayElementType()) + createText ("[]");
        if (t.isArray())          return create (t.getArrayElementType()) + createText ("[" + std::to_string (t.getArraySize()) + "]");
        if (t.isWrapped())        return createKeyword ("wrap") + createText ("<" + std::to_string (t.getBoundedIntLimit()) + ">");
        if (t.isClamped())        return createKeyword ("clamp") + createText ("<" + std::to_string (t.getBoundedIntLimit()) + ">");
        if (t.isStruct())         return createStruct (t.getStructRef().getName());
        if (t.isStringLiteral())  return createPrimitive ("string");

        return createPrimitive (t.getPrimitiveType().getDescription());
    }

    static DocumentationModel::TypeDesc forVariable (AST::VariableDeclaration& v)
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

    static DocumentationModel::TypeDesc fromSection (DocumentationModel::TypeDesc::Section&& s)
    {
        DocumentationModel::TypeDesc d;
        d.sections.push_back (std::move (s));
        return d;
    }

    static DocumentationModel::TypeDesc createIfNotNull (pool_ptr<AST::Expression> e)    { return e != nullptr ? create (*e) : DocumentationModel::TypeDesc(); }

    static DocumentationModel::TypeDesc createKeyword      (std::string s) { return fromSection ({ DocumentationModel::TypeDesc::Section::Type::keyword,    std::move (s) }); }
    static DocumentationModel::TypeDesc createText         (std::string s) { return fromSection ({ DocumentationModel::TypeDesc::Section::Type::text,       std::move (s) }); }
    static DocumentationModel::TypeDesc createPrimitive    (std::string s) { return fromSection ({ DocumentationModel::TypeDesc::Section::Type::primitive,  std::move (s) }); }
    static DocumentationModel::TypeDesc createStruct       (std::string s) { return fromSection ({ DocumentationModel::TypeDesc::Section::Type::structure,  std::move (s) }); }
};

std::string DocumentationModel::TypeDesc::toString() const
{
    std::string result;

    for (auto& s : sections)
        result += s.text;

    return result;
}

std::string DocumentationModel::ModuleDesc::resolvePartialTypename (const std::string& partialName) const
{
    AST::Scope::NameSearch search;
    search.partiallyQualifiedPath = IdentifierPath::fromString (allocator.identifiers, partialName);
    search.stopAtFirstScopeWithResults = true;
    search.findVariables = false;
    search.findTypes = true;
    search.findFunctions = false;
    search.findNamespaces = true;
    search.findProcessors = true;
    search.findProcessorInstances = false;
    search.findEndpoints = false;

    module.performFullNameSearch (search, nullptr);

    if (search.itemsFound.size() != 0)
    {
        auto item = search.itemsFound.front();
        IdentifierPath path;

        if (auto n = cast<AST::ModuleBase> (item))
        {
            path = n->getFullyQualifiedPath();
        }
        else if (auto t = cast<AST::TypeDeclarationBase> (item))
        {
            if (auto p = t->getParentScope())
                path = IdentifierPath (p->getFullyQualifiedPath(), t->name);
            else
                path = IdentifierPath (t->name);
        }

        return Program::stripRootNamespaceFromQualifiedPath (path.toString());
    }

    return {};
}

DocumentationModel::TOCNode& DocumentationModel::TOCNode::getNode (ArrayView<std::string> path)
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

bool DocumentationModel::shouldIncludeComment (const SourceCodeOperations::Comment& comment)
{
    return comment.isDoxygenStyle || ! comment.getText().empty();
}

SourceCodeOperations::Comment DocumentationModel::getComment (const AST::Context& context)
{
    return SourceCodeOperations::parseComment (SourceCodeOperations::findStartOfPrecedingComment (context.location.getStartOfLine()));
}

bool DocumentationModel::shouldShow (const AST::Function& f)
{
    return shouldIncludeComment (getComment (f.context));
}

bool DocumentationModel::shouldShow (const AST::VariableDeclaration& v)
{
    return ! v.isSpecialisation;
}

bool DocumentationModel::shouldShow (const AST::StructDeclaration&)
{
    return true; // TODO
}

bool DocumentationModel::shouldShow (const SourceCodeOperations::ModuleDeclaration& module)
{
    if (module.module.isProcessor())
        return true;

    if (shouldIncludeComment (module.getComment()))
        return true;

    if (auto functions = module.module.getFunctionList())
        for (auto& f : *functions)
            if (shouldShow (f))
                return true;

    for (auto& v : module.module.getStateVariableList())
        if (shouldShow (v))
            return true;

    for (auto& s : module.module.getStructDeclarations())
        if (shouldShow (s))
            return true;

    return false;
}

//==============================================================================
std::string DocumentationModel::getStringBetween (CodeLocation start, CodeLocation end)
{
    SOUL_ASSERT (end.location.getAddress() >= start.location.getAddress());
    return std::string (start.location.getAddress(), end.location.getAddress());
}

CodeLocation DocumentationModel::findNextOccurrence (CodeLocation start, char character)
{
    for (auto pos = start;; ++(pos.location))
    {
        auto c = *(pos.location);

        if (c == static_cast<decltype(c)> (character))
            return pos;

        if (c == 0)
            return {};
    }
}

CodeLocation DocumentationModel::findEndOfExpression (CodeLocation start)
{
    while (! start.location.isEmpty())
    {
        auto c = *(start.location);

        if (c == ',' || c == ';' || c == ')' || c == '}')
            return start;

        if (c == '(')
            start = SourceCodeOperations::findEndOfMatchingParen (start);
        else if (c == '{')
            start = SourceCodeOperations::findEndOfMatchingBrace (start);
        else
            ++(start.location);
    }

    return {};
}

void DocumentationModel::buildTOCNodes()
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

static std::string getInitialiserValue (CodeLocation name)
{
    auto equalsOp = DocumentationModel::findNextOccurrence (name, '=');
    SOUL_ASSERT (! equalsOp.isEmpty());
    ++(equalsOp.location);

    auto endOfStatement = DocumentationModel::findEndOfExpression (equalsOp);
    SOUL_ASSERT (! endOfStatement.isEmpty());

    return DocumentationModel::getStringBetween (equalsOp, endOfStatement);
}

static std::string getInitialiserValue (AST::VariableDeclaration& v)
{
    if (v.initialValue == nullptr)
        return {};

    return getInitialiserValue (v.context.location);
}

void DocumentationModel::buildSpecialisationParams()
{
    for (auto& f : files)
    {
        for (auto& m : f.modules)
        {
            for (auto& p : m.module.getSpecialisationParameters())
            {
                SpecialisationParamDesc desc;

                if (auto u = cast<AST::UsingDeclaration> (p))
                {
                    desc.type = TypeDescHelpers::createKeyword ("using");
                    desc.name = u->name.toString();

                    if (u->targetType != nullptr)
                        desc.defaultValue = getInitialiserValue (u->context.location);
                }
                else if (auto pa = cast<AST::ProcessorAliasDeclaration> (p))
                {
                    desc.type = TypeDescHelpers::createKeyword ("processor");
                    desc.name = pa->name.toString();

                    if (pa->targetProcessor != nullptr)
                        desc.defaultValue = getInitialiserValue (pa->context.location);
                }
                else if (auto na = cast<AST::NamespaceAliasDeclaration> (p))
                {
                    desc.type = TypeDescHelpers::createKeyword ("namespace");
                    desc.name = na->name.toString();

                    if (na->targetNamespace != nullptr)
                        desc.defaultValue = getInitialiserValue (na->context.location);
                }
                else if (auto v = cast<AST::VariableDeclaration> (p))
                {
                    desc.type = TypeDescHelpers::forVariable (*v);
                    desc.name = v->name.toString();
                    desc.defaultValue = getInitialiserValue (*v);
                }
                else
                {
                    SOUL_ASSERT_FALSE;
                }

                m.specialisationParams.push_back (std::move (desc));
            }
        }
    }
}

void DocumentationModel::buildEndpoints()
{
    for (auto& f : files)
    {
        for (auto& m : f.modules)
        {
            for (auto& e : m.module.getEndpoints())
            {
                EndpointDesc desc;
                desc.comment = getComment (e->context);
                desc.type = endpointTypeToString (e->details->endpointType);
                desc.name = e->name.toString();

                for (auto& type : e->details->dataTypes)
                    desc.dataTypes.push_back (TypeDescHelpers::create (type));

                if (e->isInput)
                    m.inputs.push_back (std::move (desc));
                else
                    m.outputs.push_back (std::move (desc));
            }
        }
    }
}

void DocumentationModel::buildFunctions()
{
    for (auto& file : files)
    {
        for (auto& m : file.modules)
        {
            if (auto functions = m.module.getFunctionList())
            {
                for (auto& f : *functions)
                {
                    if (shouldShow (f))
                    {
                        FunctionDesc desc;
                        desc.comment = getComment (f->context);
                        desc.bareName = f->name.toString();

                        auto openParen = findNextOccurrence (f->nameLocation.location, '(');
                        SOUL_ASSERT (! openParen.isEmpty());

                        desc.nameWithGenerics = simplifyWhitespace (getStringBetween (f->nameLocation.location, openParen));

                        if (auto ret = f->returnType.get())
                            desc.returnType = TypeDescHelpers::create (*ret);

                        for (auto& p : f->parameters)
                        {
                            VariableDesc param;
                            param.comment = getComment (p->context);
                            param.name = p->name.toString();
                            param.type = TypeDescHelpers::forVariable (p);
                            param.initialiser = getInitialiserValue (p);

                            desc.parameters.push_back (std::move (param));
                        }

                        m.functions.push_back (std::move (desc));
                    }
                }
            }
        }
    }
}

void DocumentationModel::buildStructs()
{
    for (auto& f : files)
    {
        for (auto& m : f.modules)
        {
            for (auto& s : m.module.getStructDeclarations())
            {
                if (shouldShow (s))
                {
                    StructDesc desc;
                    desc.comment = getComment (s->context);
                    desc.shortName = s->name.toString();
                    desc.fullName = TokenisedPathString::join (m.fullyQualifiedName, desc.shortName);

                    for (auto& sm : s->getMembers())
                    {
                        StructDesc::Member member;
                        member.name = sm.name.toString();
                        member.comment = getComment (sm.nameLocation);
                        member.type = TypeDescHelpers::create (sm.type);

                        desc.members.push_back (std::move (member));
                    }

                    m.structs.push_back (std::move (desc));
                }
            }
        }
    }
}

void DocumentationModel::buildVariables()
{
    for (auto& f : files)
    {
        for (auto& m : f.modules)
        {
            for (auto& v : m.module.getStateVariableList())
            {
                if (shouldShow (v))
                {
                    VariableDesc desc;
                    desc.comment = getComment (v->context);
                    desc.name = v->name.toString();
                    desc.isExternal = v->isExternal;
                    desc.type = TypeDescHelpers::forVariable (v);
                    desc.initialiser = getInitialiserValue (v);

                    m.variables.push_back (std::move (desc));
                }
            }
        }
    }
}

} // namespace soul
