//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_TESTS_HEADER_INCLUDED
#define CHOC_TESTS_HEADER_INCLUDED

#include <iostream>

#include "../platform/choc_Platform.h"
#include "../platform/choc_SpinLock.h"
#include "../text/choc_CodePrinter.h"
#include "../text/choc_FloatToString.h"
#include "../text/choc_HTML.h"
#include "../text/choc_JSON.h"
#include "../text/choc_StringUtilities.h"
#include "../text/choc_UTF8.h"
#include "../math/choc_MathHelpers.h"
#include "../containers/choc_DirtyList.h"
#include "../containers/choc_Span.h"
#include "../containers/choc_Value.h"
#include "../containers/choc_MultipleReaderMultipleWriterFIFO.h"
#include "../containers/choc_SingleReaderMultipleWriterFIFO.h"
#include "../containers/choc_SingleReaderSingleWriterFIFO.h"
#include "../containers/choc_VariableSizeFIFO.h"
#include "../containers/choc_SmallVector.h"
#include "../containers/choc_PoolAllocator.h"
#include "../audio/choc_MIDI.h"
#include "../audio/choc_MIDIFile.h"
#include "../audio/choc_SampleBuffers.h"

#ifndef CHOC_JAVASCRIPT_IMPLEMENTATION
 #define CHOC_JAVASCRIPT_IMPLEMENTATION 1
#endif

#include "../javascript/choc_javascript.h"

/**
    To keep things simpole for users, I've just shoved all the tests for everything into this
    one dependency-free header, and provided one function call (`choc::test::runAllTests) that
    tests everything.

    The idea is that you can then simply include this header and call runAllTests() somewhere in
    your own test build, to make sure that everything is working as expected in your project.

    At some point the library will probably grow to a size where this needs to be refactored into
    smaller modules and done in a more sophisticated way, but we're not there yet!
*/
namespace choc::test
{

/// Keeps track of the number of passes/fails for a test-run.
struct TestProgress
{
    void startCategory (std::string category);
    void startTest (std::string_view testName);
    void endTest();
    void fail (const char* filename, int lineNumber, std::string_view message);
    void check (bool, const char* filename, int lineNumber, std::string_view failureMessage);
    void print (std::string_view);

    /// Call this at the end to print out the number of passes and fails
    void printReport();

    /// This is used to print progress messages as the tests run. If you don't
    /// supply your own funciton here, the default will print to std::cout
    std::function<void(std::string_view)> printMessage;

    std::string currentCategory, currentTest;
    int numPasses = 0, numFails = 0;
    bool currentTestFailed = false;
    std::vector<std::string> failedTests;
};

/// Just create a TestProgress and pass it to this function to run all the
/// tests. The TestProgress object contains a callback that will be used
/// to log its progress.
bool runAllTests (TestProgress&);


// Some macros to use to perform tests. If you want to use this to write your own tests,
// have a look at the tests for choc functions, and it should be pretty obvious how the
// macros are supposed to be used.

#define CHOC_CATEGORY(category)          progress.startCategory (#category);
#define CHOC_TEST(name)                  ScopedTest scopedTest_ ## __LINE__ (progress, #name);
#define CHOC_FAIL(message)               progress.fail (__FILE__, __LINE__, message);
#define CHOC_EXPECT_TRUE(b)              progress.check (b, __FILE__, __LINE__, "Expected " #b);
#define CHOC_EXPECT_FALSE(b)             progress.check (! (b), __FILE__, __LINE__, "Expected ! " #b);
#define CHOC_EXPECT_EQ(a, b)             { auto x = a; auto y = b; progress.check (x == y, __FILE__, __LINE__, "Expected " #a " (" + choc::test::convertToString (x) + ") == " #b " (" + choc::test::convertToString (y) + ")"); }
#define CHOC_EXPECT_NE(a, b)             { auto x = a; auto y = b; progress.check (x != y, __FILE__, __LINE__, "Expected " #a " (" + choc::test::convertToString (x) + ") != " #b); }
#define CHOC_EXPECT_NEAR(a, b, diff)     { auto x = a; auto y = b; auto d = diff; progress.check (std::abs (x - y) <= d, __FILE__, __LINE__, #a " (" + choc::test::convertToString (x) + ") and " #b " (" + choc::test::convertToString (y) + ") differ by more than " + choc::test::convertToString (d)); }
#define CHOC_CATCH_UNEXPECTED_EXCEPTION  catch (...) { CHOC_FAIL ("Unexpected exception thrown"); }


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline void TestProgress::print (std::string_view message)
{
    if (printMessage != nullptr)
        printMessage (message);
    else
        std::cout << message << std::endl;
}

inline void TestProgress::startCategory (std::string category)
{
    currentCategory = std::move( category);
}

inline void TestProgress::startTest (std::string_view testName)
{
    CHOC_ASSERT (! currentCategory.empty());
    currentTest = currentCategory + "/" + std::string (testName);
    currentTestFailed = false;
    print ("[ RUN      ] " + currentTest);
}

inline void TestProgress::endTest()
{
    if (currentTestFailed)
    {
        print ("[     FAIL ] " + currentTest);
        ++numFails;
        failedTests.push_back (currentTest);
    }
    else
    {
        print ("[       OK ] " + currentTest);
        ++numPasses;
    }

    currentTest = {};
}

inline void TestProgress::fail (const char* filename, int lineNumber, std::string_view message)
{
    currentTestFailed = true;
    CHOC_ASSERT (! currentTest.empty());
    print ("FAILED: " + std::string (filename) + ":" + std::to_string (lineNumber));
    print (message);
}

inline void TestProgress::check (bool condition, const char* filename, int lineNumber, std::string_view message)
{
    if (! condition)
        fail (filename, lineNumber, message);
}

inline void TestProgress::printReport()
{
    print ("========================================================");
    print (" Passed:      " + std::to_string (numPasses));
    print (" Failed:      " + std::to_string (numFails));

    for (auto& failed : failedTests)
        print ("  Failed test: " + failed);

    print ("========================================================");
}

struct ScopedTest
{
    ScopedTest (TestProgress& p, std::string name) : progress (p)   { progress.startTest (std::move (name)); }
    ~ScopedTest()                                                   { progress.endTest(); }

