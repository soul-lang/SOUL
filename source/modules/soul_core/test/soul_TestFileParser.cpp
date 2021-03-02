/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/


namespace soul::test
{

struct TestFileParser::TestList
{
    TestList (TestFileParser& t) : owner (t) {}
    ~TestList() = default;

    bool runSpecifiedTests (CompileMessageList& messages,
                            TestResults& testResults,
                            const Options& testOptions,
                            std::string& code)
    {
        log ("========================================================");
        std::vector<Test*> testsToRun;

        if (testOptions.testToRun == 0)
        {
            for (auto& t : tests)
                testsToRun.push_back (t.get());
        }
        else if (testOptions.testToRun > tests.size())
        {
            log ("testToRun out of range - code contains "
                   + std::to_string (tests.size())
                   + " tests, can't run test " + std::to_string (testOptions.testToRun));

            testResults.numFails++;
        }
        else
        {
            testsToRun.push_back (tests[static_cast<size_t> (testOptions.testToRun - 1)].get());
        }

        const ScopedTimer totalRunTime ({});
        std::vector<std::future<Test&>> futures;

        for (auto& test : testsToRun)
        {
            futures.emplace_back (std::async (testOptions.numThreads == 1 ? std::launch::deferred
                                                                          : std::launch::async,
                                              [this, test, &testOptions] () -> Test& { return runTest (*test, testOptions); }));
        }

        for (auto& f : futures)
        {
            auto& test = f.get();

            log (test.getTestNameAndLine());
            messages.add (test.messageList);
            log (padded (test.getTestNameAndLine(), 10) + getDescription (test.testResult)
                   + "   (" + choc::text::getDurationDescription (test.timeInSeconds) + ")");

            switch (test.testResult)
            {
                case Result::OK:        ++testResults.numPasses;   break;
                case Result::failed:    ++testResults.numFails;    break;
                case Result::disabled:  ++testResults.numDisabled; break;
            }

            code = replaceLine (code, test.startLineInFile - 1, choc::text::trimEnd (test.sectionHeaderLine));
        }

        testResults.totalSeconds = totalRunTime.getElapsedSeconds();

        log (testResults.toString());

        return testResults.numFails == 0;
    }

    bool findTestChunks (CompileMessageList& messages, const std::string& filename, const std::string& code, bool runDisabled)
    {
        const auto lines = choc::text::splitIntoLines (code, true);
        std::unique_ptr<Test> currentTest;
        auto nextLocation = CodeLocation::createFromString (filename, code);
        size_t testNumber = 0;
        std::string globalCodeChunk;
        bool isAddingToGlobalCode = false;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            nextLocation.location = choc::text::UTF8Pointer (nextLocation.location.data() + lines[i].length());
            auto trimmedLine = choc::text::trimStart (lines[i]);

            if (choc::text::startsWith (trimmedLine, "##"))
            {
                isAddingToGlobalCode = false;

                if (currentTest != nullptr && ! currentTest->lines.empty())
                {
                    currentTest->globalCodeChunk = globalCodeChunk;
                    tests.push_back (std::move (currentTest));
                }

                trimmedLine = choc::text::trimStart (trimmedLine.substr (2));

                if (runDisabled && choc::text::startsWith (trimmedLine, "disabled"))
                    trimmedLine = trimmedLine.substr (9);

                if (choc::text::startsWith (trimmedLine, "error"))
                {
                    currentTest = std::make_unique<ErrorTest>();
                }
                else if (choc::text::startsWith (trimmedLine, "compile"))
                {
                    currentTest = std::make_unique<CompileTest>();
                }
                else if (choc::text::startsWith (trimmedLine, "function"))
                {
                    currentTest = std::make_unique<FunctionTest> (choc::text::contains (trimmedLine, "ignoreWarnings"));
                }
                else if (choc::text::startsWith (trimmedLine, "console"))
                {
                    currentTest = std::make_unique<ConsoleTest>();
                }
                else if (choc::text::startsWith (trimmedLine, "processor"))
                {
                    currentTest = std::make_unique<ProcessorTest>();
                }
                else if (choc::text::startsWith (trimmedLine, "global"))
                {
                    if (! globalCodeChunk.empty())
                    {
                        messages.addError ("Only one global code chunk allowed per file", nextLocation);
                        return false;
                    }

                    isAddingToGlobalCode = true;
                    globalCodeChunk = repeatedCharacter ('\n', i + 1);
                    continue;
                }
                else if (choc::text::startsWith (trimmedLine, "disabled"))
                {
                    currentTest = std::make_unique<DisabledTest>();
                }
                else
                {
                    messages.addError ("Unknown test type", nextLocation);
                    return false;
                }

                currentTest->testList = this;
                currentTest->testNumber = ++testNumber;
                currentTest->sectionHeaderLine = lines[i];
                currentTest->startLineInFile = i + 1;
                currentTest->location = nextLocation;
            }
            else if (currentTest != nullptr)
            {
                currentTest->lines.emplace_back (lines[i]);
            }
            else if (isAddingToGlobalCode)
            {
                globalCodeChunk += lines[i];
            }
        }

