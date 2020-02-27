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
void Logger::log (std::string description, std::string detail)
{
    log ({ std::move (description), std::move (detail) });
}

void Logger::log (std::string description, const std::function<std::string()>& detail)
{
    log ({ std::move (description), detail != nullptr ? detail() : std::string() });
}

void Logger::log (const Message& message)
{
    auto& holder = getCallbackHolder();
    std::lock_guard<std::mutex> g (holder.lock);

    if (holder.callback != nullptr)
        holder.callback (message);
}

void Logger::setLogFunction (LoggingFunction f)
{
    auto& holder = getCallbackHolder();
    std::lock_guard<std::mutex> g (holder.lock);
    holder.callback = f;
}

void Logger::clearLogFunction()
{
    setLogFunction (nullptr);
}

bool Logger::isLoggingEnabled() noexcept
{
    auto& holder = getCallbackHolder();
    std::lock_guard<std::mutex> g (holder.lock);
    return holder.callback != nullptr;
}

Logger::LoggerHolder& Logger::getCallbackHolder()
{
    static LoggerHolder l;
    return l;
}


} // namespace soul
