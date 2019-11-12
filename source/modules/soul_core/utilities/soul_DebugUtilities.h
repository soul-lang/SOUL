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

//==============================================================================
#if defined (DEBUG) || defined (_DEBUG) || ! (defined (NDEBUG) || defined (_NDEBUG))
  #define SOUL_DEBUG 1
#endif

//==============================================================================
#ifndef SOUL_ENABLE_ASSERTIONS
 #define SOUL_ENABLE_ASSERTIONS 1
#endif

#if SOUL_ENABLE_ASSERTIONS
 #define SOUL_ASSERT(x)     soul::checkAssertion (x, #x, __FUNCTION__, __LINE__)
#else
 #define SOUL_ASSERT(x)     do {} while (false)
#endif

#define SOUL_ASSERT_FALSE  soul::throwInternalCompilerError (__FUNCTION__, __LINE__)

// We use the SOUL_TODO macro to noisily mark bits of code that need to be fixed urgently
#if SOUL_DISABLE_TODO_WARNINGS
  #define SOUL_TODO
#else
  #ifdef __clang__
   #define SOUL_TODO  _Pragma("message(\"TODO\")")
  #else
   #define SOUL_TODO // ignore this on Windows for now
  #endif
#endif


//==============================================================================
namespace soul
{
    [[noreturn]] void throwInternalCompilerError (const std::string& message);
    [[noreturn]] void throwInternalCompilerError (const char* location, int line);
    [[noreturn]] void throwInternalCompilerError (const char* message, const char* location, int line);

    void checkAssertion (bool condition, const std::string& message, const char* location, int line);
    void checkAssertion (bool condition, const char* message, const char* location, int line);
    void checkAssertion (bool condition, const char* location, int line);
}

#if defined(__clang__)
 #define SOUL_NO_SIGNED_INTEGER_OVERFLOW_WARNING __attribute__((no_sanitize("signed-integer-overflow")))
#else
 #define SOUL_NO_SIGNED_INTEGER_OVERFLOW_WARNING
#endif
