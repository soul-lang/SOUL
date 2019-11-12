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
/** RAII timer object for measuring time taken in a block. */
struct ScopedTimer  final
{
    ScopedTimer (std::string desc) noexcept;
    ~ScopedTimer();

    double getElapsedSeconds() const;
    std::string getElapsedTimeDescription() const;

    std::string description;

private:
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start = clock::now();
};

#define SOUL_LOG_TIME_OF_SCOPE(description) \
    const ScopedTimer timer_ ## __LINE__ (description);

// Helper method to read the bela audio load
float getBelaLoadFromString (const std::string& input);

//==============================================================================
/** Keeps a running count of the proportion of time spent in a block. */
struct CPULoadMeasurer
{
    CPULoadMeasurer();

    void reset();
    void startMeasurement();
    void stopMeasurement();

    float getCurrentLoad() const;

private:
    using clock = std::chrono::high_resolution_clock;

    clock::time_point previousEnd, currentStart;
    std::atomic<float> currentLoad;
    double runningProportion = 0;
};


} // namespace soul