    TestProgress& progress;
};

template <typename Type>
std::string convertToString (Type n)
{
    if constexpr (std::is_same<const Type, const char* const>::value)           return std::string (n);
    else if constexpr (std::is_same<const Type, const std::string>::value)      return n;
    else if constexpr (std::is_same<const Type, const std::string_view>::value) return std::string (n);
    else                                                                        return std::to_string (n);
}

//==============================================================================
//
//  The tests themselves....
//
//==============================================================================
inline void testContainerUtils (TestProgress& progress)
{
    CHOC_CATEGORY (Containers);

    {
        CHOC_TEST (Span)

        std::vector<int>  v { 1, 2, 3 };
        int a[] = { 1, 2, 3 };

        CHOC_EXPECT_TRUE (choc::span<int>().empty());
        CHOC_EXPECT_FALSE (choc::span<int> (a).empty());
        CHOC_EXPECT_TRUE (choc::span<int> (v).size() == 3);
        CHOC_EXPECT_TRUE (choc::span<int> (v).tail().size() == 2);
        CHOC_EXPECT_TRUE (choc::span<int> (v).createVector().size() == 3);
        CHOC_EXPECT_TRUE (choc::span<int> (v) == choc::span<int> (a));
    }
}

inline void testStringUtilities (TestProgress& progress)
{
    CHOC_CATEGORY (Strings);

    {
        CHOC_TEST (FloatToString)

        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0));
        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0));
        CHOC_EXPECT_EQ ("1.1",                      choc::text::floatToString (1.1));
        CHOC_EXPECT_EQ ("1.1",                      choc::text::floatToString (1.1));
        CHOC_EXPECT_EQ ("0.0",                      choc::text::floatToString (1.123e-6, 4));
        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0, 1));
        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0, 2));
        CHOC_EXPECT_EQ ("1.1",                      choc::text::floatToString (1.1, 2));
        CHOC_EXPECT_EQ ("1.12",                     choc::text::floatToString (1.126, 2));
        CHOC_EXPECT_EQ ("0.0012",                   choc::text::floatToString (1.23e-3, 4));
        CHOC_EXPECT_EQ ("0.0",                      choc::text::floatToString (0.0f));
        CHOC_EXPECT_EQ ("0.0",                      choc::text::floatToString (0.0f));
        CHOC_EXPECT_EQ ("-0.0",                     choc::text::floatToString (-1.0f / std::numeric_limits<float>::infinity()));
        CHOC_EXPECT_EQ ("-0.0",                     choc::text::floatToString (-1.0 / std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("inf",                      choc::text::floatToString (std::numeric_limits<float>::infinity()));
        CHOC_EXPECT_EQ ("-inf",                     choc::text::floatToString (-std::numeric_limits<float>::infinity()));
        CHOC_EXPECT_EQ ("inf",                      choc::text::floatToString (std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("-inf",                     choc::text::floatToString (-std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("nan",                      choc::text::floatToString (std::numeric_limits<float>::quiet_NaN()));
        CHOC_EXPECT_EQ ("-nan",                     choc::text::floatToString (-std::numeric_limits<float>::quiet_NaN()));
        CHOC_EXPECT_EQ ("nan",                      choc::text::floatToString (std::numeric_limits<double>::quiet_NaN()));
        CHOC_EXPECT_EQ ("-nan",                     choc::text::floatToString (-std::numeric_limits<double>::quiet_NaN()));
        CHOC_EXPECT_EQ ("3.4028235e38",             choc::text::floatToString (std::numeric_limits<float>::max()));
        CHOC_EXPECT_EQ ("1.1754944e-38",            choc::text::floatToString (std::numeric_limits<float>::min()));
        CHOC_EXPECT_EQ ("-3.4028235e38",            choc::text::floatToString (std::numeric_limits<float>::lowest()));
        CHOC_EXPECT_EQ ("1.7976931348623157e308",   choc::text::floatToString (std::numeric_limits<double>::max()));
        CHOC_EXPECT_EQ ("2.2250738585072014e-308",  choc::text::floatToString (std::numeric_limits<double>::min()));
        CHOC_EXPECT_EQ ("-1.7976931348623157e308",  choc::text::floatToString (std::numeric_limits<double>::lowest()));
    }

    {
        CHOC_TEST (HexConversion)

        CHOC_EXPECT_EQ ("1",                choc::text::createHexString (1));
        CHOC_EXPECT_EQ ("100",              choc::text::createHexString (256));
        CHOC_EXPECT_EQ ("ffff",             choc::text::createHexString (65535));
        CHOC_EXPECT_EQ ("fffffffffffffffe", choc::text::createHexString (-2ll));
        CHOC_EXPECT_EQ ("00000001",         choc::text::createHexString (1, 8));
        CHOC_EXPECT_EQ ("00000100",         choc::text::createHexString (256, 8));
        CHOC_EXPECT_EQ ("0000ffff",         choc::text::createHexString (65535, 8));
        CHOC_EXPECT_EQ ("fffffffffffffffe", choc::text::createHexString (-2ll, 8));
    }

    {
        CHOC_TEST (Trimming)

        CHOC_EXPECT_EQ ("test", choc::text::trim ("test"));
        CHOC_EXPECT_EQ ("test", choc::text::trim (" test"));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("  test"));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("test  "));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("test "));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("  test  "));
        CHOC_EXPECT_EQ ("",     choc::text::trim ("  "));
        CHOC_EXPECT_EQ ("",     choc::text::trim (" "));
        CHOC_EXPECT_EQ ("",     choc::text::trim (""));

        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view (" test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("  test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("test  ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("test ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("  test  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string_view ("  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string_view (" ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string_view ("")));

        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string (" test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("  test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("test  ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("test ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("  test  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string ("  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string (" ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimStart ("test"));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (" test"));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart ("  test"));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart ("test  "));
        CHOC_EXPECT_EQ ("test ",  choc::text::trimStart ("test "));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart ("  test  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart ("  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (" "));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (""));

        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string_view ("test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string_view (" test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string_view ("  test")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string_view ("test  ")));
        CHOC_EXPECT_EQ ("test ",  choc::text::trimStart (std::string_view ("test ")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string_view ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string_view ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string_view (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string_view ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string ("test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string (" test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string ("  test")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string ("test  ")));
        CHOC_EXPECT_EQ ("test ",  choc::text::trimStart (std::string ("test ")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd ("test"));
        CHOC_EXPECT_EQ (" test",  choc::text::trimEnd (" test"));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd ("  test"));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd ("test  "));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd ("test "));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd ("  test  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd ("  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (" "));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (""));

        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string_view ("test")));
        CHOC_EXPECT_EQ (" test",  choc::text::trimEnd (std::string_view (" test")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string_view ("  test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string_view ("test  ")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string_view ("test ")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string_view ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string_view ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string_view (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string_view ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string ("test")));
        CHOC_EXPECT_EQ (" test",  choc::text::trimEnd (std::string (" test")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string ("  test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string ("test  ")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string ("test ")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string ("")));
    }

    {
        CHOC_TEST (EndsWith)

        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "t"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "st"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "est"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "test"));
        CHOC_EXPECT_FALSE (choc::text::endsWith ("test", "x"));
        CHOC_EXPECT_FALSE (choc::text::endsWith ("test", "ttest"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", ""));
    }

    {
        CHOC_TEST (Durations)

        CHOC_EXPECT_EQ ("0 sec", choc::text::getDurationDescription (std::chrono::milliseconds (0)));
        CHOC_EXPECT_EQ ("999 microseconds", choc::text::getDurationDescription (std::chrono::microseconds (999)));
        CHOC_EXPECT_EQ ("1 microsecond", choc::text::getDurationDescription (std::chrono::microseconds (1)));
        CHOC_EXPECT_EQ ("-1 microsecond", choc::text::getDurationDescription (std::chrono::microseconds (-1)));
        CHOC_EXPECT_EQ ("1 ms", choc::text::getDurationDescription (std::chrono::milliseconds (1)));
        CHOC_EXPECT_EQ ("-1 ms", choc::text::getDurationDescription (std::chrono::milliseconds (-1)));
        CHOC_EXPECT_EQ ("2 ms", choc::text::getDurationDescription (std::chrono::milliseconds (2)));
        CHOC_EXPECT_EQ ("1.5 ms", choc::text::getDurationDescription (std::chrono::microseconds (1495)));
        CHOC_EXPECT_EQ ("2 ms", choc::text::getDurationDescription (std::chrono::microseconds (1995)));
        CHOC_EXPECT_EQ ("1 sec", choc::text::getDurationDescription (std::chrono::seconds (1)));
        CHOC_EXPECT_EQ ("2 sec", choc::text::getDurationDescription (std::chrono::seconds (2)));
        CHOC_EXPECT_EQ ("2.3 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2300)));
        CHOC_EXPECT_EQ ("2.31 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2310)));
        CHOC_EXPECT_EQ ("2.31 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2314)));
        CHOC_EXPECT_EQ ("2.31 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2305)));
        CHOC_EXPECT_EQ ("1 min 3 sec", choc::text::getDurationDescription (std::chrono::milliseconds (63100)));
        CHOC_EXPECT_EQ ("2 min 3 sec", choc::text::getDurationDescription (std::chrono::milliseconds (123100)));
        CHOC_EXPECT_EQ ("1 hour 2 min", choc::text::getDurationDescription (std::chrono::seconds (3726)));
        CHOC_EXPECT_EQ ("-1 hour 2 min", choc::text::getDurationDescription (std::chrono::seconds (-3726)));
    }

    {
        CHOC_TEST (BytesSizes)

        CHOC_EXPECT_EQ ("0 bytes", choc::text::getByteSizeDescription (0));
        CHOC_EXPECT_EQ ("1 byte", choc::text::getByteSizeDescription (1));
        CHOC_EXPECT_EQ ("2 bytes", choc::text::getByteSizeDescription (2));
        CHOC_EXPECT_EQ ("1 KB", choc::text::getByteSizeDescription (1024));
        CHOC_EXPECT_EQ ("1.1 KB", choc::text::getByteSizeDescription (1024 + 100));
        CHOC_EXPECT_EQ ("1 MB", choc::text::getByteSizeDescription (1024 * 1024));
        CHOC_EXPECT_EQ ("1.2 MB", choc::text::getByteSizeDescription ((1024 + 200) * 1024));
        CHOC_EXPECT_EQ ("1 GB", choc::text::getByteSizeDescription (1024 * 1024 * 1024));
        CHOC_EXPECT_EQ ("1.3 GB", choc::text::getByteSizeDescription ((1024 + 300) * 1024 * 1024));
    }

    {
        CHOC_TEST (UTF8)
        {
            auto text = "line1\xd7\x90\n\xcf\x88line2\nli\xe1\xb4\x81ne3\nline4\xe1\xb4\xa8";
            choc::text::UTF8Pointer p (text);

            CHOC_EXPECT_TRUE (choc::text::findInvalidUTF8Data (text, std::string_view (text).length()) == nullptr);
            CHOC_EXPECT_EQ (2u, choc::text::findLineAndColumn (p, p.find ("ine2")).line);
            CHOC_EXPECT_EQ (3u, choc::text::findLineAndColumn (p, p.find ("ine2")).column);
            CHOC_EXPECT_TRUE (p.find ("ine4").findStartOfLine (p).startsWith ("line4"));
        }
    }
}

//==============================================================================
inline void testValues (TestProgress& progress)
{
    CHOC_CATEGORY (Values);

    {
        CHOC_TEST (Primitives)

        auto v = choc::value::createPrimitive (101);
        CHOC_EXPECT_TRUE (v.isInt32());
        CHOC_EXPECT_EQ (sizeof(int), v.getRawDataSize());
        CHOC_EXPECT_EQ (101, v.get<int>());
    }

    {
        CHOC_TEST (Defaults)

        choc::value::Value v;
        CHOC_EXPECT_TRUE (v.isVoid());
        CHOC_EXPECT_EQ (0ul, v.getRawDataSize());

        try
        {
            v.getObjectMemberAt (2);
            CHOC_FAIL ("Failed to fail");
        }
        catch (choc::value::Error& e)
        {
            CHOC_EXPECT_EQ (e.description, std::string ("This type is not an object"));
        }
    }

    {
        CHOC_TEST (ObjectCreation)

        auto v = choc::value::createObject ("test",
                                            "int32Field", (int32_t) 1,
                                            "boolField", true);
        CHOC_EXPECT_TRUE (v.isObject());
        CHOC_EXPECT_EQ (5ul, v.getRawDataSize());
        CHOC_EXPECT_EQ (2ul, v.size());

        auto member0 = v.getObjectMemberAt (0);
        auto member1 = v.getObjectMemberAt (1);

        CHOC_EXPECT_EQ (member0.name, std::string ("int32Field"));
        CHOC_EXPECT_TRUE (member0.value.isInt32());
        CHOC_EXPECT_EQ (1, member0.value.getInt32());
        CHOC_EXPECT_EQ (member1.name, std::string ("boolField"));
        CHOC_EXPECT_TRUE (member1.value.isBool());
        CHOC_EXPECT_TRUE (member1.value.getBool());

        try
        {
            v.getObjectMemberAt (2);
            CHOC_FAIL ("Failed to fail");
        }
        catch (choc::value::Error& e)
        {
            CHOC_EXPECT_EQ (e.description, std::string ("Index out of range"));
        }
    }

    {
        CHOC_TEST (Vectors)

        float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        auto v1 = choc::value::createVector (values, 6);
        auto v2 = choc::value::createVector (6, [](uint32_t index) -> float { return (float) index; });

        CHOC_EXPECT_TRUE (v1.isVector());
        CHOC_EXPECT_EQ (6ul, v1.size());
        CHOC_EXPECT_EQ (6 * sizeof(float), v1.getRawDataSize());
        CHOC_EXPECT_TRUE (v2.isVector());
        CHOC_EXPECT_EQ (6ul, v2.size());
        CHOC_EXPECT_EQ (6 * sizeof(float), v2.getRawDataSize());
    }

    {
        CHOC_TEST (UniformArray)

        auto v = choc::value::createEmptyArray();
        v.addArrayElement (1);
        v.addArrayElement (2);
        v.addArrayElement (3);
        CHOC_EXPECT_TRUE (v.getType().isUniformArray());
    }

    {
        CHOC_TEST (ComplexArray)

        auto v = choc::value::createEmptyArray();
        v.addArrayElement (1);
        v.addArrayElement (2.0);
        v.addArrayElement (3);
        v.addArrayElement (false);
        CHOC_EXPECT_FALSE (v.getType().isUniformArray());
    }

    {
        CHOC_TEST (Alignment)

        {
            auto v1 = choc::value::createEmptyArray();
            v1.addArrayElement (false);
            v1.addArrayElement (2.0);
            CHOC_EXPECT_EQ (1u, ((size_t) v1[1].getRawData()) & 3);
        }

        auto v2 = choc::value::createObject ("foo",
                                            "x", choc::value::createVector (3, [] (uint32_t) { return true; }),
                                            "y", choc::value::createVector (3, [] (uint32_t) { return true; }),
                                            "z", choc::value::createVector (3, [] (uint32_t) { return 1.0; }));

        CHOC_EXPECT_EQ (3u, ((size_t) v2["y"].getRawData()) & 3);
        CHOC_EXPECT_EQ (2u, ((size_t) v2["z"].getRawData()) & 3);
    }

    {
        CHOC_TEST (Serialisation)

        auto v = choc::value::createObject ("testObject",
                                            "int32", (int32_t) 1,
                                            "int64", (int64_t) 2,
                                            "float32", 3.0f,
                                            "float64", 4.0,
                                            "boolean", false,
                                            "string1", "string value1",
                                            "string2", std::string_view ("string value2"),
                                            "string3", std::string ("string value3"));

        {
            float floatVector[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
            auto vector = choc::value::createVector (floatVector, 6);
            v.addMember ("vector", vector);
        }

        {
            auto array = choc::value::createEmptyArray();

            array.addArrayElement (1);
            array.addArrayElement (2);
            array.addArrayElement (3);
            v.addMember ("primitiveArray", array);
        }

        {
            auto array = choc::value::createEmptyArray();

            array.addArrayElement (1);
            array.addArrayElement (2.0);
            array.addArrayElement (true);
            v.addMember ("complexArray", array);
        }

        v.addMember ("object", choc::value::createObject ("object",
                                                          "int32", choc::value::createPrimitive (1)));

        CHOC_EXPECT_EQ (90ul, v.getRawDataSize());

        struct Serialiser
        {
            choc::value::InputData getData() const  { return { data.data(), data.data() + data.size() }; }

            void write (const void* d, size_t num)
            {
                data.insert (data.end(), static_cast<const char*> (d), static_cast<const char*> (d) + num);
            }

            std::vector<uint8_t> data;
        };

        auto compare = [&] (const choc::value::ValueView& original, const choc::value::ValueView& deserialised)
        {
            std::ostringstream s1, s2;
            choc::json::writeAsJSON (s1, original);
            choc::json::writeAsJSON (s2, deserialised);
            CHOC_EXPECT_EQ (s1.str(), s2.str());
        };

        {
            Serialiser serialised;
            v.serialise (serialised);
            auto data = serialised.getData();
            auto deserialised = choc::value::Value::deserialise (data);
            compare (v, deserialised);
        }

        {
            Serialiser serialised;
            v.getView().serialise (serialised);
            auto data = serialised.getData();
            auto deserialised = choc::value::Value::deserialise (data);
            compare (v, deserialised);
        }

        {
            Serialiser serialised;
            v.serialise (serialised);
            auto data = serialised.getData();

            choc::value::ValueView::deserialise (data, [&] (const choc::value::ValueView& deserialised)
            {
                compare (v, deserialised);
            });
        }

        {
            Serialiser serialised;
            v.getView().serialise (serialised);
            auto data = serialised.getData();

            choc::value::ValueView::deserialise (data, [&] (const choc::value::ValueView& deserialised)
            {
                compare (v, deserialised);
            });
        }
    }
}

//==============================================================================
inline void testJSON (TestProgress& progress)
{
    CHOC_CATEGORY (JSON);

    {
        CHOC_TEST (ConvertDoubles)

        CHOC_EXPECT_EQ ("2.5",                      choc::json::doubleToString (2.5));
        CHOC_EXPECT_EQ ("\"NaN\"",                  choc::json::doubleToString (std::numeric_limits<double>::quiet_NaN()));
        CHOC_EXPECT_EQ ("\"Infinity\"",             choc::json::doubleToString (std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("\"-Infinity\"",            choc::json::doubleToString (-std::numeric_limits<double>::infinity()));
    }

    auto checkError = [&] (const std::string& json, const std::string& message, size_t line, size_t column)
    {
        try
        {
            auto v = choc::json::parse (json);
            CHOC_FAIL ("Should have thrown");
        }
        catch (choc::json::ParseError& e)
        {
            CHOC_EXPECT_EQ (e.message, message);
            CHOC_EXPECT_EQ (e.lineAndColumn.line, line);
            CHOC_EXPECT_EQ (e.lineAndColumn.column, column);
        }
    };

    {
        CHOC_TEST (InvalidTopLevel)

        auto json = R"(
"invalidTopLevel": 5,
)";

        checkError (json, "Expected an object or array", 2, 1);
    }

    {
        CHOC_TEST (InvalidTrailingComma)

        auto json = R"(
{
"hasTrailingComma": 5,
}
)";
        checkError (json, "Expected a name", 4, 1);
    }

    {
        CHOC_TEST (InvalidMissingValue)

        auto json = R"(
{
"hasTrailingComma": 5,
"hasMissingValue": ,
}
)";

        checkError (json, "Syntax error", 4, 20);
    }

    {
        CHOC_TEST (InvalidWrongQuotes)

        auto json = R"(
{ "field": 'value' }
)";

        checkError (json, "Syntax error", 2, 12);
    }

    {
        CHOC_TEST (ValidLongNumber)

        auto json = R"(
{
  "negativeInt64": -1234,
  "largestInt64Possible": 9223372036854775806,
  "largestInt64": 9223372036854775807,
  "veryLarge": 12345678901234567890123456789012345678901234567890,
  "veryVeryLarge": 12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890,
  "scientificNotation": 1e5,
  "booleanTrue": true,
  "booleanFalse": false
}
)";

        auto holder = choc::json::parse (json);
        auto v = holder.getView();

        CHOC_EXPECT_TRUE (v["negativeInt64"].isInt64());
        CHOC_EXPECT_EQ (-1234, v["negativeInt64"].get<int64_t>());
        CHOC_EXPECT_TRUE (v["largestInt64Possible"].isInt64());
        CHOC_EXPECT_EQ (9223372036854775806, v["largestInt64Possible"].get<int64_t>());
        CHOC_EXPECT_TRUE (v["largestInt64"].isFloat64());
        CHOC_EXPECT_NEAR (9223372036854775807.0, v["largestInt64"].get<double>(), 0.0001);
        CHOC_EXPECT_TRUE (v["veryLarge"].isFloat64());
        CHOC_EXPECT_NEAR (1.2345678901234567e+49, v["veryLarge"].get<double>(), 0.0001);
        CHOC_EXPECT_TRUE (v["veryVeryLarge"].isFloat64());
        CHOC_EXPECT_EQ (INFINITY, v["veryVeryLarge"].get<double>());
        CHOC_EXPECT_TRUE (v["scientificNotation"].isFloat64());
        CHOC_EXPECT_NEAR (1e5, v["scientificNotation"].get<double>(), 0.0001);
        CHOC_EXPECT_TRUE (v["booleanTrue"].isBool());
        CHOC_EXPECT_TRUE (v["booleanTrue"].get<bool>());
        CHOC_EXPECT_TRUE (v["booleanFalse"].isBool());
        CHOC_EXPECT_FALSE (v["booleanFalse"].get<bool>());
    }

    {
        CHOC_TEST (ValidJSON)

        auto validJSON = R"(
{
    "tests": [
        {
            "name": "test1",
            "actions": [
                {
                    "action": "standardTestSteps",
                    "deviceType": "llvm",
                    "deviceName": "llvm",
                    "codeName": "adsr",
                    "sampleRate": 44100.0,
                    "blockSize": 32,
                    "requiredSamples": 1000
                }
            ]
        },
        {
            "name": "test2",
            "actions": [
                {
                    "action": "standardTestSteps",
                    "deviceType": "cpp",
                    "deviceName": "cpp",
                    "codeName": "\u12aB",
                    "sampleRate": 44100.0,
                    "blockSize": 32,
                    "requiredSamples": 1000
                }
            ]
        }
    ]
}
)";

        auto holder = choc::json::parse (validJSON);
        auto v = holder.getView();

        // Test some aspects of the parsed JSON
        CHOC_EXPECT_TRUE (v.getType().isObject());

        CHOC_EXPECT_EQ ("test1",   v["tests"][0]["name"].get<std::string>());
        CHOC_EXPECT_NEAR (44100.0, v["tests"][0]["actions"][0]["sampleRate"].get<double>(), 0.0001);
        CHOC_EXPECT_EQ (32,        v["tests"][0]["actions"][0]["blockSize"].get<int>());

        CHOC_EXPECT_EQ ("test2", v["tests"][1]["name"].get<std::string>());
    }

    {
        CHOC_TEST (RoundTrip)
        auto json = R"({"tests": [{"name": "\"\\\n\r\t\a\b\f\u12ab", "actions": [{"action": "standardTestSteps", "deviceType": "llvm", "deviceName": "llvm", "codeName": "adsr", "sampleRate": 44100, "blockSize": 32, "requiredSamples": 1000}]}, {"name": "test2", "actions": [{"action": "standardTestSteps", "deviceType": "cpp", "deviceName": "cpp", "codeName": "adsr", "sampleRate": 44100, "array": [1, 2, 3, 4, 5], "emptyArray": [], "requiredSamples": 1000}]}]})";
        auto holder = choc::json::parse (json);
        auto output = choc::json::toString (holder.getView());
        CHOC_EXPECT_EQ (json, output);
    }
}

//==============================================================================
inline void testMIDI (TestProgress& progress)
{
    CHOC_CATEGORY (MIDI);

    {
        CHOC_TEST (FrequencyUtils)

        CHOC_EXPECT_NEAR (440.0f, choc::midi::noteNumberToFrequency (69), 0.001f);
        CHOC_EXPECT_NEAR (440.0f, choc::midi::noteNumberToFrequency (69.0f), 0.001f);
        CHOC_EXPECT_NEAR (880.0f, choc::midi::noteNumberToFrequency (69 + 12), 0.001f);
        CHOC_EXPECT_NEAR (880.0f, choc::midi::noteNumberToFrequency (69.0f + 12.0f), 0.001f);
        CHOC_EXPECT_NEAR (69.0f + 12.0f, choc::midi::frequencyToNoteNumber (880.0f), 0.001f);
    }

    {
        CHOC_TEST (ControllerNames)

        CHOC_EXPECT_EQ ("Bank Select",               choc::midi::getControllerName (0));
        CHOC_EXPECT_EQ ("Modulation Wheel (coarse)", choc::midi::getControllerName (1));
        CHOC_EXPECT_EQ ("Sound Variation",           choc::midi::getControllerName (70));
        CHOC_EXPECT_EQ ("255",                       choc::midi::getControllerName (255));
    }

    {
        CHOC_TEST (NoteNumbers)

        {
            choc::midi::NoteNumber note { 60 };

            CHOC_EXPECT_EQ (60,          note);
            CHOC_EXPECT_EQ (0,           note.getChromaticScaleIndex());
            CHOC_EXPECT_EQ (3,           note.getOctaveNumber());
            CHOC_EXPECT_NEAR (261.625f,  note.getFrequency(), 0.001f);
            CHOC_EXPECT_EQ ("C",         note.getName());
            CHOC_EXPECT_EQ ("C",         note.getNameWithSharps());
            CHOC_EXPECT_EQ ("C",         note.getNameWithFlats());
            CHOC_EXPECT_TRUE (note.isNatural());
            CHOC_EXPECT_FALSE (note.isAccidental());
            CHOC_EXPECT_EQ ("C3",        note.getNameWithOctaveNumber());
        }

        {
            choc::midi::NoteNumber note { 61 + 12 };

            CHOC_EXPECT_EQ (73,          note);
            CHOC_EXPECT_EQ (1,           note.getChromaticScaleIndex());
            CHOC_EXPECT_EQ (4,           note.getOctaveNumber());
            CHOC_EXPECT_NEAR (554.365f,  note.getFrequency(), 0.001f);
            CHOC_EXPECT_EQ ("C#",        note.getName());
            CHOC_EXPECT_EQ ("C#",        note.getNameWithSharps());
            CHOC_EXPECT_EQ ("Db",        note.getNameWithFlats());
            CHOC_EXPECT_FALSE (note.isNatural());
            CHOC_EXPECT_TRUE (note.isAccidental());
            CHOC_EXPECT_EQ ("C#4",       note.getNameWithOctaveNumber());
        }
    }

    {
        CHOC_TEST (ShortMessages)

        choc::midi::ShortMessage msg;
        CHOC_EXPECT_TRUE (msg.isNull());
    }
}

//==============================================================================
inline void testChannelSets (TestProgress& progress)
{
    CHOC_CATEGORY (ChannelSets);

    {
        CHOC_TEST (InterleavedChannelSetApplyClear)

        choc::buffer::InterleavedBuffer<float> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator (0).stride == 2);

        for (int i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, (uint32_t) i) = (float) i;
            channels.getSample (1, (uint32_t) i) = (float) -i;
        }

        setAllSamples (channels, [] (float f) { return f + 10.0f; });

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), float (i) + 10.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 10.0f - float (i));
        }

        channels.clear();

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), 0.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 0.0f);
        }
    }

    {
        CHOC_TEST (InterleavedChannelSetFrame)

        choc::buffer::InterleavedBuffer<uint32_t> channels (3, 10);
        CHOC_ASSERT (channels.getNumChannels() == 3);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator (0).stride == 3);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
        }

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            uint32_t frame[3];
            channels.getSamplesInFrame (i, frame);

            CHOC_EXPECT_EQ (i, frame[0]);
            CHOC_EXPECT_EQ (i + 100, frame[1]);
            CHOC_EXPECT_EQ (i + 200, frame[2]);
        }
    }

    {
        CHOC_TEST (InterleavedChannelSetSlice)

        choc::buffer::InterleavedBuffer<double> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator(0).stride == 2);

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
        }

        CHOC_EXPECT_EQ (channels.getSample (0, 0), 0);
        CHOC_EXPECT_EQ (channels.getSample (1, 0), 100);

        auto slice = channels.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.data.stride == 2);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 100);
        }
    }

    {
        CHOC_TEST (InterleavedChannelSetChannelSet)

        choc::buffer::InterleavedBuffer<uint32_t> channels (5, 10);
        CHOC_ASSERT (channels.getNumChannels() == 5);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator (0).stride == 5);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
            channels.getSample (3, i) = i + 300;
            channels.getSample (4, i) = i + 400;
        }

        auto set = channels.getChannelRange ({ 1, 3 });

        CHOC_ASSERT (set.getNumChannels() == 2);
        CHOC_ASSERT (set.getNumFrames() == 10);
        CHOC_ASSERT (set.data.stride == 5);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (set.getSample (0, i), i + 100);
            CHOC_EXPECT_EQ (set.getSample (1, i), i + 200);
        }

        auto slice = set.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.data.stride == 5);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i + 100);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 200);
        }
    }

    {
        CHOC_TEST (InterleavedChannelSetPackedInterleavedData)

        choc::buffer::InterleavedBuffer<uint32_t> channels (3, 10);
        CHOC_ASSERT (channels.getNumChannels() == 3);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator(0).stride == 3);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
        }

        auto iter0 = channels.getIterator (0);
        auto iter1 = channels.getIterator (1);
        auto iter2 = channels.getIterator (2);

        for (uint32_t i = 0; i < 10; ++i)
        {
            CHOC_EXPECT_EQ (*iter0++, i);
            CHOC_EXPECT_EQ (*iter1++, i + 100);
            CHOC_EXPECT_EQ (*iter2++, i + 200);
        }
    }

    {
        CHOC_TEST (DiscreteChannelSetApplyClear)

        choc::buffer::ChannelArrayBuffer<float> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator(0).stride == 1);
        CHOC_ASSERT (channels.getView().data.offset == 0);

        for (int i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, (uint32_t) i) = (float) i;
            channels.getSample (1, (uint32_t) i) = (float) -i;
        }

        setAllSamples (channels, [] (float f) { return f + 10.0f; });

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), float (i) + 10.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 10.0f - float (i));
        }

        channels.clear();

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), 0.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 0.0f);
        }
    }

    {
        CHOC_TEST (DiscreteChannelSetFrame)

        choc::buffer::ChannelArrayBuffer<uint32_t> channels (3, 10);
        CHOC_EXPECT_EQ (channels.getNumChannels(), 3UL);
        CHOC_EXPECT_EQ (channels.getNumFrames(), 10UL);
        CHOC_EXPECT_EQ (channels.getIterator(0).stride, 1UL);
        CHOC_EXPECT_EQ (channels.getView().data.offset, 0UL);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
        }

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            uint32_t frame[3];

            channels.getSamplesInFrame (i, frame);

            CHOC_EXPECT_EQ (i, frame[0]);
            CHOC_EXPECT_EQ (i + 100, frame[1]);
            CHOC_EXPECT_EQ (i + 200, frame[2]);
        }
    }

    {
        CHOC_TEST (DiscreteChannelSetSlice)

        choc::buffer::ChannelArrayBuffer<double> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator(0).stride == 1);
        CHOC_ASSERT (channels.getView().data.offset == 0);

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
        }

        CHOC_EXPECT_EQ (channels.getSample (0, 0), 0);
        CHOC_EXPECT_EQ (channels.getSample (1, 0), 100);

        auto slice = channels.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.getIterator(0).stride == 1);
        CHOC_ASSERT (slice.data.offset == 2);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 100);
        }
    }

    {
        CHOC_TEST (DiscreteChannelSetChannelSet)

        choc::buffer::ChannelArrayBuffer<uint32_t> channels (5, 10);
        CHOC_ASSERT (channels.getNumChannels() == 5);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator(0).stride == 1);
        CHOC_ASSERT (channels.getView().data.offset == 0);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
            channels.getSample (3, i) = i + 300;
            channels.getSample (4, i) = i + 400;
        }

        auto set = channels.getChannelRange ({ 1, 3 });

        CHOC_ASSERT (set.getNumChannels() == 2);
        CHOC_ASSERT (set.getNumFrames() == 10);
        CHOC_ASSERT (set.getIterator(0).stride == 1);
        CHOC_ASSERT (set.data.offset == 0);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (set.getSample (0, i), i + 100);
            CHOC_EXPECT_EQ (set.getSample (1, i), i + 200);
        }

        auto slice = set.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.getIterator(0).stride == 1);
        CHOC_ASSERT (slice.data.offset == 2);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i + 100);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 200);
        }
    }

    {
        CHOC_TEST (SetsAreSameSize)

        choc::buffer::ChannelArrayBuffer<int>   set1 (5, 10);
        choc::buffer::ChannelArrayBuffer<int>   set2 (5, 11);
        choc::buffer::ChannelArrayBuffer<int>   set3 (6, 10);
        choc::buffer::ChannelArrayBuffer<float> set4 (5, 10);
        choc::buffer::InterleavedBuffer<double> set5 (5, 10);

        CHOC_EXPECT_EQ (true,  set1.getSize() == set1.getSize());
        CHOC_EXPECT_EQ (false, set1.getSize() == set2.getSize());
        CHOC_EXPECT_EQ (false, set1.getSize() == set3.getSize());
        CHOC_EXPECT_EQ (true, set1.getSize() == set4.getSize());
        CHOC_EXPECT_EQ (true, set1.getSize() == set5.getSize());
    }

    {
        CHOC_TEST (CopyChannelSet)

        choc::buffer::ChannelArrayBuffer<float> source (5, 10);

        for (uint32_t i = 0; i < 10; i ++)
        {
            source.getSample (0, i) = (float) i;
            source.getSample (1, i) = (float) i + 100;
            source.getSample (2, i) = (float) i + 200;
            source.getSample (3, i) = (float) i + 300;
            source.getSample (4, i) = (float) i + 400;
        }

        auto slice = source.getChannelRange ({ 1, 3 }).getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.getIterator(0).stride == 1);
        CHOC_ASSERT (slice.data.offset == 2);

        choc::buffer::InterleavedBuffer<double> dest (2, 5);

        copy (dest, slice);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (dest.getSample (0, i), 2 + i + 100);
            CHOC_EXPECT_EQ (dest.getSample (1, i), 2 + i + 200);
        }
    }

    {
        CHOC_TEST (CopyChannelSetToFit)

        choc::buffer::ChannelArrayBuffer<float> source1 (1, 10),
                                                source2 (2, 10);
        source1.clear();
        source2.clear();

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            source1.getSample (0, i) = (float) i;
            source2.getSample (0, i) = (float) i;
            source2.getSample (1, i) = (float) i + 100;
        }

        choc::buffer::InterleavedBuffer<double> dest1 (1, 10);
        choc::buffer::InterleavedBuffer<double> dest2 (2, 10);
        choc::buffer::InterleavedBuffer<double> dest3 (3, 10);

        // 1 -> 1, 1-> 2, 1 -> 3
        copyRemappingChannels (dest1, source1);
        copyRemappingChannels (dest2, source1);
        copyRemappingChannels (dest3, source1);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (dest1.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest2.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest2.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);
        }

        // 2 -> 1, 2-> 2, 2 -> 3
        dest1.clear();
        dest2.clear();
        dest3.clear();

        copyRemappingChannels (dest1, source2);
        copyRemappingChannels (dest2, source2);
        copyRemappingChannels (dest3, source2);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (dest1.getSample (0, i), i);       // Channel 0
            CHOC_EXPECT_EQ (dest2.getSample (0, i), i);       // Channel 0
            CHOC_EXPECT_EQ (dest2.getSample (1, i), i + 100); // Channel 1
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);       // Channel 0
            CHOC_EXPECT_EQ (dest3.getSample (1, i), i + 100); // Channel 1
            CHOC_EXPECT_EQ (dest3.getSample (2, i), 0);       // blank
        }
    }

    {
        CHOC_TEST (CopyChannelSetAllZero)

        choc::buffer::ChannelArrayBuffer<float> source (5, 10);
        source.clear();
        CHOC_EXPECT_EQ (true, isAllZero (source));
        source.getSample (2, 6) = 1.0f;
        CHOC_EXPECT_EQ (false, isAllZero (source));
    }

    {
        CHOC_TEST (ChannelSetContentIsIdentical)

        choc::buffer::ChannelArrayBuffer<float> source (2, 10);

        for (uint32_t i = 0; i < 10; ++i)
        {
            source.getSample (0, i) = (float) i;
            source.getSample (1, i) = (float) i + 100;
        }

        choc::buffer::ChannelArrayBuffer<float> dest (2, 10);
        copy (dest, source);
        CHOC_EXPECT_EQ (true, contentMatches (source, dest));
    }
}

