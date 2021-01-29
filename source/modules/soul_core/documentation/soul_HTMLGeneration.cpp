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
    Builds documentation as HTML from a SourceCodeModel
*/
struct HTMLGenerator
{
    std::string run (soul::CompileMessageList& errors,
                     const HTMLGenerationOptions& options)
    {
        if (! model.rebuild (errors, options.sourceFiles))
            return {};

        auto nav = createNav();

        choc::html::HTMLElement content ("section");
        content.setID ("content");

        for (auto& f : model.files)
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
    SourceCodeModel model;

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

    void printLibrary (choc::html::HTMLElement& parent, const SourceCodeModel::File& library)
    {
        auto& libraryDiv = parent.addDiv ("library").setID (library.UID);

        libraryDiv.addChild ("h1").addContent (library.title);

        if (! library.summary.empty())
            addMarkdownAsHTML (libraryDiv.addDiv ("summary"),
                               choc::text::splitIntoLines (library.summary, false));

        for (auto& m : library.modules)
            printModule (libraryDiv, m);
    }

    void printModule (choc::html::HTMLElement& parent, const SourceCodeModel::Module& m)
    {
        auto& moduleDiv = parent.addDiv ("module").setID (m.UID);

        auto& title = moduleDiv.addChild ("h2");
        title.addSpan ("module_type").addContent (m.moduleTypeDescription).addContent (" ");
        title.addSpan ("module_name").addContent (m.fullyQualifiedName);

        addComment (moduleDiv, m.comment, "summary");

        auto& sections = moduleDiv.addDiv ("module_sections");
        printAnnotation (sections, m.annotation);
        printSpecialisationParams (sections, m);
        printEndpoints (sections, m);
        printStructs (sections, m);
        printFunctions (sections, m);
        printVariables (sections, m);
        printProcessorInstances (sections, m);
        printConnections (sections, m);
    }

    choc::html::HTMLElement createNav()
    {
        choc::html::HTMLElement nav ("nav");
        nav.setID ("contents").setClass ("contents");
        printTOCNode (nav, model.createTableOfContentsRoot(), true);
        return nav;
    }

    void printTOCNode (choc::html::HTMLElement& parent, const SourceCodeModel::TableOfContentsNode& node, bool isRoot)
    {
        auto p = &parent;

        if (! isRoot)
        {
            auto& li = parent.addChild ("li");

            if (auto m = node.module)
                li.setClass ("toc_item").addLink ("#" + m->UID).addContent (node.name);
            else if (auto f = node.file)
                li.setClass ("toc_module").addLink ("#" + f->UID).addContent (node.name);
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

    choc::html::HTMLElement& printExpression (choc::html::HTMLElement& parent,
                                              const SourceCodeModel::Expression& e)
    {
        auto getClassForTypeSection = [] (SourceCodeModel::Expression::Section::Type t) -> const char*
        {
            if (t == SourceCodeModel::Expression::Section::Type::keyword)   return "keyword";
            if (t == SourceCodeModel::Expression::Section::Type::structure) return "struct_name";
            if (t == SourceCodeModel::Expression::Section::Type::primitive) return "primitive_type";

            SOUL_ASSERT (t == SourceCodeModel::Expression::Section::Type::text);
            return "typename_text";
        };

        for (auto& s : e.sections)
        {
            auto classID = getClassForTypeSection (s.type);

            if (s.type == SourceCodeModel::Expression::Section::Type::structure)
            {
                if (! s.referencedUID.empty())
                {
                    parent.addLink ("#" + s.referencedUID).setClass (classID).addContent (s.text);
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

    void printAnnotation (choc::html::HTMLElement& parent, const SourceCodeModel::Annotation& a)
    {
        if (! a.properties.empty())
        {
            auto& div = parent.addDiv ("annotation");
            div.addContent ("[[ ");
            bool first = true;

            for (auto& prop : a.properties)
            {
                if (first)
                    first = false;
                else
                    div.addContent (", ");

                div.addSpan ("annotation_name").addContent (prop.first);
                div.addContent (": ");
                auto& value = div.addSpan ("annotation_value").setInline (true);
                printExpression (value, prop.second);
            }

            div.addContent (" ]]");
        }
    }

    void printSpecialisationParams (choc::html::HTMLElement& parent, const SourceCodeModel::Module& m)
    {
        if (! m.specialisationParams.empty())
        {
            auto& section = createModuleSection (parent, "Specialisation Parameters");

            auto& desc = section.addParagraph().setClass ("code_block");

            desc.addSpan ("module_type").addContent (m.moduleTypeDescription);
            desc.addContent (" ");
            desc.addSpan ("module_name").addContent (m.fullyQualifiedName);
            desc.addContent (" (");

            bool isFirst = true;
            auto indent = m.moduleTypeDescription.length() + m.fullyQualifiedName.length() + 3;

            for (auto& p : m.specialisationParams)
            {
                if (isFirst)
                    isFirst = false;
                else
                    desc.addContent (",").addLineBreak().addNBSP (indent);

                printExpression (desc, p.type).addContent (" ");

                auto& name = desc.addSpan ("variable_name");
                name.addContent (p.name);

                if (p.type.sections.size() == 1 && p.type.sections.front().text == "using")
                {
                    if (! p.UID.empty())
                        name.setID (p.UID);
                }

                if (! p.defaultValue.empty())
                    desc.addContent (" = " + choc::text::trim (p.defaultValue));
            }

            desc.addContent (")");
        }
    }

    void printEndpoints (choc::html::HTMLElement& parent, const SourceCodeModel::Module& m)
    {
        if (! m.inputs.empty())
        {
            auto& ul = createModuleSection (parent, "Inputs").addChild ("ul");

            for (auto& e : m.inputs)
                printEndpoint (ul, e);
        }

        if (! m.outputs.empty())
        {
            auto& ul = createModuleSection (parent, "Outputs").addChild ("ul");

            for (auto& e : m.outputs)
                printEndpoint (ul, e);
        }
    }

    void printEndpoint (choc::html::HTMLElement& ul, const SourceCodeModel::Endpoint& e)
    {
        auto& li = ul.addChild ("li").setClass ("endpoint_desc").setID (e.UID);

        addComment (li, e.comment, "summary");

        li.addSpan ("endpoint_type").addContent (e.endpointType);
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

            printExpression (li, t);
        }

        li.addContent (")");
    }

    void printStructs (choc::html::HTMLElement& parent, const SourceCodeModel::Module& module)
    {
        if (! module.structs.empty())
        {
            auto& section = createModuleSection (parent, "Structures");

            for (auto& s : module.structs)
            {
                auto& structDiv = section.addDiv ("struct").setID (s.UID);

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
                    printExpression (memberLine, member.type);
                    memberLine.addContent (" ").addSpan ("member_name").addContent (member.name);
                    memberLine.addContent (";").addLineBreak();
                }

                codeDiv.addParagraph().setClass ("code_block")
                       .addContent ("}");
            }
        }
    }

    void printFunctions (choc::html::HTMLElement& parent, const SourceCodeModel::Module& m)
    {
        if (! m.functions.empty())
        {
            auto& section = createModuleSection (parent, "Functions");

            for (auto& f : m.functions)
            {
                auto& div = section.addDiv ("function");

                div.addChild ("h3").setClass ("function_name")
                   .setID (f.UID)
                   .addContent (f.bareName);

                addComment (div, f.comment, "summary");

                printAnnotation (div, f.annotation);

                auto& proto = div.addParagraph().setClass ("code_block");

                printExpression (proto, f.returnType);
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

                        printExpression (proto, p.type);
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

    void printVariables (choc::html::HTMLElement& parent, const SourceCodeModel::Module& m)
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

                printExpression (name, v.type);
                name.addContent (" ").addSpan ("variable_name").addContent (v.name);

                if (! v.initialiser.empty())
                    name.addContent (" = " + v.initialiser);

                addComment (div, v.comment, "summary");
            }
        }
    }

    void printProcessorInstances (choc::html::HTMLElement& parent, const SourceCodeModel::Module& m)
    {
        if (! m.processorInstances.empty())
        {
            auto& section = createModuleSection (parent, "Processor Instances");

            for (auto& p : m.processorInstances)
            {
                auto& div = section.addDiv ("processor_instances");

                auto& instance = div.addParagraph().setClass ("processor_instance").setID (p.UID);
                instance.addSpan ("name").addContent (p.name);
                instance.addContent (" = ");
                printExpression (instance, p.targetProcessor);
                instance.addContent (" ");
                printExpression (instance, p.specialisationArgs);

                if (! p.clockMultiplierRatio.sections.empty())
                {
                    instance.addContent (" * ");
                    printExpression (instance, p.clockMultiplierRatio);
                }
                else if (! p.clockDividerRatio.sections.empty())
                {
                    instance.addContent (" / ");
                    printExpression (instance, p.clockDividerRatio);
                }

                if (! p.arraySize.sections.empty())
                {
                    instance.addContent (" [");
                    printExpression (instance, p.arraySize);
                    instance.addContent ("]");
                }
            }
        }
    }

    void printConnections (choc::html::HTMLElement& parent, const SourceCodeModel::Module& m)
    {
        if (! m.connections.empty())
        {
            auto& section = createModuleSection (parent, "Connections");

            for (auto& c : m.connections)
            {
                auto& div = section.addDiv ("connections");

                auto& connection = div.addParagraph().setClass ("connection");

                if (! (c.interpolationType.empty() || c.interpolationType == "none"))
                    connection.addContent ("[" + c.interpolationType + "] ");

                printExpression (connection, c.sourceEndpoint);

                auto& arrow = connection.addSpan ("connection_arrow");
                arrow.addContent (" -> ");

                if (! c.delayLength.sections.empty())
                {
                    arrow.addContent ("[");
                    printExpression (arrow, c.delayLength);
                    arrow.addContent ("] -> ");
                }

                printExpression (connection, c.destEndpoint);
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

                if (leadingSpaces >= codeSectionIndent || trimmedLine.empty())
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

    static char getChar (std::string_view text, size_t index)    { return index < text.length() ? text[index] : 0; }

    enum class CharType { whitespace, text, other };

    static CharType getCharType (char c)
    {
        if (c == 0 || std::iswspace (static_cast<wint_t> (c)))
            return CharType::whitespace;

        if (std::isalnum (c))
            return CharType::text;

        return CharType::other;
    }

    static auto findDelimiter (std::string_view text, std::string_view delimiter, size_t startPos)
    {
        auto index = text.find (delimiter, startPos);

        if (index != std::string_view::npos)
            if (getChar (text, index - 1) != delimiter.front()
                  && getChar (text, index + delimiter.length()) != delimiter.back())
                return index;

        return std::string_view::npos;
    }

    struct DelimitedSection
    {
        std::string_view::size_type outerStart = 0, outerEnd = 0, delimiterLength = 0;
    };

    static DelimitedSection findDelimitedSection (std::string_view text, std::string_view delimiter)
    {
        auto start = findDelimiter (text, delimiter, 0);

        if (start != std::string_view::npos)
        {
            auto end = findDelimiter (text, delimiter, start + delimiter.length());

            if (end != std::string_view::npos)
            {
                auto preStart  = getCharType (getChar (text, start - 1));
                auto postStart = getCharType (getChar (text, start + delimiter.length()));

                auto preEnd  = getCharType (getChar (text, end - 1));
                auto postEnd = getCharType (getChar (text, end + delimiter.length()));

                if (preStart != CharType::text
                     && postEnd != CharType::text
                     && postStart != CharType::whitespace
                     && preEnd != CharType::whitespace)
                    return { start, end + delimiter.length(), delimiter.length() };
            }
        }

        return {};
    }

    struct DelimiterType
    {
        std::function<DelimitedSection(std::string_view)> findNextMatch;
        std::function<void(choc::html::HTMLElement&, std::string_view)> addReplacementElement;
    };

    static void appendSpansForContent (choc::html::HTMLElement& parent, std::string_view markdown)
    {
        static const DelimiterType delimiterTypes[] =
        {
            {
                // `code`
                [] (std::string_view text) -> DelimitedSection
                {
                    return findDelimitedSection (text, "`");
                },
                [] (choc::html::HTMLElement& parentElement, std::string_view text)
                {
                    parentElement.addChild ("code").addContent (text);
                }
            },
            {
                // _Emphasis_
                [] (std::string_view text) -> DelimitedSection
                {
                    return findDelimitedSection (text, "_");
                },
                [] (choc::html::HTMLElement& parentElement, std::string_view text)
                {
                    appendSpansForContent (parentElement.addChild ("em").setInline (true), text);
                }
            },
            {
                // **Bold**
                [] (std::string_view text) -> DelimitedSection
                {
                    return findDelimitedSection (text, "**");
                },
                [] (choc::html::HTMLElement& parentElement, std::string_view text)
                {
                    appendSpansForContent (parentElement.addChild ("strong").setInline (true), text);
                }
            },
            {
                // Links..
                [] (std::string_view text) -> DelimitedSection
                {
                    for (auto* proto : { "http:", "https:", "file:" })
                    {
                        auto start = text.find (proto);

                        if (start != std::string::npos)
                        {
                            auto end = text.find (' ', start);

                            if (end == std::string_view::npos) end = text.find ('\n', start);
                            if (end == std::string_view::npos) end = text.length();

                            return { start, end, 0 };
                        }
                    }

                    return {};
                },
                [] (choc::html::HTMLElement& parentElement, std::string_view text)
                {
                    parentElement.addLink (text).addContent (text);
                }
            }
        };

        struct FoundDelimiter
        {
            DelimitedSection range;
            const DelimiterType* type;

            bool operator< (const FoundDelimiter& other) const    { return range.outerStart < other.range.outerStart; }
        };

        ArrayWithPreallocation<FoundDelimiter, 4> found;

        for (auto& type : delimiterTypes)
        {
            auto range = type.findNextMatch (markdown);

            if (range.outerEnd > range.outerStart)
                found.push_back ({ range, &type });
        }

        if (! found.empty())
        {
            std::sort (found.begin(), found.end());
            auto range = found.front().range;

            auto part1 = markdown.substr (0, range.outerStart);
            auto part2 = std::string (markdown.substr (range.outerStart + range.delimiterLength,
                                                       range.outerEnd - range.outerStart - range.delimiterLength * 2));
            auto part3 = markdown.substr (range.outerEnd);

            appendSpansForContent (parent, part1);
            found.front().type->addReplacementElement (parent, part2);
            appendSpansForContent (parent, part3);
            return;
        }

        if (! markdown.empty())
            parent.addContent (markdown);
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
                    auto type = choc::text::trim (trimmed.substr (3, endOfLine1 - 3));

                    if (endOfLine1 < trimmed.length() - 1)
                        ++endOfLine1;

                    auto content = trimmed.substr (endOfLine1);
                    auto& code = getParent().addChild ("code").setInline (true);

                    if (type == "soul")
                    {
                        code.setClass ("hljs");

                        SourceCodeUtilities::iterateSyntaxTokens (CodeLocation::createFromString ({}, std::move (content)),
                                                                  [&] (std::string_view token, std::string_view cssClass) -> bool
                        {
                            if (cssClass.empty())
                                code.addContent (token);
                            else
                                code.addSpan (cssClass).addContent (token);

                            return true;
                        });
                    }
                    else
                    {
                        code.setClass ("unspecified_code").addContent (content);
                    }
                }

                continue;
            }

            if (leadingSpaces >= 4)
            {
                getParent().addChild ("code").setClass ("unspecified_code").setInline (true)
                    .addContent (choc::text::trimEnd (paragraph));

                continue;
            }

            if (! trimmed.empty())
                appendSpansForContent (getParent().addParagraph(), trimmed);
        }
    }

    static void addComment (choc::html::HTMLElement& parent,
                            const SourceCodeUtilities::Comment& comment,
                            std::string_view classType)
    {
        if (comment.valid && ! comment.range.isEmpty())
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
