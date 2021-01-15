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

bool DocumentationModel::generate (CompileMessageList& errors, ArrayView<SourceCodeText::Ptr> filesToLoad)
{
    for (auto& f : filesToLoad)
    {
        FileDesc desc;

        desc.source = std::make_unique<SourceCodeOperations>();

        if (! desc.source->reload (errors, f, {}))
            return false;

        desc.filename = f->filename;
        desc.title = desc.source->getFileSummaryTitle();
        desc.summary = desc.source->getFileSummaryBody();

        if (desc.title.empty())
            desc.title = f->filename;

        for (auto& m : desc.source->getAllModules())
            if (shouldShow (m))
                desc.modules.push_back ({ m, m.getType(), m.getFullyQualifiedName() });

        files.emplace_back (std::move (desc));
    }

    buildEndpoints();
    buildFunctions();
    buildVariables();
    buildStructs();
    buildTOCNodes();
    return true;
}

DocumentationModel::TOCNode& DocumentationModel::TOCNode::getNode (ArrayView<std::string> path)
{
    SOUL_ASSERT (! path.empty());
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

std::string DocumentationModel::createParameterList (std::string line, ArrayView<std::string> items)
{
    if (items.empty())
        return line + "()";

    line += " (";
    auto indent = line.length();
    line += items.front();

    for (size_t i = 1; i < items.size(); ++i)
        line += ",\n" + std::string (indent, ' ') + items[i];

    line += ")";
    return line;
}

void DocumentationModel::buildTOCNodes()
{
    for (auto& f : files)
    {
        std::vector<std::string> filePath { f.title };
        topLevelTOCNode.getNode (filePath).file = std::addressof (f);

        for (auto& m : f.modules)
        {
            auto fullPath = Program::stripRootNamespaceFromQualifiedPath (m.module.module.getFullyQualifiedDisplayPath().toString());
            TokenisedPathString path (fullPath);

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

void DocumentationModel::buildEndpoints()
{
    for (auto& f : files)
    {
        for (auto& m : f.modules)
        {
            for (auto& e : m.module.module.getEndpoints())
            {
                EndpointDesc desc;
                desc.comment = getComment (e->context);
                desc.type = endpointTypeToString (e->details->endpointType);
                desc.name = e->name.toString();

                for (auto& type : e->details->dataTypes)
                    desc.dataTypes.push_back (SourceCodeOperations::getStringForType (type));

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
            if (auto functions = m.module.module.getFunctionList())
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
                        auto closeParen = SourceCodeOperations::findEndOfMatchingParen (openParen);
                        SOUL_ASSERT (! closeParen.isEmpty());

                        desc.nameWithGenerics = simplifyWhitespace (getStringBetween (f->nameLocation.location, openParen));

                        if (auto ret = f->returnType.get())
                            desc.returnType = SourceCodeOperations::getStringForType (*ret);

                        auto params = simplifyWhitespace (getStringBetween (openParen, closeParen));
                        SOUL_ASSERT (params.front() == '(' && params.back() == ')');
                        params = simplifyWhitespace (params.substr (1, params.length() - 2));

                        if (! params.empty())
                        {
                            desc.parameters = choc::text::splitString (params, ',', false);

                            for (auto& p : desc.parameters)
                                p = simplifyWhitespace (p);
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
            for (auto& s : m.module.module.getStructDeclarations())
            {
                if (shouldShow (s))
                {
                    StructDesc desc;
                    desc.comment = getComment (s->context);
                    desc.name = s->name.toString();

                    for (auto& sm : s->getMembers())
                    {
                        StructDesc::Member member;
                        member.name = sm.name.toString();
                        member.comment = getComment (sm.nameLocation);
                        member.type = SourceCodeOperations::getStringForType (sm.type);

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
            for (auto& v : m.module.module.getStateVariableList())
            {
                if (shouldShow (v))
                {
                    VariableDesc desc;
                    desc.comment = getComment (v->context);
                    desc.name = v->name.toString();
                    desc.isConstant = v->isConstant;
                    desc.isExternal = v->isExternal;

                    if (v->declaredType != nullptr)
                        desc.type = SourceCodeOperations::getStringForType (*v->declaredType);
                    else if (v->initialValue != nullptr && v->initialValue->isResolved())
                        desc.type = v->initialValue->getResultType().getDescription();
                    else if (auto cc = cast<AST::CallOrCast> (v->initialValue))
                        desc.type = SourceCodeOperations::getStringForType (cc->nameOrType);

                    m.variables.push_back (std::move (desc));
                }
            }
        }
    }
}

} // namespace soul