        if (currentTest != nullptr && ! currentTest->lines.empty())
        {
            currentTest->globalCodeChunk = globalCodeChunk;
            tests.push_back (std::move (currentTest));
        }

        return true;
    }

    enum class Result
    {
        OK,
        failed,
        disabled
    };

    static std::string getDescription (Result r)
    {
        switch (r)
        {
            case Result::OK:        return "OK";
            case Result::failed:    return "FAILED";
            case Result::disabled:  return "DISABLED";
        }

        SOUL_ASSERT_FALSE;
        return {};
    }

    struct TestOptions
    {
        CompileMessageList& messages;
        Options options;
    };

    //==============================================================================
    struct Test
    {
        virtual ~Test() {}
        virtual Result run (TestOptions&) = 0;

        struct TestFunction
        {
            std::string name;
            CodeLocation location;
        };

        void runTest (const Options& globalOptions)
        {
            TestOptions options { messageList, globalOptions };
            const ScopedTimer thisTestTime ({});
            struct FailedParse {};
            testResult = Result::failed;

            try
            {
                CompileMessageHandler handler ([&] (const CompileMessageGroup& messageGroup)
                                               {
                                                   options.messages.add (messageGroup);

                                                   for (auto& m : messageGroup.messages)
                                                       if (m.isError())
                                                           throw FailedParse();
                                               });
                testResult = run (options);
            }
            catch (FailedParse) {}

            timeInSeconds = thisTestTime.getElapsedSeconds();
        }

        Program compile (TestOptions& options, bool useAbsoluteLineNumber)
        {
            BuildBundle build;
            build.settings = options.options.buildSettings;
            addFilesToBuild (options, build, useAbsoluteLineNumber);
            return Compiler::build (options.messages, build);
        }

        static std::string rebuildCodeFromLines (choc::span<std::string> lines, size_t initialPaddingLines)
        {
            return repeatedCharacter ('\n', initialPaddingLines) + choc::text::joinStrings (lines, {});
        }

        std::string getTestNameAndLine() const
        {
            return "Test " + std::to_string (testNumber) + " (line " + std::to_string (startLineInFile) + ")";
        }

        virtual void addFilesToBuild (TestOptions&, BuildBundle& build, bool useAbsoluteLineNumber)
        {
            if (! globalCodeChunk.empty() && ! isHeart())
                build.sourceFiles.push_back ({ location.sourceCode->filename, globalCodeChunk });

            auto code = rebuildCodeFromLines (lines, useAbsoluteLineNumber ? startLineInFile : 0);
            build.sourceFiles.push_back ({ location.sourceCode->filename, code });
        }

