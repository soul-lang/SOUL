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
    Compiles and links some source code to create a Program that can be
    passed to a device for running.

    You can either create a Compiler, feed it some individual chunks of code with
    addCode() and then call link() to create a finished Program. Or you can just call
    Compiler::build() to do this in one step for a set of files.
*/
class Compiler  final
{
public:
    Compiler (bool includeStandardLibrary = true);

    /** This static method runs a complete build and link for a BuildBundle, and returns
        the resulting program.
    */
    static Program build (CompileMessageList& messageList,
                          const BuildBundle& buildBundle);

    /** Compiles a chunk of code which is expected to contain a list of top-level
        processor/graph/namespace decls, and these are added to the program.
    */
    bool addCode (CompileMessageList& messageList, CodeLocation code);

    /** After adding one or more chunks of code, call this to link them all together
        into a single program, which is returned. After calling this, the state
        of the Compiler object is reset to empty.
    */
    Program link (CompileMessageList& messageList, const BuildSettings&);

    /** Just parses the top-level objects from a chunk of code */
    static std::vector<pool_ref<AST::ModuleBase>> parseTopLevelDeclarations (AST::Allocator&,
                                                                             CodeLocation code,
                                                                             AST::Namespace& parentNamespace);

private:
    //==============================================================================
    AST::Allocator allocator;
    pool_ptr<AST::Namespace> topLevelNamespace;

    void reset();
    void addDefaultBuiltInLibrary();
    void compile (CodeLocation);
    Program link (CompileMessageList&, const BuildSettings&, AST::ProcessorBase& processorToRun);
    AST::ProcessorBase& findMainProcessor (const BuildSettings&);

    void compileAllModules (const AST::Namespace& parentNamespace, Program&, AST::ProcessorBase& processorToRun);

    bool includeStandardLibrary;
};

} // namespace soul
