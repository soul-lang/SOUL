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
/** A class for building a model of all the info needed to generate
    some documentation for a soul file.
*/
struct DocumentationModel
{
    bool generate (CompileMessageList&, ArrayView<SourceCodeText::Ptr> files);

    //==============================================================================
    struct TypeDesc
    {
        struct Section
        {
            enum class Type { keyword, text, structure, primitive };

            std::string text;
            Type type;
        };

        std::vector<Section> sections;
    };

    struct EndpointDesc
    {
        SourceCodeOperations::Comment comment;
        std::string type, name;
        std::vector<TypeDesc> dataTypes;
    };

    struct FunctionDesc
    {
        SourceCodeOperations::Comment comment;
        TypeDesc returnType;
        std::string bareName, nameWithGenerics;

        struct Parameter
        {
            TypeDesc type;
            std::string name;
        };

        std::vector<Parameter> parameters;
    };

    struct StructDesc
    {
        SourceCodeOperations::Comment comment;
        std::string name;

        struct Member
        {
            SourceCodeOperations::Comment comment;
            TypeDesc type;
            std::string name;
        };

        std::vector<Member> members;
    };

    struct VariableDesc
    {
        SourceCodeOperations::Comment comment;
        TypeDesc type;
        std::string name;
        bool isExternal = false;
    };

    struct SpecialisationParamDesc
    {
        TypeDesc type;
        std::string name;
    };

    struct ModuleDesc
    {
        const SourceCodeOperations::ModuleDeclaration& module;
        std::string typeName, displayName;

        std::vector<SpecialisationParamDesc> specialisationParams;
        std::vector<EndpointDesc> inputs, outputs;
        std::vector<FunctionDesc> functions;
        std::vector<VariableDesc> variables;
        std::vector<StructDesc> structs;
    };

    struct FileDesc
    {
        std::string filename, title, summary;
        std::unique_ptr<SourceCodeOperations> source;
        std::vector<ModuleDesc> modules;
    };

    struct TOCNode
    {
        std::string name;
        std::vector<TOCNode> children;
        ModuleDesc* module = nullptr;
        FileDesc* file = nullptr;

        TOCNode& getNode (ArrayView<std::string> path);
    };

    static SourceCodeOperations::Comment getComment (const AST::Context& context);
    static bool shouldIncludeComment (const SourceCodeOperations::Comment& comment);

    bool shouldShow (const AST::Function&);
    bool shouldShow (const AST::VariableDeclaration&);
    bool shouldShow (const AST::StructDeclaration&);
    bool shouldShow (const SourceCodeOperations::ModuleDeclaration&);

    //==============================================================================
    static std::string getStringBetween (CodeLocation start, CodeLocation end);
    static CodeLocation findNextOccurrence (CodeLocation start, char character);

    //==============================================================================
    std::vector<FileDesc> files;
    TOCNode topLevelTOCNode;

private:
    //==============================================================================
    void addModule (const SourceCodeOperations::ModuleDeclaration&);

    void buildTOCNodes();
    void buildSpecialisationParams();
    void buildEndpoints();
    void buildFunctions();
    void buildStructs();
    void buildVariables();
};


} // namespace soul