        TestList* testList = nullptr;
        size_t testNumber;
        std::string sectionHeaderLine;
        size_t startLineInFile = 0;
        std::vector<std::string> lines;
        std::string globalCodeChunk;
        CodeLocation location;
        CompileMessageList messageList;
        Result testResult;
        std::chrono::duration<double> timeInSeconds;

        bool isHeart() const
        {
            return ! lines.empty() && choc::text::startsWith (choc::text::trimStart (lines.front()), "#SOUL");
        }
    };

    //==============================================================================
    struct CompileTest   : public Test
    {
        Result run (TestOptions& options) override
        {
            if (loadPerformer (options) != nullptr)
                return Result::OK;

            return Result::failed;
        }

        std::unique_ptr<Performer> loadPerformer (TestOptions& options)
        {
            program = compile (options, true);

            if (options.messages.hasErrors())
                return {};

            if (program.isEmpty())
                location.throwError (Errors::emptyProgram());

            auto performer = options.options.factory.createPerformer();

            if (! performer->load (options.messages, program))
                location.throwError (Errors::failedToLoadProgram());

            return performer;
        }

        soul::Program program;
    };

    //==============================================================================
    struct FunctionTest  : public CompileTest
    {
        FunctionTest (bool shouldIgnoreWarnings = false) : ignoreWarnings (shouldIgnoreWarnings)
        {
        }

        Result run (TestOptions& options) override
        {
            options.options.buildSettings.mainProcessor = getTestProcessorName();

            if (auto performer = loadPerformer (options))
            {
                if (! performer->getInputEndpoints().empty())
                    CodeLocation().throwError (Errors::customRuntimeError ("Expected no input endpoints"));

                bool shouldStop = false;
                auto outputEndpoints = performer->getOutputEndpoints().createVector();
                auto handleConsole = getConsoleHandlerFn (*performer, outputEndpoints);
                auto handleOutput = getOutputHandlerFn (*performer, outputEndpoints, shouldStop);

                if (options.messages.hasErrors())
                    return Result::failed;

                if (! performer->link (options.messages, options.options.buildSettings, nullptr)
                     || options.messages.hasErrors()
                     || (! ignoreWarnings && options.options.warningsAsErrors && options.messages.hasWarnings()))
                    location.throwError (Errors::customRuntimeError ("Failed to prepare"));

                while (! shouldStop)
                {
                    performer->prepare (performer->getBlockSize());
                    performer->advance();
                    handleConsole();
                    handleOutput (options.messages);
                }

                if (! options.messages.hasErrors())
                    return Result::OK;
            }

            return Result::failed;
        }

        std::function<void()> getConsoleHandlerFn (Performer& performer, std::vector<EndpointDetails>& outputs)
        {
            for (auto& output : outputs)
            {
                if (output.isConsoleOutput())
                {
                    auto handle = performer.getEndpointHandle (output.endpointID);

                    auto result = [this, handle, &performer]
                    {
                        performer.iterateOutputEvents (handle, [this] (uint32_t, const choc::value::ValueView& event) -> bool
                        {
                            logConsoleMessage (event);
                            return true;
                        });
                    };

                    removeIf (outputs, [] (const soul::EndpointDetails& e) { return e.isConsoleOutput(); });
                    return result;
                }
            }

            return [] {};
        }

