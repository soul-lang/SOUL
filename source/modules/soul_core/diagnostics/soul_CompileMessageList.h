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
struct CompileMessage  final
{
    bool isWarning() const;
    bool isError() const;
    bool isInternalCompilerError() const;

    std::string getFullDescription() const;
    std::string getFullDescriptionWithoutFilename() const;

    bool hasPosition() const;
    std::string getPositionString() const;

    std::string getSeverity() const;
    std::string getAnnotatedSourceLine() const;

    CompileMessage withLocation (CodeLocation) const;

    enum class Type
    {
        error,
        warning,
        internalCompilerError
    };

    enum class Category
    {
        none,
        syntax,
        limitExceeded,
        performanceProblem,
        runtimeProblem
    };

    std::string description;
    CodeLocation location;
    Type type = Type::error;
    Category category = Category::none;
};

struct CompileMessageGroup
{
    ArrayWithPreallocation<CompileMessage, 4> messages;
};

//==============================================================================
/** A list of errors and warnings. */
struct CompileMessageList  final
{
    CompileMessageList();
    ~CompileMessageList();

    //==============================================================================
    void add (const CompileMessage&);
    void add (const CompileMessageGroup&);
    void add (const CompileMessageList&);

    void addError (const std::string& desc, CodeLocation);
    void addWarning (const std::string& desc, CodeLocation);

    bool hasErrors() const;
    bool hasWarnings() const;
    bool hasErrorsOrWarnings() const;
    bool hasInternalCompilerErrors() const;

    void clear();

    /** Returns a dump of all the messages - this is probably want you want to use
        if you're printing the output of a compilation.
    */
    std::string toString() const;

    /** If this lambda is set, then every time a message is added, the callback is
        called, allowing user code to do some custom task with it.
    */
    std::function<void(const CompileMessage&)> onMessage;

    /** The raw list of messages. */
    std::vector<CompileMessage> messages;
};

//==============================================================================
/** An RAII class used to provide a handler for errors, warnings and assertions for the current thread.
*/
struct CompileMessageHandler
{
    using HandleMessageFn = std::function<void(const CompileMessageGroup&)>;

    /** This is the most commonly used constructor - it just adds all incoming messages to a list. */
    CompileMessageHandler (CompileMessageList&);

    /** This constructor allows custom behaviour when a message is emitted. */
    CompileMessageHandler (HandleMessageFn);
    ~CompileMessageHandler();

    /** True if the current thread has a handler active. */
    static bool isHandlerEnabled();

    HandleMessageFn handleMessageFn;
    CompileMessageHandler* const lastHandler;
};

//==============================================================================
/** Calls the given function while catching any non-fatal parse errors that get thrown. */
void catchParseErrors (std::function<void()>&& functionToPerform);

//==============================================================================
/** Throwing one of these in any compile task will stop the current compilation. */
struct AbortCompilationException {};

//==============================================================================
/** Sends an error or warning message to the current message handler. */
void emitMessage (CompileMessage);
/** Sends a set of error or warning messages to the current message handler. */
void emitMessage (const CompileMessageGroup&);

/** Sends an error message to the current message handler and throws an AbortCompilationException. */
[[noreturn]] void throwError (CompileMessage);
/** Sends a set of error messages to the current message handler and throws an AbortCompilationException. */
[[noreturn]] void throwError (const CompileMessageGroup&);


} // namespace soul
