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

namespace soul::cpp
{
    struct CodeGenOptions
    {
        BuildSettings buildSettings;
        std::string className;               ///< Leave blank to use a default class name. NB: the generateCode method will update this to the name it used
        std::string classNamespace;          ///< Optionally specifies a namespace to wrap around the generated class
        std::string sourceDescription;       ///< An optional source file desc that will be added to a comment
        soul::Annotation staticConstants;    ///< A set of properties which will get added as static constexpr values
        bool createEndpointFunctions = true; ///< Whether to create methods that return a list of the available endpoints
        bool generateRenderMethod = true;    ///< Whether to create a high-level render method
        bool generatePluginMethods = true;   ///< Whether to create some high-level plugin-style helpers
        bool packStructures = false;         ///< Whether to pack the generated class
        bool generateJUCEHeader = false;     ///< If true, creates the .h for a juce::AudioPluginInstance
        bool generateJUCECPP = false;        ///< If true, creates the .cpp header for a juce::AudioPluginInstance
    };

    /// Runs the C++ generator with the given options, writing the C++ to the given CodePrinter.
    /// Any errors will be reported to the message list provided.
    /// On exit, the generator will have updated the CodeGenOptions::className field so you can
    /// find out exactly what name was used.
    /// Returns true if no errors were encountered.
    bool generateCode (choc::text::CodePrinter& destination,
                       Program programToCompile,
                       CompileMessageList& messages,
                       CodeGenOptions& options);

    /// Uses a default CodePrinter to generate C++ and return the result as a string
    std::string generateCode (Program programToCompile,
                              CompileMessageList& messages,
                              CodeGenOptions& options);

    /// Creates a set of files that form a JUCE plugin project
    std::vector<soul::SourceFile> generateJUCEProjectFiles (const Program& programToCompile,
                                                            CompileMessageList& messages,
                                                            CodeGenOptions& options);
}