//==============================================================================
inline void testFIFOs (TestProgress& progress)
{
    CHOC_CATEGORY (FIFOs);

    {
        CHOC_TEST (Valid)

        choc::fifo::VariableSizeFIFO queue;
        queue.reset (10000);

        CHOC_EXPECT_EQ (false, queue.push (nullptr, 0));

        for (int i = 0; i < 100; ++i)
        {
            CHOC_EXPECT_TRUE (queue.push (&i, sizeof (i)));
        }

        int msgCount = 0;

        while (queue.pop ([&] (void* data, size_t size)
                          {
                              CHOC_EXPECT_EQ (sizeof (int), size);
                              auto i = static_cast<int*> (data);
                              CHOC_EXPECT_EQ (msgCount, *i);
                          }))
        {
            ++msgCount;
        }

        CHOC_EXPECT_EQ (100, msgCount);
    }

    {
        CHOC_TEST (Overflow)

        choc::fifo::VariableSizeFIFO queue;
        queue.reset (1000);

        std::vector<uint8_t> buffer;
        buffer.resize (1000);

        // The total available space includes the message headers, so although it looks like we've got space for 1000 bytes,
        // we actually only have space for 1000 - sizeof (MessageHeader)
        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 200));
        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 200));
        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 200));
        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 200));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 1001 - 4 * 4));

        queue.reset (200);
        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 195));
        CHOC_EXPECT_TRUE (queue.pop([&] (void*, size_t) {}));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 196));
        CHOC_EXPECT_FALSE (queue.pop([&] (void*, size_t) {}));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 197));
        CHOC_EXPECT_FALSE (queue.pop([&] (void*, size_t) {}));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 201));
        CHOC_EXPECT_FALSE (queue.pop([&] (void*, size_t) {}));
    }

    {
        CHOC_TEST (Wrapping)

        choc::fifo::VariableSizeFIFO queue;
        queue.reset (1000);

        std::vector<uint8_t> buffer;

        for (uint8_t i = 0; i < 200; ++i)
            buffer.push_back (i);

        for (int i = 1; i <= 200; i += 7)
        {
            for (int j = 0; j < 100; j++)
            {
                auto messageSize = static_cast<uint32_t> (i);
                size_t retrievedBytes = 0;

                CHOC_EXPECT_TRUE (queue.push (&buffer[0], messageSize));
                CHOC_EXPECT_TRUE (queue.push (&buffer[0], messageSize));
                CHOC_EXPECT_TRUE (queue.push (&buffer[0], messageSize));

                int msgCount = 0;

                while (queue.pop ([&] (void* data, size_t size)
                                  {
                                      CHOC_EXPECT_EQ (0, memcmp (&buffer[0], data, size));
                                      retrievedBytes += size;
                                  }))
                {
                    ++msgCount;
                }

                CHOC_EXPECT_EQ (3, msgCount);
                CHOC_EXPECT_EQ (retrievedBytes, messageSize * 3);
            }
        }
    }
}

