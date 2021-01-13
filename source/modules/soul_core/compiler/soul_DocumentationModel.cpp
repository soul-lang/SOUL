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

void DocumentationModel::generate (std::string sourceFilename)
{
    filename = sourceFilename;
    title = source.getFileSummaryTitle();
    summary = source.getFileSummaryBody();

    if (title.empty())
        title = filename;

    for (auto& m : source.getAllModules())
        addModule (m);

    buildEndpoints();
    buildFunctions();
    buildVariables();
    buildStructs();
    buildTOCNodes();
}

DocumentationModel::TOCNode& DocumentationModel::TOCNode::getNode (const std::string& fullPath, std::string pathNeeded)
{
    if (pathNeeded == name)
        return *this;

    auto thisPath = name;

    if (! thisPath.empty())
    {
        thisPath += "::";
        SOUL_ASSERT (choc::text::startsWith (pathNeeded, thisPath));
        pathNeeded = pathNeeded.substr (thisPath.length());
    }

    TokenisedPathString remainder (pathNeeded);
    SOUL_ASSERT (! remainder.sections.empty());
    auto firstSection = remainder.getSection (0);

    for (auto& g : children)
        if (g.name == firstSection)
            return g.getNode (fullPath, pathNeeded);

    children.push_back ({ this, firstSection, nullptr, {} });
    return children.back().getNode (fullPath, pathNeeded);
}

void DocumentationModel::TOCNode::coalesceSingleItems()
{
    for (auto& c : children)
    {
        c.coalesceSingleItems();

        if (c.children.size() == 1)
        {
            auto child = c.children.front();
            c.name += "::" + child.name;
            c.children = std::move (child.children);
            c.module = child.module;
        }
    }
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

    if (module.module.isNamespace())
        return false;

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

void DocumentationModel::addModule (const SourceCodeOperations::ModuleDeclaration& module)
{
    if (shouldShow (module))
        allModules.push_back ({ module, module.getType(), module.getFullyQualifiedName() });
}

void DocumentationModel::buildTOCNodes()
{
    for (auto& m : allModules)
    {
        auto fullPath = Program::stripRootNamespaceFromQualifiedPath (m.module.module.getFullyQualifiedDisplayPath().toString());
        topLevelTOCNode.getNode (fullPath, fullPath).module = std::addressof (m);
    }

    topLevelTOCNode.coalesceSingleItems();
}

void DocumentationModel::buildEndpoints()
{
    for (auto& m : allModules)
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

void DocumentationModel::buildFunctions()
{
    for (auto& m : allModules)
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

void DocumentationModel::buildStructs()
{
    for (auto& module : allModules)
    {
        for (auto& s : module.module.module.getStructDeclarations())
        {
            if (shouldShow (s))
            {
                StructDesc desc;
                desc.comment = getComment (s->context);
                desc.name = s->name.toString();

                for (auto& m : s->getMembers())
                {
                    StructDesc::Member member;
                    member.name = m.name.toString();
                    member.comment = getComment (m.nameLocation);
                    member.type = SourceCodeOperations::getStringForType (m.type);

                    desc.members.push_back (std::move (member));
                }

                module.structs.push_back (std::move (desc));
            }
        }
    }
}

void DocumentationModel::buildVariables()
{
    for (auto& m : allModules)
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

} // namespace soul
