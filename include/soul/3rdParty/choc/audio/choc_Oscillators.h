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

#ifndef CHOC_OSCILLATOR_HEADER_INCLUDED
#define CHOC_OSCILLATOR_HEADER_INCLUDED

#include "choc_SampleBuffers.h"

/**
    Some very basic oscillators: sine, square-wave, sawtooth, etc.
*/
namespace choc::oscillator
{

/// Holds a phase position and an increment, and takes care of
/// wrapping and other concerns.
template <typename FloatType>
struct Phase
{
    void resetPhase() noexcept          { phase = 0; }
    void setFrequency (FloatType frequency, FloatType sampleRate);
    /// Returns the current phase before incrementing (and wrapping) it.
    FloatType next (FloatType wrapLimit) noexcept;

    FloatType phase = 0, increment = 0;
};

//==============================================================================
/// Sinewave generator
template <typename FloatType>
struct Sine
{
    using SampleType = FloatType;

    void resetPhase() noexcept                                     { phase.resetPhase(); }
    void setFrequency (FloatType frequency, FloatType sampleRate)  { phase.setFrequency (twoPi * frequency, sampleRate); }

    /// Returns the next sample
    SampleType getSample() noexcept;

private:
    Phase<FloatType> phase;
    static constexpr auto twoPi = static_cast<FloatType> (3.141592653589793238 * 2);
};

//==============================================================================
/// Sawtooth wave generator
template <typename FloatType>
struct Saw
{
    using SampleType = FloatType;

    void resetPhase() noexcept                                     { phase.resetPhase(); }
    void setFrequency (FloatType frequency, FloatType sampleRate)  { phase.setFrequency (frequency, sampleRate); }

    /// Returns the next sample
    SampleType getSample() noexcept;

private:
    Phase<FloatType> phase;
};

//==============================================================================
/// Square wave generator
template <typename FloatType>
struct Square
{
    using SampleType = FloatType;

    void resetPhase() noexcept                                     { phase.resetPhase(); }
    void setFrequency (FloatType frequency, FloatType sampleRate)  { phase.setFrequency (frequency, sampleRate); }

    /// Returns the next sample
    SampleType getSample() noexcept;

    Phase<FloatType> phase;
};

//==============================================================================
/// Triangle wave generator
template <typename FloatType>
struct Triangle
{
    using SampleType = FloatType;

    void resetPhase() noexcept                                     { square.resetPhase(); sum = static_cast<FloatType>(1); }
    void setFrequency (FloatType frequency, FloatType sampleRate)  { square.setFrequency (frequency, sampleRate); }

    /// Returns the next sample
    SampleType getSample() noexcept;

private:
    Square<FloatType> square;
    FloatType sum = 1;
};

//==============================================================================
/// Fills a choc::buffer::BufferView with a generated oscillator waveform
template <typename OscillatorType, typename BufferView>
void render (BufferView& targetView, OscillatorType& oscillator)
{
    using TargetType = typename std::remove_reference<BufferView>::type::Sample;
    setAllFrames (targetView, [&] { return static_cast<TargetType> (oscillator.getSample()); });
}

/// Fills a choc::buffer::BufferView with a generated oscillator waveform
template <typename OscillatorType, typename BufferView>
void render (BufferView& targetView, double frequency, double sampleRate)
{
    OscillatorType osc;
    osc.setFrequency (static_cast<typename OscillatorType::SampleType> (frequency),
                      static_cast<typename OscillatorType::SampleType> (sampleRate));

    using TargetType = typename std::remove_reference<BufferView>::type::Sample;
    setAllFrames (targetView, [&] { return static_cast<TargetType> (osc.getSample()); });
}


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

template <typename FloatType>
void Phase<FloatType>::setFrequency (FloatType frequency, FloatType sampleRate)
{
    CHOC_ASSERT (sampleRate > 0 && frequency >= 0);
    increment = frequency / sampleRate;
}

template <typename FloatType>
FloatType Phase<FloatType>::next (FloatType wrap) noexcept
{
    auto p = phase;
    phase += increment;

    while (phase >= wrap)
        phase -= wrap;

    return p;
}

//==============================================================================
template <typename FloatType>
static FloatType blep (FloatType phase, FloatType increment)
{
    static constexpr FloatType one = 1;

    if (phase < increment)
    {
        auto p = phase / increment;
        return (2 - p) * p - one;
    }

    if (phase > one - increment)
    {
        auto p = (phase - one) / increment;
        return (p + 2) * p + one;
    }

    return {};
}

template <typename FloatType>
FloatType Sine<FloatType>::getSample() noexcept
{
    return std::sin (phase.next (twoPi));
}

template <typename FloatType>
FloatType Saw<FloatType>::getSample() noexcept
{
    auto p = phase.next (1);
    return static_cast<FloatType>(2) * p - static_cast<FloatType>(1) - blep (p, phase.increment);
}

template <typename FloatType>
FloatType Square<FloatType>::getSample() noexcept
{
    auto p = phase.next (1);
    static constexpr auto half = static_cast<FloatType>(0.5);

    return (p < half ? static_cast<FloatType>(-1)
                     : static_cast<FloatType>(1))
              - blep (p, phase.increment)
              + blep (std::fmod (p + half, static_cast<FloatType>(1)), phase.increment);
}

template <typename FloatType>
FloatType Triangle<FloatType>::getSample() noexcept
{
    sum += static_cast<FloatType>(4) * square.phase.increment * square.getSample();
    return sum;
}


} // namespace choc::oscillator

#endif // CHOC_OSCILLATOR_HEADER_INCLUDED
