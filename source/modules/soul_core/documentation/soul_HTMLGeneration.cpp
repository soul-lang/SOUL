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
/**
    Builds documentation as HTML from a set of soul files.
*/
struct HTMLGenerator
{
    std::string run (soul::CompileMessageList& errors,
                     const HTMLGenerationOptions& options)
    {
        if (! docs.generate (errors, options.sourceFiles))
            return {};

        auto nav = createNav();

        choc::html::HTMLElement content ("section");
        content.setID ("content");

        for (auto& f : docs.files)
            printLibrary (content, f);

        if (options.templateContent.empty())
        {
            choc::html::HTMLElement root ("html");
            root.setProperty ("lang", "en-US");

            auto& head = root.addChild ("head");
            head.addChild ("title").addContent ("SOUL Documentation");
            head.addChild ("link").setProperty ("rel", "stylesheet")
                                  .setProperty ("href", options.cssFilename);

            auto& body = root.addChild ("body");
            body.addChild (std::move (nav));
            body.addChild (std::move (content));
            return root.toDocument (true);
        }

        std::ostringstream navText, contentText;

        for (auto& c : nav.getChildren())
            c.writeToStream (navText, false);

        for (auto& c : content.getChildren())
            c.writeToStream (contentText, false);

        auto doc = options.templateContent;

        if (replaceTemplatePlaceholder (errors, doc, "$NAVIGATION", navText.str())
             && replaceTemplatePlaceholder (errors, doc, "$CONTENT", contentText.str()))
        {
            return doc;
        }

        return {};
    }

private:
    DocumentationModel docs;

    bool replaceTemplatePlaceholder (soul::CompileMessageList& errors,
                                     std::string& templateCode,
                                     const std::string& placeholder,
                                     const std::string& replacement)
    {
        if (! choc::text::contains (templateCode, placeholder))
        {
            errors.addError ("Template doesn't contain placeholder " + placeholder, {});
            return false;
        }

        templateCode = choc::text::replace (templateCode, placeholder, replacement);
        return true;
    }

    void printLibrary (choc::html::HTMLElement& parent, const DocumentationModel::FileDesc& library)
    {
        auto& libraryDiv = parent.addDiv ("library")
                                 .setID (getLibraryID (library));

        libraryDiv.addChild ("h1").addContent (library.title);

        if (! library.summary.empty())
            addMarkdownAsHTML (libraryDiv.addDiv ("summary"),
                               choc::text::splitIntoLines (library.summary, false));

        for (auto& m : library.modules)
            printModule (libraryDiv, m);
    }

    void printModule (choc::html::HTMLElement& parent, const DocumentationModel::ModuleDesc& m)
    {
        auto& moduleDiv = parent.addDiv ("module")
                              .setID (getModuleID (m));

        auto& title = moduleDiv.addChild ("h2");
        title.addSpan ("module_type").addContent (m.typeOfModule).addContent (" ");
        title.addSpan ("module_name").addContent (m.fullyQualifiedName);

        addComment (moduleDiv, m.comment, "summary");

        auto& sections = moduleDiv.addDiv ("module_sections");
        printSpecialisationParams (sections, m);
        printEndpoints (sections, m);
        printStructs (sections, m);
        printFunctions (sections, m);
        printVariables (sections, m);
    }

    choc::html::HTMLElement createNav()
    {
        choc::html::HTMLElement nav ("nav");
        nav.setID ("contents").setClass ("contents");
        printTOCNode (nav, docs.topLevelTOCNode, true);
        return nav;
    }

    void printTOCNode (choc::html::HTMLElement& parent, const DocumentationModel::TOCNode& node, bool isRoot)
    {
        auto p = &parent;

        if (! isRoot)
        {
            auto& li = parent.addChild ("li");

            if (auto m = node.module)
                li.setClass ("toc_item").addLink ("#" + getModuleID (*m)).addContent (node.name);
            else if (auto f = node.file)
                li.setClass ("toc_module").addLink ("#" + getLibraryID (*f)).addContent (node.name);
            else
                li.setClass ("toc_item").addContent (node.name);

            p = &li;
        }

        if (! node.children.empty())
        {
            auto& ul = p->addChild ("ul").setClass ("toc_item");

            for (auto& n : node.children)
                printTOCNode (ul, n, false);
        }
    }