        std::function<void(CompileMessageList&)> getOutputHandlerFn (Performer& performer, std::vector<EndpointDetails>& outputs, bool& shouldStop)
        {
            if (outputs.size() != 1)
                CodeLocation().throwError (Errors::customRuntimeError ("Expected 1 output endpoint"));

            auto& firstOutput = outputs.front();
            auto handle = performer.getEndpointHandle (firstOutput.endpointID);

            if (isEvent (firstOutput))
            {
                return [this, handle, &performer, &shouldStop] (CompileMessageList& messages)
                {
                    performer.iterateOutputEvents (handle, [this, &shouldStop, &messages] (uint32_t, const choc::value::ValueView& value) -> bool
                    {
                        if (handleTestResult (messages, value))
                            return true;

                        shouldStop = true;
                        return false;
                    });
                };
            }

            if (isStream (firstOutput))
            {
                return [this, handle, &performer, &shouldStop] (CompileMessageList& messages)
                {
                    auto frameArray = performer.getOutputStreamFrames (handle);
                    auto numFrames = frameArray.size();

                    for (uint32_t i = 0; i < numFrames; ++i)
                    {
                        if (! handleTestResult (messages, frameArray[i]))
                        {
                            shouldStop = true;
                            break;
                        }
                    }
                };
            }

            CodeLocation().throwError (Errors::customRuntimeError ("Failed to attach test to the output endpoint"));
        }

        bool findTestFunctions (TestOptions& options, const BuildBundle& build)
        {
            auto buildCopy = build;
            buildCopy.settings.optimisationLevel = 0;

            // Compile the code under test in a namespace, and return all bool functions that take no arguments
            auto compiledProgram = Compiler::build (options.messages, buildCopy);

            if (options.messages.hasErrors())
                return false;

            if (compiledProgram.isEmpty())
                CodeLocation().throwError (Errors::emptyProgram());

            pool_ptr<Module> testsModule;

            for (auto module : compiledProgram.getModules())
            {
                if (module->shortName == "tests")
                {
                    testsModule = module;
                    break;
                }
            }

            if (testsModule)
            {
                for (auto f : testsModule->functions.get())
                    if (f->returnType.isBool() && f->parameters.empty())
                        testFunctions.push_back ({ f->name.toString(), f->location});
            }

            return true;
        }

        void addFilesToBuild (TestOptions& options, BuildBundle& build, bool useAbsoluteLineNumber) override
        {
            CompileTest::addFilesToBuild (options, build, useAbsoluteLineNumber);

            if (isHeart())
            {
                build.sourceFiles.back().content += heartFunctionCaller;
            }
            else
            {
                build.sourceFiles.back().content = "namespace tests { " + build.sourceFiles.back().content + " }";
                build.sourceFiles.push_back ({ "test_wrapper_code", soulFunctionCaller });
            }

            findTestFunctions (options, build);

            if (testFunctions.empty() && (! allowEmptyTests))
                CodeLocation().throwError (Errors::customRuntimeError ("No tests found"));

            std::ostringstream oss;

            if (isHeart())
            {
                size_t temp = 0;

                for (auto& test : testFunctions)
                {
                    oss << "            let $" << temp << " = call tests::" << test.name << "();\n"
                        << "            let $" << temp + 1 << " = call toInt ($" << temp << ");\n"
                        << "            write functionResults $" << temp + 1 << ";\n";

                    temp += 2;
                }
            }
            else
            {
                for (auto& test : testFunctions)
                    oss << "functionResults << (tests::" << test.name << "() ? 1 : 0); advance();\n";
            }

            build.sourceFiles.back().content = choc::text::replace (build.sourceFiles.back().content,
                                                                    "//FUNCTION_CALLS_GO_HERE", oss.str());
        }

        static constexpr auto soulFunctionCaller = R"(
processor TestFunctionCaller
{
    output event int functionResults;

    void run()
    {
        //FUNCTION_CALLS_GO_HERE

        loop
        {
            functionResults << -1;
            advance();
        }
    }
}
)";

