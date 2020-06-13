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

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

namespace soul
{

//==============================================================================
CompileMessageList::CompileMessageList() = default;
CompileMessageList::~CompileMessageList() = default;

bool CompileMessage::isWarning() const                  { return type == Type::warning; }
bool CompileMessage::isError() const                    { return type == Type::error || isInternalCompilerError(); }
bool CompileMessage::isInternalCompilerError() const    { return type == Type::internalCompilerError; }

CompileMessage CompileMessage::withLocation (CodeLocation l) const
{
    return { description, std::move (l), type };
}

std::string CompileMessage::getFullDescriptionWithoutFilename() const
{
    std::string filename;

    if (hasPosition())
        filename = getPositionString() + ": ";

    return filename + getSeverity() + ": " + description;
}

std::string CompileMessage::getFullDescription() const
{
    auto filename = location.getFilename();

    if (filename.empty())
        return getFullDescriptionWithoutFilename();

    if (hasPosition())
        filename += ":" + getPositionString();

    return filename + ": " + getSeverity() + ": " + description;
}

bool CompileMessage::hasPosition() const
{
    return location.location.getAddress() != nullptr;
}

std::string CompileMessage::getPositionString() const
{
    if (! hasPosition())
        return "0:0";

    auto lc = location.getLineAndColumn();
    return std::to_string (lc.line) + ":" + std::to_string (lc.column);
}

std::string CompileMessage::getSeverity() const
{
    if (isError())    return "error";
    if (isWarning())  return "warning";

    SOUL_ASSERT_FALSE;
    return {};
}

std::string CompileMessage::getAnnotatedSourceLine() const
{
    if (location.location.getAddress() != nullptr)
    {
        auto lc = location.getLineAndColumn();

        if (lc.column != 0)
        {
            auto sourceLine = location.getSourceLine();
            std::string indent;

            // because some fools insist on using tab characters, we need to make sure we mirror
            // any tabs in the original source line when indenting the '^' character, so that when
            // it's printed underneath it lines-up regardless of tab size
            for (size_t i = 0; i < lc.column - 1; ++i)
                indent += sourceLine[i] == '\t' ? '\t' : ' ';

            return sourceLine + "\n" + indent + "^";
        }
    }

    return {};
}

//==============================================================================
void CompileMessageList::add (const CompileMessage& message)
{
    messages.push_back (message);

    if (onMessage != nullptr)
        onMessage (message);
}

void CompileMessageList::addError (const std::string& desc, CodeLocation location)
{
    add ({ desc, location, CompileMessage::Type::error });
}

void CompileMessageList::addWarning (const std::string& desc, CodeLocation location)
{
    add ({ desc, location, CompileMessage::Type::warning });
}

void CompileMessageList::add (const CompileMessageList& other)
{
    for (auto& m : other.messages)
        add (m);
}

void CompileMessageList::add (const CompileMessageGroup& group)
{
    for (auto& m : group.messages)
        add (m);
}

bool CompileMessageList::hasErrorsOrWarnings() const
{
    return ! messages.empty();
}

bool CompileMessageList::hasErrors() const
{
    for (auto& m : messages)
        if (m.isError())
            return true;

    return false;
}

bool CompileMessageList::hasWarnings() const
{
    for (auto& m : messages)
        if (m.isWarning())
            return true;

    return false;
}

bool CompileMessageList::hasInternalCompilerErrors() const
{
    for (auto& m : messages)
        if (m.isInternalCompilerError())
            return true;

    return false;
}

void CompileMessageList::clear()
{
    messages.clear();
}

std::string CompileMessageList::toString() const
{
    std::ostringstream oss;

    for (const auto& message : messages)
        oss << message.getFullDescription() << std::endl
            << message.getAnnotatedSourceLine() << std::endl;

    return oss.str();
}

//==============================================================================
static thread_local CompileMessageHandler* messageHandler = nullptr;

CompileMessageHandler::CompileMessageHandler (HandleMessageFn m)
   : handleMessageFn (std::move (m)), lastHandler (messageHandler)
{
    messageHandler = this;
}

CompileMessageHandler::CompileMessageHandler (CompileMessageList& list)
    : CompileMessageHandler ([&] (const CompileMessageGroup& messageGroup)
                             {
                                 list.add (messageGroup);
                             })
{
}

CompileMessageHandler::~CompileMessageHandler()
{
    messageHandler = lastHandler;
}

bool CompileMessageHandler::isHandlerEnabled()
{
    return messageHandler != nullptr;
}

//==============================================================================
void catchParseErrors (std::function<void()>&& functionToPerform)
{
    struct ErrorWasIgnoredException {};

    struct ParseErrorIgnoringMessageHandler  : public CompileMessageHandler
    {
        ParseErrorIgnoringMessageHandler()
            : CompileMessageHandler ([this] (const CompileMessageGroup& messageGroup)
                                     {
                                         for (auto& m : messageGroup.messages)
                                             if (! m.isInternalCompilerError())
                                                 throw ErrorWasIgnoredException();

                                         if (lastHandler != nullptr)
                                             lastHandler->handleMessageFn (messageGroup);
                                     })
        {}
    };

    try
    {
        ParseErrorIgnoringMessageHandler handler;
        functionToPerform();
    }
    catch (ErrorWasIgnoredException) {}
}

//==============================================================================
void emitMessage (const CompileMessageGroup& messageGroup)
{
    if (messageHandler != nullptr)
        messageHandler->handleMessageFn (messageGroup);
}

void emitMessage (CompileMessage m)
{
    CompileMessageGroup messageGroup;
    messageGroup.messages.push_back (std::move (m));
    emitMessage (messageGroup);
}

[[noreturn]] void throwError (const CompileMessageGroup& messageGroup)
{
    emitMessage (messageGroup);
    throw AbortCompilationException();
}

[[noreturn]] void throwError (CompileMessage m)
{
    CompileMessageGroup messageGroup;
    messageGroup.messages.push_back (std::move (m));
    throwError (messageGroup);
}

[[noreturn]] void throwInternalCompilerError (const std::string& message)
{
    soul::throwError ({ "Internal compiler error: " + choc::text::addDoubleQuotes (message),
                        CodeLocation(), CompileMessage::Type::internalCompilerError });
}

[[noreturn]] void throwInternalCompilerError (const char* location, int line)
{
    throwInternalCompilerError (std::string (location) + ":" + std::to_string (line));
}

[[noreturn]] void throwInternalCompilerError (const char* message, const char* location, int line)
{
    throwInternalCompilerError (choc::text::addDoubleQuotes (message) + " failed at " + location + ":" + std::to_string (line));
}

void checkAssertion (bool condition, const char* message, const char* location, int line)
{
    if (! condition)
        throwInternalCompilerError (message, location, line);
}

void checkAssertion (bool condition, const std::string& message, const char* location, int line)
{
    checkAssertion (condition, message.c_str(), location, line);
}

void checkAssertion (bool condition, const char* location, int line)
{
    checkAssertion (condition, "false", location, line);
}

} // namespace soul