//==============================================================================
inline void testMIDIFiles (TestProgress& progress)
{
    auto simpleHash = [] (const std::string& s)
    {
        uint64_t n = 123;

        for (auto c : s)
            n = (n * 127) + (uint8_t) c;

        return (uint64_t) n;
    };

    CHOC_CATEGORY (MIDIFile);

    {
        CHOC_TEST (SimpleFile)

        uint8_t testData[] =
             {77,84,104,100,0,0,0,6,0,1,0,2,1,0,77,84,114,107,0,0,0,25,0,255,88,4,3,3,36,8,0,255,89,2,255,1,0,255,81,3,
              12,53,0,1,255,47,0,77,84,114,107,0,0,1,40,0,192,0,0,176,121,0,0,176,64,0,0,176,91,48,0,176,10,51,0,176,7,100,0,176,
              121,0,0,176,64,0,0,176,91,48,0,176,10,51,0,176,7,100,0,255,3,5,80,105,97,110,111,0,144,62,74,64,128,62,0,0,144,64,83,64,
              128,64,0,0,144,65,86,64,128,65,0,0,144,67,92,64,128,67,0,0,144,69,93,64,128,69,0,0,144,70,89,64,128,70,0,0,144,61,69,64,
              128,61,0,0,144,70,98,64,128,70,0,0,144,69,83,64,128,69,0,0,144,67,83,64,128,67,0,0,144,65,78,64,128,65,0,0,144,64,73,64,
              128,64,0,0,144,65,86,0,144,50,76,64,128,50,0,0,144,52,82,64,128,65,0,0,128,52,0,0,144,69,95,0,144,53,84,64,128,53,0,0,
              144,55,91,64,128,69,0,0,128,55,0,0,144,74,98,0,144,57,87,64,128,57,0,0,144,58,90,64,128,74,0,0,128,58,0,0,144,67,69,0,
              144,49,73,64,128,49,0,0,144,58,87,64,128,67,0,0,128,58,0,0,144,73,98,0,144,57,81,64,128,57,0,0,144,55,83,64,128,73,0,0,
              128,55,0,0,144,76,90,0,144,53,81,64,128,53,0,0,144,52,81,64,128,76,0,0,128,52,0,1,255,47,0,0,0};

        choc::midi::File mf;

        try
        {
            mf.load (testData, sizeof (testData));
            CHOC_EXPECT_EQ (2u, mf.tracks.size());

            std::string output1, output2;

            mf.iterateEvents ([&] (const choc::midi::Message& m, double time)
                              {
                                  output1 += choc::text::floatToString (time, 3) + " " + m.toHexString() + "\n";
                              });

            for (auto& e : mf.toSequence())
                output2 += choc::text::floatToString (e.timeInSeconds, 3) + " " + e.message.toHexString() + "\n";

            // This is just a simple regression test to see whether anything changes. Update the hash number if it does.
            CHOC_EXPECT_EQ (5294939095423848520ull, simpleHash (output1));
            CHOC_EXPECT_EQ (output1, output2);
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION

        testData[51] = 0x90;

        try
        {
            mf.load (testData, sizeof (testData));
            CHOC_FAIL ("Expected a failure")
        }
        catch (...) {}
    }
}

//==============================================================================
inline void testJavascript (TestProgress& progress)
{
    CHOC_CATEGORY (Javascript);

    {
        CHOC_TEST (Basics)

        try
        {
            choc::javascript::Context context;

            CHOC_EXPECT_EQ (3, context.evaluate ("1 + 2").get<int>());
            CHOC_EXPECT_EQ (3.5, context.evaluate ("1 + 2.5").get<double>());
            CHOC_EXPECT_EQ ("hello", context.evaluate ("\"hello\"").get<std::string>());

            context.evaluate ("const x = 100; function foo() { return 200; }");
            CHOC_EXPECT_EQ (300, context.evaluate ("x + foo()").get<int>());

            context.evaluate ("const a = [1, 2, 3, [4, 5]]");
            CHOC_EXPECT_EQ ("[1, 2, 3, [4, 5]]", choc::json::toString (context.evaluate ("a")));

            context.evaluate ("const b = [1, 2, 3, { x: 123, y: 4.3, z: [2, 3], s: \"abc\" }, [4, 5], {}]");
            CHOC_EXPECT_EQ ("[1, 2, 3, {\"x\": 123, \"y\": 4.3, \"z\": [2, 3], \"s\": \"abc\"}, [4, 5], {}]", choc::json::toString (context.evaluate ("b")));
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION
    }

    {
        CHOC_TEST (Errors)

        try
        {
            choc::javascript::Context context;
            context.evaluate ("function foo() { dfgdfsg> }");
            CHOC_FAIL ("Expected an error");
        }
        catch (const choc::javascript::Error& e)
        {
            CHOC_EXPECT_EQ ("SyntaxError: parse error (line 1, end of input)\n    at [anon] (eval:1) internal\n    at [anon] (duk_js_compiler.c:3740) internal", e.message);
        }
    }

    {
        CHOC_TEST (NativeBindings)

        try
        {
            choc::javascript::Context context;

            context.registerFunction ("addUp", [] (const choc::value::Value* args, size_t numArgs) -> choc::value::Value
                                                   {
                                                       int total = 0;
                                                       for (size_t i = 0; i < numArgs; ++i)
                                                           total += args[i].get<int>();

                                                       return choc::value::createInt32 (total);
                                                   });

            context.registerFunction ("concat", [] (const choc::value::Value* args, size_t numArgs) -> choc::value::Value
                                                   {
                                                       std::string s;
                                                       for (size_t i = 0; i < numArgs; ++i)
                                                           s += args[i].get<std::string>();

                                                       return choc::value::createString (s);
                                                   });

            CHOC_EXPECT_EQ (50, context.evaluate ("addUp (11, 12, 13, 14)").get<int>());
            CHOC_EXPECT_EQ (45, context.evaluate ("addUp (11, 12, addUp (1, 1)) + addUp (5, 15)").get<int>());
            CHOC_EXPECT_EQ ("abcdef", context.evaluate ("concat (\"abc\", \"def\")").get<std::string>());
            CHOC_EXPECT_TRUE (context.evaluate ("const xx = concat (\"abc\", \"def\")").isVoid());

            choc::value::Value args[] = { choc::value::createInt32 (100), choc::value::createInt32 (200), choc::value::createInt32 (300) };
            CHOC_EXPECT_EQ (0, context.invoke ("addUp", (const choc::value::ValueView*) nullptr, 0).get<int>());
            CHOC_EXPECT_EQ (100, context.invoke ("addUp", args, 1).get<int>());
            CHOC_EXPECT_EQ (300, context.invoke ("addUp", args, 2).get<int>());
            CHOC_EXPECT_EQ (600, context.invoke ("addUp", args, 3).get<int>());
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION
    }
}

//==============================================================================
inline bool runAllTests (TestProgress& progress)
{
    testContainerUtils (progress);
    testStringUtilities (progress);
    testValues (progress);
    testJSON (progress);
    testMIDI (progress);
    testChannelSets (progress);
    testFIFOs (progress);
    testMIDIFiles (progress);
    testJavascript (progress);

    progress.printReport();
    return progress.numFails == 0;
}

}

#endif
