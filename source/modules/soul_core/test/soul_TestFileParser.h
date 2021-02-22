/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

namespace soul::test
{

/// Parses and runs a .soultest file.
class TestFileParser
{
public:
    TestFileParser();
    ~TestFileParser();

    //==============================================================================
    struct Options
    {
        Options (PerformerFactory& f) : factory (f)  { buildSettings.sampleRate = 44100.0; }

        PerformerFactory& factory;
        BuildSettings buildSettings;
        bool warningsAsErrors = false;
        size_t testToRun = 0;
        uint32_t numThreads = 0;    // 0 = leave it up to std::async to decide
        bool runDisabled = false;
    };

    //==============================================================================
    struct TestResults
    {
        TestResults() = default;
        TestResults (bool success);

        std::chrono::duration<double> totalSeconds;
        int numPasses = 0, numFails = 0, numDisabled = 0;

        void addResults (const TestResults&);
        bool hasErrors() const                  { return numFails != 0; }

        std::string toString() const;
    };

    //==============================================================================
    bool runTests (CompileMessageList& messages,
                   TestResults& results,
                   const Options& testOptions,
                   const std::string& filename,
                   std::string& code);   // NB: this modifies the code string, so a caller
                                         // can re-save the file if it wants to

    /// Add a lambda here to receive callbacks with the running status of the tests
    std::function<void(const std::string&)> logFunction;

private:
    //==============================================================================
    struct TestList;
    std::unique_ptr<TestList> testList;
};

}