    static std::string makeID (std::string_view name)
    {
        return retainCharacters (choc::text::replace (name, " ", "_", "::", "_"),
                                 "_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
    }

    static std::string getModuleID (const DocumentationModel::ModuleDesc& m)        { return makeID ("mod_" + choc::text::trim (m.fullyQualifiedName)); }
    static std::string getLibraryID (const DocumentationModel::FileDesc& f)         { return makeID ("lib_" + f.title); }
    static std::string getFunctionID (const DocumentationModel::FunctionDesc& f)    { return makeID ("fn_" + f.fullyQualifiedName); }
    static std::string makeTypeID (std::string_view type)                           { return makeID ("type_" + std::string (type)); }

    std::string findTypeID (const DocumentationModel::ModuleDesc& parent, const std::string& partialName) const
    {
        auto name = parent.resolvePartialTypename (partialName);

        if (! name.empty())
            return makeTypeID (name);

        return {};
    }

    choc::html::HTMLElement& printType (choc::html::HTMLElement& parent, const DocumentationModel::ModuleDesc& module, const DocumentationModel::TypeDesc& type)
    {
        auto getClassForTypeSection = [] (DocumentationModel::TypeDesc::Section::Type t) -> const char*
        {
            if (t == DocumentationModel::TypeDesc::Section::Type::keyword)   return "keyword";
            if (t == DocumentationModel::TypeDesc::Section::Type::structure) return "struct_name";
            if (t == DocumentationModel::TypeDesc::Section::Type::primitive) return "primitive_type";

            SOUL_ASSERT (t == DocumentationModel::TypeDesc::Section::Type::text);
            return "typename_text";
        };

        for (auto& s : type.sections)
        {
            auto classID = getClassForTypeSection (s.type);

            if (s.type == DocumentationModel::TypeDesc::Section::Type::structure)
            {
                auto typeID = findTypeID (module, s.text);

                if (! typeID.empty())
                {
                    parent.addLink ("#" + typeID).setClass (classID).addContent (s.text);
                    continue;
                }
            }

            parent.addSpan (classID).addContent (s.text);
        }

        return parent;
    }

    choc::html::HTMLElement& createModuleSection (choc::html::HTMLElement& parent, std::string_view name)
    {
        auto& list = parent.addDiv ("module_section");
        list.addChild ("h3").addContent (name);
        return list;
    }

    void printSpecialisationParams (choc::html::HTMLElement& parent, const DocumentationModel::ModuleDesc& m)
    {
        if (! m.specialisationParams.empty())
        {
            auto& section = createModuleSection (parent, "Specialisation Parameters");

            auto& desc = section.addParagraph().setClass ("code_block");

            desc.addSpan ("module_type").addContent (m.typeOfModule);
            desc.addContent (" ");
            desc.addSpan ("module_name").addContent (m.fullyQualifiedName);
            desc.addContent (" (");

            bool isFirst = true;
            auto indent = m.typeOfModule.length() + m.fullyQualifiedName.length() + 3;

            for (auto& p : m.specialisationParams)
            {
                if (isFirst)
                    isFirst = false;
                else
                    desc.addContent (",").addLineBreak().addNBSP (indent);

                printType (desc, m, p.type).addContent (" ");

                auto& name = desc.addSpan ("variable_name");
                name.addContent (p.name);

                if (p.type.sections.size() == 1 && p.type.sections.front().text == "using")
                {
                    auto typeID = findTypeID (m, p.name);

                    if (! typeID.empty())
                        name.setID (typeID);
                }

                if (! p.defaultValue.empty())
                    desc.addContent (" = " + choc::text::trim (p.defaultValue));
            }

            desc.addContent (")");
        }
    }

    void printEndpoints (choc::html::HTMLElement& parent, const DocumentationModel::ModuleDesc& m)
    {
        if (! m.inputs.empty())
        {
            auto& ul = createModuleSection (parent, "Inputs").addChild ("ul");

            for (auto& e : m.inputs)
                printEndpoint (ul, m, e);
        }

        if (! m.outputs.empty())
        {
            auto& ul = createModuleSection (parent, "Outputs").addChild ("ul");

            for (auto& e : m.outputs)
                printEndpoint (ul, m, e);
        }
    }

    void printEndpoint (choc::html::HTMLElement& ul, const DocumentationModel::ModuleDesc& parent,
                        const DocumentationModel::EndpointDesc& e)
    {
        auto& li = ul.addChild ("li").setClass ("endpoint_desc");

        addComment (li, e.comment, "summary");

        li.addSpan ("endpoint_type").addContent (e.type);
        li.addNBSP (1);
        li.addSpan ("endpoint_name").addContent (e.name);
        li.addNBSP (1);
        li.addContent ("(");

        bool first = true;

        for (auto& t : e.dataTypes)
        {
            if (first)
                first = false;
            else
                li.addContent (", ");

            printType (li, parent, t);
        }

        li.addContent (")");
    }

    void printStructs (choc::html::HTMLElement& parent, const DocumentationModel::ModuleDesc& module)
    {
        if (! module.structs.empty())
        {
            auto& section = createModuleSection (parent, "Structures");

            for (auto& s : module.structs)
            {
                auto& structDiv = section.addDiv ("struct").setID (makeTypeID (s.fullName));

                addComment (structDiv, s.comment, "summary");

                auto& codeDiv = structDiv.addDiv ("listing");

                auto& start = codeDiv.addParagraph();
                start.addSpan ("keyword").addContent ("struct ");
                start.addSpan ("struct_name").addContent (s.shortName);
                start.addLineBreak().addContent ("{").addLineBreak();

                for (auto& member : s.members)
                {
                    auto& memberDiv = codeDiv.addDiv().setClass ("struct_member");

                    addComment (memberDiv, member.comment, "summary");

                    auto& memberLine = memberDiv.addDiv ("listing");
                    printType (memberLine, module, member.type);
                    memberLine.addContent (" ").addSpan ("member_name").addContent (member.name);
                    memberLine.addContent (";").addLineBreak();
                }

                codeDiv.addParagraph().setClass ("code_block")
                       .addContent ("}");
            }
        }
    }

    void printFunctions (choc::html::HTMLElement& parent, const DocumentationModel::ModuleDesc& m)
    {
        if (! m.functions.empty())
        {
            auto& section = createModuleSection (parent, "Functions");

            for (auto& f : m.functions)
            {
                auto& div = section.addDiv ("function");

                div.addChild ("h3").setClass ("function_name")
                   .setID (getFunctionID (f))
                   .addContent (f.bareName);

                addComment (div, f.comment, "summary");

                auto& proto = div.addParagraph().setClass ("code_block");

                printType (proto, m, f.returnType);
                proto.addContent (" ").addSpan ("function_name").addContent (f.nameWithGenerics);

                if (f.parameters.empty())
                {
                    proto.addContent ("()");
                }
                else
                {
                    proto.addContent (" (");
                    bool isFirst = true;
                    auto indentSpaces = f.returnType.toString().length() + f.nameWithGenerics.length() + 3;

                    for (auto& p : f.parameters)
                    {
                        if (isFirst)
                            isFirst = false;
                        else
                            proto.addContent (",").addLineBreak().addNBSP (indentSpaces);

                        printType (proto, m, p.type);
                        proto.addContent (" ");
                        proto.addSpan ("parameter_name").addContent (p.name);

                        if (! p.initialiser.empty())
                            proto.addContent (" = " + p.initialiser);
                    }

                    proto.addContent (")");
                }
            }
        }
    }

    void printVariables (choc::html::HTMLElement& parent, const DocumentationModel::ModuleDesc& m)
    {
        if (! m.variables.empty())
        {
            auto& section = createModuleSection (parent, "Variables");

            for (auto& v : m.variables)
            {
                auto& div = section.addDiv ("variable");

                auto& name = div.addParagraph().setClass ("code_block");

                if (v.isExternal)
                    name.addSpan ("typename_text").addContent ("external");

                printType (name, m, v.type);
                name.addContent (" ").addSpan ("variable_name").addContent (v.name);

                if (! v.initialiser.empty())
                    name.addContent (" = " + v.initialiser);

                addComment (div, v.comment, "summary");
            }
        }
    }

    static bool isListMarker (const std::string& s)
    {
        return s.length() > 1
            && (s.front() == '-' || s.front() == '*' || s.front() == '*')
            && s[1] == ' ';
    }

    static std::vector<std::string> groupMarkdownIntoParagraphs (const std::vector<std::string>& lines)
    {
        std::vector<std::string> paragraphs;
        paragraphs.push_back ({});
        bool isInCodeSection = false;
        size_t codeSectionIndent = 0;

        for (auto& line : lines)
        {
            auto trimmedLine = choc::text::trim (line);
            auto leadingSpaces = line.length() - trimmedLine.length();

            if (isInCodeSection)
            {
                if (choc::text::startsWith (trimmedLine, "```"))
                {
                    isInCodeSection = false;
                    codeSectionIndent = 0;
                    paragraphs.push_back ({});
                    continue;
                }

                if (leadingSpaces == codeSectionIndent)
                {
                    paragraphs.back() += line + '\n';
                    continue;
                }

                isInCodeSection = false;
                codeSectionIndent = 0;
                paragraphs.push_back (line);
                continue;
            }

            if (trimmedLine.empty())
            {
                paragraphs.push_back ({});
            }
            else if (isListMarker (trimmedLine))
            {
                paragraphs.push_back (line);
            }
            else if (choc::text::startsWith (trimmedLine, "```"))
            {
                isInCodeSection = true;
                codeSectionIndent = 0;
                paragraphs.push_back (trimmedLine + '\n');
            }
            else if (leadingSpaces >= 4 && paragraphs.back().empty())
            {
                isInCodeSection = true;
                codeSectionIndent = leadingSpaces;
                paragraphs.push_back (line + '\n');
            }
            else if (! trimmedLine.empty())
            {
                if (! (paragraphs.back().empty() || line.front() == ' '))
                    paragraphs.back() += ' ';

                paragraphs.back() += line;
            }
        }

        return paragraphs;
    }

    struct TextRange { std::string_view::size_type start = 0, end = 0; };

    static TextRange findLink (std::string_view text)
    {
        for (auto* proto : { "http:", "https:", "file:" })
        {
            auto start = text.find (proto);

            if (start != std::string::npos)
            {
                auto end = text.find (' ', start);

                if (end == std::string_view::npos) end = text.find ('\n', start);
                if (end == std::string_view::npos) end = text.length();

                return { start, end };
            }
        }

        return {};
    }

    static TextRange findBacktickSection (std::string_view text)
    {
        auto start = text.find ('`');

        if (start != std::string_view::npos)
        {
            auto end = text.find ('`', start + 1);

            if (end != std::string_view::npos)
                return { start, end + 1 };
        }

        return {};
    }

    static void appendSpansForContent (choc::html::HTMLElement& parent, std::string_view text)
    {
        auto backticks = findBacktickSection (text);

        if (backticks.end != 0)
        {
            auto part1 = text.substr (0, backticks.start);
            auto part2 = std::string (text.substr (backticks.start + 1, backticks.end - backticks.start - 2));
            auto part3 = text.substr (backticks.end);

            if (! part1.empty())
                appendSpansForContent (parent, part1);

            parent.addChild ("code").addContent (part2);
            appendSpansForContent (parent, part3);
            return;
        }

        auto link = findLink (text);

        if (link.end != 0)
        {
            auto part1 = text.substr (0, link.start);
            auto part2 = std::string (text.substr (link.start, link.end - link.start));
            auto part3 = text.substr (link.end);

            appendSpansForContent (parent, part1);
            parent.addLink (part2).addContent (part2);
            appendSpansForContent (parent, part3);
            return;
        }

        if (! text.empty())
            parent.addContent (text);
    }

    static void addMarkdownAsHTML (choc::html::HTMLElement& parent, const std::vector<std::string>& lines)
    {
        auto paragraphs = groupMarkdownIntoParagraphs (lines);

        struct ListLevel
        {
            choc::html::HTMLElement& ul;
            size_t indent;
        };

        std::vector<ListLevel> listStack;

        auto getParent = [&] () -> choc::html::HTMLElement& { return listStack.empty() ? parent : listStack.back().ul; };

        for (auto& paragraph : paragraphs)
        {
            auto trimmed = choc::text::trimStart (paragraph);
            auto leadingSpaces = paragraph.length() - trimmed.length();

            if (isListMarker (trimmed))
            {
                auto indent = 1 + leadingSpaces;

                while (! listStack.empty() && listStack.back().indent > indent)
                    listStack.pop_back();

                if (listStack.empty() || listStack.back().indent < indent)
                    listStack.push_back ({ getParent().addChild ("ul"), indent });

                appendSpansForContent (listStack.back().ul.addChild ("li"),
                                       choc::text::trimStart (trimmed.substr (2)));
                continue;
            }

            listStack.clear();

            if (choc::text::startsWith (trimmed, "```"))
            {
                auto endOfLine1 = trimmed.find ('\n');

                if (endOfLine1 != std::string::npos)
                {
                    auto& code = getParent().addChild ("code").setClass ("multiline");
                    code.addContent (trimmed.substr (endOfLine1));

                    auto type = choc::text::trim (trimmed.substr (3, endOfLine1 - 3));

                    if (! type.empty())
                        code.setClass (type);
                }

                continue;
            }

            if (leadingSpaces >= 4)
            {
                getParent().addChild ("code").setClass ("multiline").addContent (paragraph);
                continue;
            }

            appendSpansForContent (getParent().addParagraph(), trimmed);
        }
    }

    static void addComment (choc::html::HTMLElement& parent,
                            const SourceCodeOperations::Comment& comment,
                            std::string_view classType)
    {
        if (comment.valid && ! comment.getText().empty())
            addMarkdownAsHTML (parent.addDiv (classType), comment.lines);
    }
};

std::string generateHTMLDocumentation (soul::CompileMessageList& errors,
                                       const HTMLGenerationOptions& options)
{
    HTMLGenerator g;
    return g.run (errors, options);
}

}
