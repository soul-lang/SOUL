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
/** A model representing the structure of a SOUL program as contained in
    a set of source files.

    The main purpose of this class is to act as a simple model for documentation
    generation and other source-manipulation utilities to work with, where the
    AST itself would be too complex and not expose quite the right set of properties.
*/
struct SourceCodeModel
{
    //==============================================================================
    bool rebuild (CompileMessageList&, choc::span<SourceCodeText::Ptr> files);

    //==============================================================================
    struct Expression
    {
        struct Section
        {
            enum class Type { keyword, text, structure, primitive };

            Type type;
            std::string text, referencedUID;
        };

        std::vector<Section> sections;

        std::string toString() const;
    };

    struct Annotation
    {
        std::unordered_map<std::string, Expression> properties;
    };

    struct Variable
    {
        SourceCodeUtilities::Comment comment;
        Expression type;
        std::string UID, name, initialiser;
        bool isExternal = false;
    };

    struct Function
    {
        SourceCodeUtilities::Comment comment;
        Expression returnType;
        std::string UID, bareName, nameWithGenerics, fullyQualifiedName;
        std::vector<Variable> parameters;
        Annotation annotation;
    };

    struct Struct
    {
        SourceCodeUtilities::Comment comment;
        std::string UID, fullName, shortName;

        struct Member
        {
            SourceCodeUtilities::Comment comment;
            Expression type;
            std::string UID, name;
        };

        std::vector<Member> members;
    };

    struct SpecialisationParameter
    {
        Expression type;
        std::string UID, name, defaultValue;
        Annotation annotation;
    };

    struct Endpoint
    {
        SourceCodeUtilities::Comment comment;
        std::string UID, endpointType, name;
        std::vector<Expression> dataTypes;
        Annotation annotation;
    };

    struct Connection
    {
        Expression sourceEndpoint, destEndpoint;
        std::string interpolationType;
        Expression delayLength;
    };

    struct ProcessorInstance
    {
        std::string UID, name;
        Expression targetProcessor, specialisationArgs,
                   clockMultiplierRatio, clockDividerRatio, arraySize;
    };

    struct Module
    {
        bool isNamespace = false, isProcessor = false, isGraph = false;
        std::string UID, moduleTypeDescription, fullyQualifiedName;
        SourceCodeUtilities::Comment comment;
        Annotation annotation;

        std::vector<SpecialisationParameter> specialisationParams;
        std::vector<Endpoint> inputs, outputs;
        std::vector<Function> functions;
        std::vector<Variable> variables;
        std::vector<Struct> structs;
        std::vector<ProcessorInstance> processorInstances;
        std::vector<Connection> connections;
    };

    struct File
    {
        SourceCodeText::Ptr source;
        SourceCodeUtilities::Comment fileComment;
        std::string UID, filename, title, summary;
        std::vector<Module> modules;
    };

    //==============================================================================
    std::vector<File> files;

    //==============================================================================
    struct TableOfContentsNode
    {
        std::string name;
        std::vector<TableOfContentsNode> children;
        const Module* module = nullptr;
        const File* file = nullptr;
    };

    TableOfContentsNode createTableOfContentsRoot() const;
};


} // namespace soul