        static constexpr auto heartFunctionCaller = R"(
processor TestFunctionCaller
{
    output functionResults event int32;

    function run() -> void
    {
        @block_0:
            //FUNCTION_CALLS_GO_HERE
            branch @loop;
        @loop:
            write functionResults -1;
            advance;
            branch @loop;
        @break:
            return;
    }

    function toInt (bool $b) -> int32
    {
        @block_0:
            branch_if $b ? @true : @false;
        @true:
            return 1;
        @false:
            return 0;
    }
}
)";

        void logConsoleMessage (const choc::value::ValueView& message)
        {
            consoleOutput += dump (message);
        }

        bool handleTestResult (CompileMessageList& messages, const choc::value::ValueView& value)
        {
            return value.isInt32() && handleTestResult (messages, value.getInt32());
        }

        bool handleTestResult (CompileMessageList& messages, int32_t result)
        {
            if (result > 0)
            {
                ++testResultIndex;
                return true;
            }

            if (result == 0)
            {
                if (testResultIndex < testFunctions.size())
                {
                    auto& test = testFunctions[testResultIndex];
                    messages.addError ("Test failed: " + test.name + "()", test.location);
                }
                else
                {
                    messages.addError ("Test failed: " + quoteName (getTestProcessorName()), this->location);
                }
            }

            return false;
        }

        virtual std::string getTestProcessorName()       { return "TestFunctionCaller"; }

        std::vector<TestFunction> testFunctions;
        size_t testResultIndex = 0;
        std::string consoleOutput;

        bool ignoreWarnings;
        bool allowEmptyTests = false;
        bool testFinished = false;
    };

    //==============================================================================
    struct ProcessorTest  : public FunctionTest
    {
        ProcessorTest()
        {
            allowEmptyTests = true;
        }

        std::string getTestProcessorName() override      { return "tests::test"; }
    };

    //==============================================================================
    struct ErrorTest  : public Test
    {
        Result run (TestOptions& options) override
        {
            CompileMessageList sectionErrors;
            TestOptions sectionOptions { sectionErrors, options.options };

            if (auto program = compile (sectionOptions, false))
            {
                auto performer = options.options.factory.createPerformer();

                if (performer->load (sectionErrors, program))
                {
                    for (auto i : performer->getInputEndpoints())  ignoreUnused (performer->getEndpointHandle (i.endpointID));
                    for (auto o : performer->getOutputEndpoints()) ignoreUnused (performer->getEndpointHandle (o.endpointID));

                    performer->link (sectionErrors, options.options.buildSettings, nullptr);
                }
            }

            if (sectionErrors.hasErrors())
            {
                if (sectionErrors.hasInternalCompilerErrors())
                {
                    options.messages.add (sectionErrors);
                    return Result::failed;
                }

                std::vector<std::string> errors;

                for (auto& m : sectionErrors.messages)
                    errors.push_back (m.getFullDescriptionWithoutFilename());

                auto error = choc::text::joinStrings (errors, " //// ");

                auto expectedError = choc::text::trim (sectionHeaderLine.substr (sectionHeaderLine.find ("error") + 5));

                if (error == expectedError)
                    return Result::OK;

                if (! expectedError.empty())
                {
                    compile (options, true);
                    location.throwError (Errors::customRuntimeError ("Failure test error didn't match! Truncate the line to just \"## error\" and re-run the test to re-save the file with the new message\n\n"
                                                                     "New error: " + error));
                }

                sectionHeaderLine = sectionHeaderLine.substr (0, sectionHeaderLine.find ("error") + 5)
                                      + " " + error;

                return Result::OK;
            }

            location.throwError (Errors::customRuntimeError ("Epic fail! Failure test failed to fail!"));
            return Result::failed;
        }

        void addFilesToBuild (TestOptions& options, BuildBundle& build, bool useAbsoluteLineNumber) override
        {
            Test::addFilesToBuild (options, build, useAbsoluteLineNumber);

            // Add a dummy processor that'll be chosen as the main one if there are no processors in the test code
            if (isHeart())
            {
                auto& code = build.sourceFiles.back().content;
                auto headerPos = code.find ("#SOUL");

                if (headerPos != std::string::npos)
                {
                    auto endOfLine = code.find ("\n", headerPos);

                    if (endOfLine != std::string::npos)
                        code.insert (endOfLine, " processor DummyProcessor { output dummy event int32; function run() -> void { @block_0: advance; return; } function init() -> void { @block_0: return; } } ");
                }
            }
            else
            {
                build.sourceFiles.insert (build.sourceFiles.begin(), { "test_wrapper_dummy_code",
                                                                       "processor DummyProcessor { output event int dummy; void run() { loop advance(); } }" });

                build.sourceFiles.back().content = "namespace tests { " + build.sourceFiles.back().content + " }";
            }
        }
    };

    //==============================================================================
    struct ConsoleTest  : public ProcessorTest
    {
        Result run (TestOptions& options) override
        {
            auto result = ProcessorTest::run (options);

            if (result != Result::OK)
                return result;

            auto expectedOutput = choc::text::trim (sectionHeaderLine.substr (sectionHeaderLine.find ("console") + 7));
            auto actualOutput = choc::json::addEscapeCharacters (choc::text::UTF8Pointer (consoleOutput.c_str()));

            if (actualOutput == expectedOutput)
                return Result::OK;

            if (! expectedOutput.empty())
            {
                compile (options, true);
                location.throwError (Errors::customRuntimeError ("Console output didn't match! Truncate the line to just \"## console\" and re-run the test to re-save the file with the new message\n\n"
                                                                 "New output: " + actualOutput));
            }

            sectionHeaderLine = sectionHeaderLine.substr (0, sectionHeaderLine.find ("console") + 7)
                                  + " " + actualOutput;

            return Result::OK;
        }
    };

    //==============================================================================
    struct DisabledTest  : public Test
    {
        Result run (TestOptions&) override
        {
            return Result::disabled;
        }
    };

    //==============================================================================
    TestFileParser& owner;
    std::vector<std::unique_ptr<Test>> tests;

    uint32_t numActiveThreads = 0;
    std::mutex numThreadsLock;

    void waitUntilNumberOfThreadsIsBelow (uint32_t num)
    {
        for (;;)
        {
            {
                std::lock_guard<decltype(numThreadsLock)> l (numThreadsLock);

                if (numActiveThreads < num)
                {
                    ++numActiveThreads;
                    return;
                }
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
    }

    void releaseActiveThread()
    {
        std::lock_guard<decltype(numThreadsLock)> l (numThreadsLock);
        --numActiveThreads;
    }

    Test& runTest (Test& test, const Options& testOptions)
    {
        if (testOptions.numThreads > 1)
            waitUntilNumberOfThreadsIsBelow (testOptions.numThreads);

        test.runTest (testOptions);

        if (testOptions.numThreads > 1)
            releaseActiveThread();

        return test;
    }

    void log (const std::string& s)
    {
        if (owner.logFunction != nullptr)
            owner.logFunction (s);
    }
};

//==============================================================================
TestFileParser::TestFileParser()  { testList = std::make_unique<TestList> (*this); }
TestFileParser::~TestFileParser() = default;

TestFileParser::TestResults::TestResults (bool success)
{
    if (success)
        numPasses = 1;
    else
        numFails = 1;
}

void TestFileParser::TestResults::addResults (const TestResults& r)
{
    totalSeconds += r.totalSeconds;
    numPasses += r.numPasses;
    numFails += r.numFails;
    numDisabled += r.numDisabled;
}

std::string TestFileParser::TestResults::toString() const
{
    std::ostringstream oss;

    oss << "========================================================" << std::endl
        << " Passed:      " << numPasses << std::endl
        << " Failed:      " << numFails << std::endl
        << " Disabled:    " << numDisabled << std::endl
        << "" << std::endl
        << " Total time:  " << choc::text::getDurationDescription (totalSeconds) << std::endl
        << "========================================================";

    return oss.str();
}

bool TestFileParser::runTests (CompileMessageList& messages,
                               TestResults& results,
                               const Options& testOptions,
                               const std::string& filename,
                               std::string& code)
{
    return testList->findTestChunks (messages, filename, code, testOptions.runDisabled)
            && testList->runSpecifiedTests (messages, results, testOptions, code);
}

}
