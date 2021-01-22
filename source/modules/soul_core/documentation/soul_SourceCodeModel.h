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
    bool generate (CompileMessageList&, ArrayView<SourceCodeText::Ptr> files);

    //==============================================================================
    struct Expression
    {
        struct Section
        {
            enum class Type { keyword, text, structure, primitive };

            Type type;
            std::string text;
        };

        std::vector<Section> sections;

        std::string toString() const;
    };

    struct Endpoint
    {
        SourceCodeUtilities::Comment comment;
        std::string UID, endpointType, name;
        std::vector<Expression> dataTypes;
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
    };

    struct ModuleDesc
    {
        AST::ModuleBase& module;
        AST::Allocator& allocator;

        std::string UID, typeOfModule, fullyQualifiedName;
        SourceCodeUtilities::Comment comment;

        std::vector<SpecialisationParameter> specialisationParams;
        std::vector<Endpoint> inputs, outputs;
        std::vector<Function> functions;
        std::vector<Variable> variables;
        std::vector<Struct> structs;

        std::string resolvePartialNameAsUID (const std::string&) const;
    };

    struct FileDesc
    {
        SourceCodeText::Ptr source;
        SourceCodeUtilities::Comment fileComment;
        std::string filename, UID, title, summary;
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

    static SourceCodeUtilities::Comment getComment (const AST::Context& context);
    static bool shouldIncludeComment (const SourceCodeUtilities::Comment& comment);

    bool shouldShow (const AST::Function&);
    bool shouldShow (const AST::VariableDeclaration&);
    bool shouldShow (const AST::StructDeclaration&);
    bool shouldShow (const ModuleDesc&);

    std::string findType (const std::string& partialType) const;

    //==============================================================================
    static std::string getStringBetween (CodeLocation start, CodeLocation end);
    static CodeLocation findNextOccurrence (CodeLocation start, char character);
    static CodeLocation findEndOfExpression (CodeLocation start);

    //==============================================================================
    std::vector<FileDesc> files;
    TOCNode topLevelTOCNode;

private:
    //==============================================================================
    AST::Allocator allocator;
    pool_ptr<AST::Namespace> topLevelNamespace;

    void recurseFindingModules (AST::ModuleBase&, FileDesc&);
    ModuleDesc createModule (AST::ModuleBase&);

    void buildTOCNodes();
    void buildSpecialisationParams();
    void buildEndpoints();
    void buildFunctions();
    void buildStructs();
    void buildVariables();
};


} // namespace soul
