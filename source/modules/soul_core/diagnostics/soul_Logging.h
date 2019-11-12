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
    Channels general log messages through a customisable callback function.
*/
class Logger  final
{
public:
    /** A log message */
    struct Message
    {
        std::string description, detail;
    };

    /** A user-supplied callback that can be registered with setLogFunction(). */
    using LoggingFunction = std::function<void(const Message&)>;

    /** Logs a message */
    static void log (std::string description, std::string detail);

    /** Logs a message via a lambda that will only called if a logging
        callback is currently enabled.
        If it's expensive to generate the string that creates the message, this
        allows that work to be skipped if logging is turned off.
    */
    static void log (std::string description, const std::function<std::string()>& detail);

    /** Logs a message. */
    static void log (const Message&);

    /** Installs a user-supplied logging callback. */
    static void setLogFunction (LoggingFunction);

    /** Removes any currently installed logging callback. */
    static void clearLogFunction();

    /** Returns true if a logging function is currently attached. */
    static bool isLoggingEnabled() noexcept;

private:
    struct LoggerHolder
    {
        std::mutex lock;
        LoggingFunction callback;
    };

    static LoggerHolder& getCallbackHolder();
};

#define SOUL_LOG(desc, detail) \
    if (soul::Logger::isLoggingEnabled()) soul::Logger::log (desc, detail);


} // namespace soul
