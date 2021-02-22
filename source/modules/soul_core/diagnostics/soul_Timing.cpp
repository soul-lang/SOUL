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

template <typename Duration>
static inline double toSeconds (Duration d)
{
    auto microsecs = std::chrono::duration_cast<std::chrono::microseconds> (d).count();
    return static_cast<double> (microsecs) / 1000000.0;
}

//==============================================================================
ScopedTimer::ScopedTimer (std::string desc) noexcept  : description (std::move (desc)) {}

ScopedTimer::~ScopedTimer()
{
    SOUL_LOG (description, [&] { return getElapsedTimeDescription(); });
}

std::chrono::duration<double> ScopedTimer::getElapsedSeconds() const
{
    return clock::now() - start;
}

std::string ScopedTimer::getElapsedTimeDescription() const
{
    return choc::text::getDurationDescription (getElapsedSeconds());
}

//==============================================================================
CPULoadMeasurer::CPULoadMeasurer() { reset(); }

void CPULoadMeasurer::reset()
{
    previousEnd = {};
    currentStart = {};
    currentLoad = 0;
    runningProportion = 0;
}

void CPULoadMeasurer::startMeasurement()
{
   #if ! JUCE_BELA
    currentStart = clock::now();
   #endif
}

void CPULoadMeasurer::stopMeasurement()
{
   #if ! JUCE_BELA
    auto now = clock::now();

    if (previousEnd != clock::time_point())
    {
        auto thisLength   = toSeconds (clock::now() - currentStart);
        auto totalPeriod  = toSeconds (now - previousEnd);

        auto proportion = thisLength / totalPeriod;

        const double filterAmount = 0.2;
        runningProportion += filterAmount * (proportion - runningProportion);
        currentLoad = (float) runningProportion;
    }

    previousEnd = now;
   #endif
}

float CPULoadMeasurer::getCurrentLoad() const
{
   #if JUCE_BELA
    return getBelaLoadFromString (loadFileAsString ("/proc/xenomai/sched/stat"));
   #else
    return currentLoad;
   #endif
}

float getBelaLoadFromString (const std::string& input)
{
    for (auto& l : choc::text::splitIntoLines (input, true))
    {
        if (choc::text::contains (l, "bela-audio"))
        {
            auto toks = choc::text::splitAtWhitespace (l);
            removeIf (toks, [] (const std::string& s) { return choc::text::trim (s).empty(); });

            if (toks.size() > 7)
                return (float) (std::stod (toks[7]) / 100.0f);
        }
    }

    return 0;
}


} // namespace soul
