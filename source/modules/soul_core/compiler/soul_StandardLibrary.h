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
const char* getSystemModuleCode (std::string_view moduleName);

inline CodeLocation getDefaultLibraryCode()
{
    return SourceCodeText::createInternal ("SOUL built-in library",
                                           getSystemModuleCode ("soul.intrinsics"));
}

inline CodeLocation getSystemModule (std::string_view moduleName)
{
    if (auto code = getSystemModuleCode (moduleName))
        return SourceCodeText::createInternal (std::string (moduleName), code);

    return {};
}

template <typename HandleMatch>
static void matchBuiltInConstant (Identifier name, HandleMatch&& handleMatch)
{
    if (name == "pi")     { handleMatch (Value (pi)); return; }
    if (name == "twoPi")  { handleMatch (Value (twoPi)); return; }
    if (name == "nan")    { handleMatch (Value (std::numeric_limits<float>::quiet_NaN())); return; }
    if (name == "inf")    { handleMatch (Value (std::numeric_limits<float>::infinity())); return; }
}

inline const char* getSystemModuleCode (std::string_view moduleName)
{
    // To allow the soul library files to be managed as normal files in the repo,
    // there's a script tools/rebuild_library_code that embeds all their content
    // in this code chunk below. This obviously needs to be run when the files change.

    // BEGIN_INCLUDED_LIBRARY_CODE
    if (moduleName == "soul.complex")  return R"soul_code(
namespace soul::complex_lib::imp (using FloatType, int vectorSize)
{
    namespace complex_element = imp (FloatType, 1);

    struct ComplexType
    {
        FloatType<vectorSize> real, imag;
    }

    ComplexType negate (ComplexType v)
    {
        return ComplexType (-v.real, -v.imag);
    }

    ComplexType conj (ComplexType v)
    {
        return ComplexType (v.real, -v.imag);
    }

    ComplexType add (ComplexType left, ComplexType right)
    {
        return ComplexType (left.real + right.real,
                            left.imag + right.imag);
    }

    ComplexType subtract (ComplexType left, ComplexType right)
    {
        return ComplexType (left.real - right.real,
                            left.imag - right.imag);
    }

    ComplexType multiply (ComplexType left, ComplexType right)
    {
        return ComplexType (left.real * right.real - left.imag * right.imag,
                            left.real * right.imag + left.imag * right.real);
    }

    ComplexType divide (ComplexType left, ComplexType right)
    {
        let c = right.conj();
        let result = multiply (left, c);
        let scale = multiply (right, c).real;

        return ComplexType (result.real / scale,
                            result.imag / scale);
    }

    bool<vectorSize> equals (ComplexType left, ComplexType right)
    {
        var realComparison = left.real == right.real;
        let imagComparison = left.imag == right.imag;

        for (wrap<vectorSize> i)
            realComparison[i] = realComparison[i] && imagComparison[i];

        return realComparison;
    }

    bool<vectorSize> notEquals (ComplexType left, ComplexType right)
    {
        var r = equals (left, right);

        for (wrap<vectorSize> i)
            r[i] = ! r[i];

        return r;
    }

    complex_element::ComplexType getElement (ComplexType c, int element)
    {
        return complex_element::ComplexType (c.real.at (element),
                                            c.imag.at (element));
    }

)soul_code"
R"soul_code(

    complex_element::ComplexType setElement (ComplexType& c, int element, complex_element::ComplexType value)
    {
        c.real.at (element) = value.real;
        c.imag.at (element) = value.imag;

        return value;
    }
}
)soul_code";

    if (moduleName == "soul.noise")  return R"soul_code(
/** Title: RNGs and noise-generators

    This module contains a range of simple RNG and noise generating functions and processors.
*/

/** This namespace contains some random number generation helpers.
    We're assuming that nobody's going to be using these RNGs for security-critical cryptographic
    applications. All the algorithms here are chosen to be fast, and definitely not cryptographically
    strong!
*/
namespace soul::random
{
    /// State for a Park-Miller random number generator.
    struct RandomNumberState
    {
        /** The current seed.

            **Top tip:** when generating a seed, you might want to use the `processor.id` constant,
            to make sure that each instance of a processor has a differently-seeded RNG. If you
            want the RNG to be different each time the program runs, you could also throw the
            `processor.session` constant into the mix too.

            For example:

            ```soul
            processor MyProcessorUsingRandomNumbers
            {
                ...etc...

                soul::random::RandomNumberState rng;

                let mySeed = 12345; // Whenever seeding a RNG, you should pick a 'salt' value
                                    // that's as unique as possible

                void run()
                {
                    // Initialising it like this will produce the same sequence of numbers for
                    // every instance of this processor:
                    rng.reset (mySeed);

                    // This will result in each instance of this processor generating a different
                    // sequence, but each time you load and run the program, you may get the
                    // same sequences as the last run:
                    rng.reset (processor.id + mySeed);

                    // This will result in each instance of this processor generating the same
                    // sequence, but it will be different each time you load and run the program:
)soul_code"
R"soul_code(

                    rng.reset (processor.session + mySeed);

                    // This will result in each instance of this processor producing a different
                    // sequence, and each will also be different each time you load and run the program:
                    rng.reset (processor.session + processor.id + mySeed);
                }
            }
            ```
        */
        int64 seed;
    }

    /// Resets an RNG state object with the given seed value.
    void reset (RandomNumberState& state, int64 seed)
    {
        state.seed = seed;
    }

    /// Returns the next number in the full 32-bit integer range.
    int32 getNextInt32 (RandomNumberState& state)
    {
        let s = (state.seed * 48271) % 0x7fffffff;
        state.seed = s + 1;
        return int32 (s);
    }

    /// Advances the given RNG state and returns a value 0 to 1.
    float getNextUnipolar (RandomNumberState& state)
    {
        return float (getNextInt32 (state)) * (1.0f / 2147483647.0f);
    }

    /// Advances the given RNG state and returns a value -1 to 1.
    float getNextBipolar (RandomNumberState& state)
    {
        return (float (getNextInt32 (state)) * (2.0f / 2147483647.0f)) - 1.0f;
    }
}

/// This namespace contains generators for various flavours of noise.
namespace soul::noise
{
    /// White noise generator
    processor White
    {
        output stream float out;

        void run()
        {
            var rng = random::RandomNumberState (processor.id + 10);

            loop
            {
                out << rng.getNextBipolar();
                advance();
            }
        }
    }

    /// Brown noise generator
    processor Brown
    {
        output stream float out;

        void run()
        {
            let limit = 32.0f;
            float runningTotal;
            var rng = random::RandomNumberState (processor.id + 20);

            loop
            {
                let white = rng.getNextBipolar();
                runningTotal += white;

)soul_code"
R"soul_code(

                if (runningTotal > limit || runningTotal < -limit)
                    runningTotal -= white;

                runningTotal *= 0.998f;
                out << runningTotal * (1.0f / limit);
                advance();
            }
        }
    }

    /// Pink noise generator
    processor Pink
    {
        output stream float out;

        void run()
        {
            let pinkBits = 12;
            int counter;
            float[pinkBits] values;
            float total;
            var rng = random::RandomNumberState (processor.id + 30);

            loop
            {
                let white = rng.getNextBipolar();
                ++counter;

                for (int bit = 0; bit < pinkBits; ++bit)
                {
                    if (((counter >> bit) & 1) != 0)
                    {
                        let index = wrap<pinkBits> (bit);
                        total -= values[index];
                        values[index] = white;
                        total += white;
                        break;
                    }
                }

                out << total * (1.0f / float (pinkBits - 1));
                advance();
            }
        }
    }
}
)soul_code";

    if (moduleName == "soul.audio.utils")  return R"soul_code(
/** Title: Miscellaneous audio utilities

    This module is a collection of commonly-used audio helper functions and types.
*/

namespace soul
{
    //==============================================================================
    /// Converts a decibel level to a gain factor.
    float32 dBtoGain (float32 decibels)   { return decibels > -100.0f ? pow (10.0f, decibels * 0.05f) : 0.0f; }
    /// Converts a decibel level to a gain factor.
    float64 dBtoGain (float64 decibels)   { return decibels > -100.0  ? pow (10.0,  decibels * 0.05)  : 0.0; }

    /// Converts a gain factor to a decibel level.
    float32 gainTodB (float32 gain)       { return gain > 0 ? log10 (gain) * 20.0f : -100.0f; }
    /// Converts a gain factor to a decibel level.
    float64 gainTodB (float64 gain)       { return gain > 0 ? log10 (gain) * 20.0  : -100.0; }

    /// Converts a MIDI note (usually in the range 0-127) to a frequency in Hz.
    float32 noteNumberToFrequency (int note)            { return 440.0f * pow (2.0f, (note - 69) * (1.0f / 12.0f)); }
    /// Converts a MIDI note (usually in the range 0-127) to a frequency in Hz.
    float32 noteNumberToFrequency (float32 note)        { return 440.0f * pow (2.0f, (note - 69.0f) * (1.0f / 12.0f)); }
    /// Converts a frequency in Hz to an equivalent MIDI note number.
    float32 frequencyToNoteNumber (float32 frequency)   { return 69.0f + (12.0f / log (2.0f)) * log (frequency * (1.0f / 440.0f)); }

    /// Returns the ratio by which a sample's playback must be sped-up in order to map
    /// from its native sample-rate and note to a target sample-rate and note.
    float64 getSpeedRatioForPitchedSample (float64 sourceSampleRate, float32 sourceMIDINote,
                                           float64 targetSampleRate, float32 targetMIDINote)
    {
        return (sourceSampleRate * pow (2.0f, targetMIDINote * (1.0f / 12.0f)))
             / (targetSampleRate * pow (2.0f, sourceMIDINote * (1.0f / 12.0f)));
    }

)soul_code"
R"soul_code(

    /// Returns the coefficient for a filter pole, based on a t60 decay time in seconds.
    float64 tau2pole (float64 t60, float64 sampleRate)   { return pow (0.001,  1.0  / (t60 * sampleRate)); }
    /// Returns the coefficient for a filter pole, based on a t60 decay time in seconds.
    float32 tau2pole (float32 t60, float32 sampleRate)   { return pow (0.001f, 1.0f / (t60 * sampleRate)); }

    /// Returns the t60 decay time in seconds for a filter pole coefficient in the range 0 to 1.
    float64 pole2tau (float64 pole, float64 sampleRate)   { return -1.0 / (log (pole) * sampleRate); }
    /// Returns the t60 decay time in seconds for a filter pole coefficient in the range 0 to 1.
    float32 pole2tau (float32 pole, float32 sampleRate)   { return -1.0f / (log (pole) * sampleRate); }
}

//==============================================================================
/// This namespace contains some handy stuctures to use when declaring `external`
/// variables which are going to be loaded with data from audio files.
namespace soul::audio_samples
{
    /// An `external` variable declared with the type `soul::audio_samples::Mono`
    /// can be loaded with monoised data from an audio file.
    struct Mono
    {
        float[] frames;
        float64 sampleRate;
    }

    /// An `external` variable declared with the type `soul::audio_samples::Stereo`
    /// can be loaded with stereo data from an audio file.
    struct Stereo
    {
        float<2>[] frames;
        float64 sampleRate;
    }
}

/// This namespace contains various pan-related helper functions
namespace soul::pan_law
{
    /// Applies a simple linear pan law to convert a pan position (-1.0 to 1.0) to
    /// a (left, right) pair of stereo gains.
    float<2> linear (float pan)
    {
        return (1.0f - pan,
                1.0f + pan);
    }

    /// Applies a 3dB-centre pan law to convert a pan position (-1.0 to 1.0) to
    /// a (left, right) pair of stereo gains.
    float<2> centre3dB (float pan)
    {
)soul_code"
R"soul_code(

        let quarterPi = float (pi / 4);

        return (sin ((1.0f - pan) * quarterPi),
                sin ((1.0f + pan) * quarterPi));
    }
}
)soul_code";

    if (moduleName == "soul.mixing")  return R"soul_code(
/** Title: Mix and gain control utilities

    This file provides a set of processors for common tasks like mixing together sources,
    applying fixed gains, or applying envelope shapes.
*/

//==============================================================================
/// This namespace contains a set of processors for common tasks like mixing together sources.
namespace soul::mixers
{
    //==============================================================================
    /// Simple processor which adds two sources together using fixed gains for each source.
    processor FixedSum (using SampleType, float gain1, float gain2)
    {
        input  stream SampleType in1, in2;
        output stream SampleType out;

        void run()
        {
            loop
            {
                out << in1 * gain1 + in2 * gain2;
                advance();
            }
        }
    }

    //==============================================================================
    /// Simple processor which adds two sources together using streams to control the
    /// gains to apply to each source.
    processor DynamicSum (using SampleType)
    {
        input  stream SampleType in1, in2;
        input  stream float gain1, gain2;
        output stream SampleType out;

        void run()
        {
            loop
            {
                out << in1 * gain1 + in2 * gain2;
                advance();
            }
        }
    }

    //==============================================================================
    /** Simple processor which mixes together two sources, using a stream of values
        to indicate the ratio.

        The mixRange constant allows you to set the range of values that will be passed
        in the mix stream, so e.g. mixRange = 1.0 means that mix will be in the range 0 to 1,
        and mixRange = 100 means the values will be 0 to 100.
        The mix stream is expected to contain values between 0 and mixRange,
)soul_code"
R"soul_code(

        where mix = 0 produces 100% in1, and mix = mixRange produces 100% in2.
    */
    processor DynamicMix (using SampleType, float mixRange)
    {
        input  stream SampleType in1, in2;
        input  stream float mix;
        output stream SampleType out;

        void run()
        {
            loop
            {
                out << lerp (in1, in2, mix * (1.0f / mixRange));
                advance();
            }
        }
    }
}

//==============================================================================
/// Utility processors for common tasks like applying gain in various ways.
namespace soul::gain
{
    //==============================================================================
    /// Simple processor which applies a fixed gain to a signal.
    processor FixedGain (using SampleType, float fixedGain)
    {
        input  stream SampleType in;
        output stream SampleType out;

        void run()
        {
            loop
            {
                out << in * fixedGain;
                advance();
            }
        }
    }

    //==============================================================================
    /// Simple processor which applies a changeable gain level to a signal.
    processor DynamicGain (using SampleType)
    {
        input  stream SampleType in;
        output stream SampleType out;
        input  stream float gain;

        void run()
        {
            loop
            {
                out << in * gain;
                advance();
            }
        }
    }

    //==============================================================================
    /// Converts an input event parameter in decibels to a smoothed stream of
    /// raw gain levels.
    processor SmoothedGainParameter (float slewRateSeconds)
    {
        input event float volume   [[ name: "Volume", unit: "dB", min: -85, max: 6 ]];
        output stream float gain;

        event volume (float targetDB)
        {
            targetGain = soul::dBtoGain (targetDB);
)soul_code"
R"soul_code(

            let maxDelta = float (processor.period) / slewRateSeconds;
            remainingRampSamples = max (1, int (abs (targetGain - currentGain) / maxDelta));
            increment = (targetGain - currentGain) / remainingRampSamples;
        }

        float targetGain, currentGain, increment;
        int remainingRampSamples;

        void run()
        {
            loop
            {
                if (remainingRampSamples != 0)
                {
                    if (--remainingRampSamples == 0)
                        currentGain = targetGain;
                    else
                        currentGain += increment;
                }

                gain << currentGain;
                advance();
            }
        }
    }

    //==============================================================================
    /// A graph that combines DynamicGain and SmoothedGainParameter
    graph SmoothedGain (/*using SampleType = float32,*/float slewRateSeconds = 0.1f)
    {
        input stream float32 in;
        output stream float32 out;
        input gainParameter.volume volume;

        let
        {
            gainProcessor = soul::gain::DynamicGain (float32);
            gainParameter = soul::gain::SmoothedGainParameter (slewRateSeconds);
        }

        connection
        {
            gainParameter.gain -> gainProcessor.gain;
            in -> gainProcessor.in;
            gainProcessor.out -> out;
        }
    }
}

//==============================================================================
/// Generators for common envelope shapes.
namespace soul::envelope
{
    /** Creates an envelope which applies convex attack and release curves based
        on a stream of NoteOn and NoteOff events.

        The envelope implements fixed-length attack and release ramps where the hold
        level is based on the velocity of the triggering NoteOn event, multiplied
        by the holdLevelMultiplier parameter.
    */
    processor FixedAttackReleaseEnvelope (float holdLevelMultiplier,
)soul_code"
R"soul_code(

                                          float attackTimeSeconds,
                                          float releaseTimeSeconds)
    {
        input event (soul::note_events::NoteOn,
                     soul::note_events::NoteOff) noteIn;

        output stream float levelOut;

        event noteIn (soul::note_events::NoteOn e)      { active = true; targetLevel = e.velocity; }
        event noteIn (soul::note_events::NoteOff e)     { active = false; }

        bool active = false;
        float targetLevel;

        void run()
        {
            let silenceThreshold = 0.00001f;

            loop
            {
                // Waiting for note-on
                while (! active)
                    advance();

                float level;

                // Attacking
                if const (attackTimeSeconds <= 0)
                {
                    level = targetLevel;
                }
                else
                {
                    let attackSamples = int (float (processor.frequency) * attackTimeSeconds);
                    let attackMultiplier = float (pow (2.0f, -1.0f / attackSamples) * pow (targetLevel + 2.0f, 1.0f / attackSamples));

                    for (var attackLevel = 2.0f; active && level < targetLevel; attackLevel *= attackMultiplier)
                    {
                        level = attackLevel - 2.0f;
                        levelOut << level;
                        advance();
                    }
                }

                // Sustaining
                while (active)
                {
                    levelOut << level;
                    advance();
                }

                // Releasing
                if const (releaseTimeSeconds > 0)
                {
                    let releaseMultiplier = pow (0.0001f, float (processor.period) / releaseTimeSeconds);

                    while (! active && level > silenceThreshold)
                    {
                        levelOut << level;
)soul_code"
R"soul_code(

                        level *= releaseMultiplier;
                        advance();
                    }
                }
            }
        }
    }
}
)soul_code";

    if (moduleName == "soul.intrinsics")  return R"soul_code(
/** Title: Intrinsic functions
*/

/**
    The `intrinsics` namespace contains low-level, commonly-used functions.

    This is a special-case namespace, as the functions inside it can be used without needing to
    specify their namespace (i.e. you can just write `abs(x)`, no need to write
    `soul::intrinsics::abs(x)`).

    Many of these functions have reference implementations defined in this file, but which
    will be substituted for more optimal implementations by a JIT engine if possible.
    The `[[intrin]]` annotation is used as a hint to the performer back-end that this function
    should be replaced by a native implementation if one is available. (Note that some of the
    functions here have non-functional implementations and rely on the performer to use a native
    implementation for them to work at all).
*/
namespace soul::intrinsics
{
    /// Familiar abs() function, accepting most scalar types.
    T.removeReference abs<T> (T n)                         [[intrin: "abs"]]       { static_assert (T.isScalar, "abs() only works with scalar types"); return n < 0 ? -n : n; }
    /// Returns the greater of two scalar values.
    T.removeReference max<T>       (T a, T b)              [[intrin: "max"]]       { static_assert (T.isScalar, "max() only works with scalar types"); return a > b ? a : b; }
    /// Returns the lesser of two scalar values.
    T.removeReference min<T>       (T a, T b)              [[intrin: "min"]]       { static_assert (T.isScalar, "min() only works with scalar types"); return a < b ? a : b; }
    /// Returns the lesser of two scalar values.
    int32 min                      (int32 a, int32 b)      [[intrin: "min"]]       { return a < b ? a : b; }

    /// Clamps a scalar value to the nearest value within a given range.
    T.removeReference clamp<T>     (T n, T low, T high)    [[intrin: "clamp"]]     { static_assert (T.isScalar, "clamp() only works with scalar types"); return n < low ? low : (n > high ? high : n); }
)soul_code"
R"soul_code(

    /// Performs a negative-number-aware modulo operation to wrap a number to a zero-based range.
    T.removeReference wrap<T>      (T n, T range)          [[intrin: "wrap"]]      { static_assert (T.isScalar, "wrap() only works with scalar types");  if (range == 0) return 0; let x = n % range; if (x < 0) return x + range; return x; }
    /// Performs a negative-number-aware integer modulo operation.
    int32 wrap                     (int32 n, int32 range)  [[intrin: "wrap"]]      { if (range == 0) return 0; let x = n % range; if (x < 0) return x + range; return x; }
    /// Performs a C++-compatible floor function on a scalar floating point value.
    T.removeReference floor<T>     (T n)                   [[intrin: "floor"]]     { static_assert (T.isScalar && T.primitiveType.isFloat, "floor() only works with scalar floating point types");     let r = T (int64 (n)); return (r == n) ? n : (n >= 0 ? r : r - 1); }
    /// Performs a C++-compatible ceil function on a scalar floating point value.
    T.removeReference ceil<T>      (T n)                   [[intrin: "ceil"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "ceil() only works with scalar floating point types");      let r = T (int64 (n)); return (r == n) ? n : (n >= 0 ? r + 1 : r); }
    /// Returns a linearly-interpolated value between two scalar values.
    T.removeReference lerp<T>      (T start, T stop, T amount)                     { static_assert (T.isScalar && T.primitiveType.isFloat, "lerp() only works with scalar floating point types");      return start + (stop - start) * amount; }

    /// Performs a C++-compatible fmod function on some scalar floating point values.
    T.removeReference fmod<T>      (T x, T y)              [[intrin: "fmod"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "fmod() only works with scalar floating point types");      return x - (y * T (int (x / y))); }
    /// Performs a C++-compatible remainder function on some scalar floating point values.
)soul_code"
R"soul_code(

    T.removeReference remainder<T> (T x, T y)              [[intrin: "remainder"]] { static_assert (T.isScalar && T.primitiveType.isFloat, "remainder() only works with scalar floating point types"); return x - (y * T (int (0.5f + x / y))); }
    /// Returns the square root of a scalar floating point value.
    T.removeReference sqrt<T>      (T n)                   [[intrin: "sqrt"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "sqrt() only works with scalar floating point types");      return T(); }
    /// Raises a scalar floating point value to the given power.
    T.removeReference pow<T>       (T a, T b)              [[intrin: "pow"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "pow() only works with scalar floating point types");       return T(); }
    /// Returns the exponential of a scalar floating point value.
    T.removeReference exp<T>       (T n)                   [[intrin: "exp"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "exp() only works with scalar floating point types");       return T(); }
    /// Returns the log-e of a scalar floating point value.
    T.removeReference log<T>       (T n)                   [[intrin: "log"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "log() only works with scalar floating point types");       return T(); }
    /// Returns the log 10 of a scalar floating point value.
    T.removeReference log10<T>     (T n)                   [[intrin: "log10"]]     { static_assert (T.isScalar && T.primitiveType.isFloat, "log10() only works with scalar floating point types");     return T(); }

    /// Rounds a floating point number up or down to the nearest integer.
    int32 roundToInt (float32 n)    [[intrin: "roundToInt"]]                       { return int32 (n + (n < 0 ? -0.5f : 0.5f)); }
    /// Rounds a floating point number up or down to the nearest integer.
    int64 roundToInt (float64 n)    [[intrin: "roundToInt"]]                       { return int64 (n + (n < 0 ? -0.5 : 0.5)); }

)soul_code"
R"soul_code(

    /// Returns true if the floating point argument is a NaN.
    bool isnan<T> (T n)  [[intrin: "isnan"]]       { static_assert (T.isPrimitive && T.primitiveType.isFloat, "isnan() only works with floating point types"); return false; }
    /// Returns true if the floating point argument is an INF.
    bool isinf<T> (T n)  [[intrin: "isinf"]]       { static_assert (T.isPrimitive && T.primitiveType.isFloat, "isinf() only works with floating point types"); return false; }

    /// Adds a delta to a value, and returns the resulting value modulo PI/2.
    /// A typical use-case for this is in incrementing the phase of an oscillator.
    ///
    T.removeReference addModulo2Pi<T> (T value, T increment) [[intrin: "addModulo2Pi"]]
    {
        value += increment;
        let pi2 = T (twoPi);

        if (value >= pi2)
        {
            if (value >= pi2 * 2)
                return value % pi2;

            return value - pi2;
        }

        return value < 0 ? value % pi2 + pi2 : value;
    }

    /// Returns the sum of an array or vector of scalar values.
    T.elementType sum<T> (T t)
    {
        static_assert (T.isArray || T.isVector, "sum() only works with arrays or vectors");
        static_assert (T.elementType.isScalar, "sum() only works with arrays of scalar values");

        if const (T.isVector && t.size > 8)
        {
            T.elementType total;
            let n = t.size / 8;

            let v = t[    0 :     n]
                  + t[    n : 2 * n]
                  + t[2 * n : 3 * n]
                  + t[3 * n : 4 * n]
                  + t[4 * n : 5 * n]
                  + t[5 * n : 6 * n]
                  + t[6 * n : 7 * n]
                  + t[7 * n : 8 * n];

            if const (n > 1)
                total = sum (v);
            else
                total = v;

            let remainder = t.size % 8;

            if const (remainder == 1)
                total += t[8 * n];

            if const (remainder > 1)
            {
                let r = t[8 * n:];
)soul_code"
R"soul_code(

                total += sum (r);
            }

            return total;
        }
        else if const (T.isFixedSizeArray || T.isVector)
        {
            var total = t[0];
            wrap<t.size> i;

            loop (t.size - 1)
                total += t[++i];

            return total;
        }
        else
        {
            if (t.size == 0)
                return T.elementType();

            var total = t[0];

            for (int i = 1; i < t.size; ++i)
                total += t[i];

            return total;
        }
    }

    /// Returns the product of an array or vector of scalar values.
    T.elementType product<T> (T t)
    {
        static_assert (T.isArray || T.isVector, "product() only works with arrays or vectors");
        static_assert (T.elementType.isScalar, "product() only works with arrays of scalar values");

        if const (T.isVector && t.size > 8)
        {
            T.elementType result;
            let n = t.size / 8;

            let v = t[    0 :     n]
                  * t[    n : 2 * n]
                  * t[2 * n : 3 * n]
                  * t[3 * n : 4 * n]
                  * t[4 * n : 5 * n]
                  * t[5 * n : 6 * n]
                  * t[6 * n : 7 * n]
                  * t[7 * n : 8 * n];

            if const (n > 1)
                result = product (v);
            else
                result = v;

            let remainder = t.size % 8;

            if const (remainder == 1)
                result *= t[8 * n];

            if const (remainder > 1)
            {
                let r = t[8 * n:];
                result *= product (r);
            }

            return result;
        }
        else if const (T.isFixedSizeArray || T.isVector)
        {
            var result = t[0];
            wrap<t.size> i;

            loop (t.size - 1)
                result *= t[++i];

            return result;
        }
        else
        {
            if (t.size == 0)
                return T.elementType();

)soul_code"
R"soul_code(

            var result = t[0];

            for (int i = 1; i < t.size; ++i)
                result *= t[i];

            return result;
        }
    }

    /// Reads an element from an array, allowing the index to be any type of floating point type.
    /// If a floating point index is used, it will be rounded down to an integer index - for an
    /// interpolated read operation, see readLinearInterpolated(). Indexes beyond the range of the
    /// array will be wrapped.
    ///
    Array.elementType read<Array, IndexType> (const Array& array, IndexType index) [[intrin: "read"]]
    {
        static_assert (Array.isArray, "read() only works with array types");
        static_assert (IndexType.isPrimitive || IndexType.isInt, "The index for read() must be a floating point or integer value");

        return array.at (int (index));
    }

    /// Reads a linearly-interpolated value from an array of some kind of scalar values (probably
    /// a float or float-vector type). Indexes beyond the range of the array will be wrapped.
    ///
    Array.elementType readLinearInterpolated<Array, IndexType> (const Array& array, IndexType index) [[intrin: "readLinearInterpolated"]]
    {
        static_assert (Array.isArray, "readLinearInterpolated() only works with array types");
        static_assert (Array.elementType.isPrimitive || array.elementType.isVector, "readLinearInterpolated() only works with arrays of primitive or vector values");
        static_assert (Array.elementType.primitiveType.isFloat, "readLinearInterpolated() only works with arrays of float values");
        static_assert (IndexType.isFloat && IndexType.isPrimitive, "The index for readLinearInterpolated() must be a floating point value");

        if (array.size == 0)
            return Array.elementType();

        let indexFloor = floor (index);
        let intIndex = int (indexFloor);
        let sample1 = array.at (intIndex);
        let sample2 = array.at (intIndex + 1);

)soul_code"
R"soul_code(

        return sample1 + (sample2 - sample1) * Array.elementType (index - indexFloor);
    }


    // NB: this is used internally, not something you'd want to call from user code
    int get_array_size<Array> (const Array& array) [[intrin: "get_array_size"]];


    // Trigonometry functions

    T sin<T>   (T n)  [[intrin: "sin"]]      { static_assert (T.isPrimitive || T.isVector, "sin() only works with floating point types");   static_assert (T.primitiveType.isFloat, "sin() only works with floating point types");   return T(); }
    T cos<T>   (T n)  [[intrin: "cos"]]      { static_assert (T.isPrimitive || T.isVector, "cos() only works with floating point types");   static_assert (T.primitiveType.isFloat, "cos() only works with floating point types");   return T(); }
    T tan<T>   (T n)  [[intrin: "tan"]]      { static_assert (T.isPrimitive || T.isVector, "tan() only works with floating point types");   static_assert (T.primitiveType.isFloat, "tan() only works with floating point types");   return sin (n) / cos (n); }
    T sinh<T>  (T n)  [[intrin: "sinh"]]     { static_assert (T.isPrimitive || T.isVector, "sinh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "sinh() only works with floating point types");  return (exp (n) - exp (-n)) / 2; }
    T cosh<T>  (T n)  [[intrin: "cosh"]]     { static_assert (T.isPrimitive || T.isVector, "cosh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "cosh() only works with floating point types");  return (exp (n) + exp (-n)) / 2; }
    T tanh<T>  (T n)  [[intrin: "tanh"]]     { static_assert (T.isPrimitive || T.isVector, "tanh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "tanh() only works with floating point types");  return sinh (n) / cosh (n); }
)soul_code"
R"soul_code(

    T asinh<T> (T n)  [[intrin: "asinh"]]    { static_assert (T.isPrimitive || T.isVector, "asinh() only works with floating point types"); static_assert (T.primitiveType.isFloat, "asinh() only works with floating point types"); return log (n + sqrt (n * n + 1)); }
    T acosh<T> (T n)  [[intrin: "acosh"]]    { static_assert (T.isPrimitive || T.isVector, "acosh() only works with floating point types"); static_assert (T.primitiveType.isFloat, "acosh() only works with floating point types"); return log (n + sqrt (n * n - 1)); }
    T atanh<T> (T n)  [[intrin: "atanh"]]    { static_assert (T.isPrimitive || T.isVector, "atanh() only works with floating point types"); static_assert (T.primitiveType.isFloat, "atanh() only works with floating point types"); return 0.5f * log ((1 + n) / (1 - n)); }
    T asin<T>  (T n)  [[intrin: "asin"]]     { static_assert (T.isPrimitive || T.isVector, "asin() only works with floating point types");  static_assert (T.primitiveType.isFloat, "asin() only works with floating point types");  return atan (n / (sqrt (1 - (n * n)))); }
    T acos<T>  (T n)  [[intrin: "acos"]]     { static_assert (T.isPrimitive || T.isVector, "acos() only works with floating point types");  static_assert (T.primitiveType.isFloat, "acos() only works with floating point types");  return atan ((sqrt (1 - (n * n))) / n); }
    T atan<T>  (T n)  [[intrin: "atan"]]     { static_assert (T.isPrimitive || T.isVector, "atan() only works with floating point types");  static_assert (T.primitiveType.isFloat, "atan() only works with floating point types");  return n < 0 ? -helpers::atanHelperPositive (-n) : helpers::atanHelperPositive (n); }

    T atan2<T> (T y, T x) [[intrin: "atan2"]]
    {
        static_assert (T.isPrimitive || T.isVector, "atan2() only works with floating point types");
        static_assert (T.primitiveType.isFloat, "atan2() only works with floating point types");

        if (x == 0)
        {
            if (y > 0) return T (pi /  2.0);
            if (y < 0) return T (pi / -2.0);

)soul_code"
R"soul_code(

            return 0;  // Undefined case: return 0
        }

        let atanYoverX = atan (y / x);

        if (x > 0)
            return atanYoverX;

        return y >= 0 ? atanYoverX + T (pi)
                      : atanYoverX - T (pi);
    }

    namespace helpers
    {
        T atanHelperPositive<T> (T n)
        {
            return n > 1.0f ? (T (pi / 2.0) - atanHelper0to1 (1 / n))
                            : atanHelper0to1 (n);
        }

        T atanHelper0to1<T> (T n)
        {
            let sqrt3 = T (sqrt (3.0));

            if (n > T (2.0 - sqrt3))
            {
                let n2 = (sqrt3 * n - 1) / (sqrt3 + n);

                if (n2 < 0)
                    return T (pi / 6.0) - atanHelperApprox (-n2);

                return T (pi / 6.0) + atanHelperApprox (n2);
            }

            return atanHelperApprox (n);
        }

        T atanHelperApprox<T> (T n)
        {
            return n - (n * n * n) / 3.0f + (n * n * n * n * n) / 5.0f;
        }
    }
}
)soul_code";

    if (moduleName == "soul.timeline")  return R"soul_code(
/** Title: Timeline event utilities

    The `timeline` namespace contains various structs and functions which are used when
    dealing with positions and tempos along a timeline.
*/

/// The `timeline` namespace contains various structs and functions which are used when
/// dealing with positions and tempos along a timeline.
namespace soul::timeline
{
    //==============================================================================
    /** Represents a simple time-signature. */
    struct TimeSignature
    {
        int numerator;     // The top number of a time-signature, e.g. the 3 of 3/4.
        int denominator;   // The bottom number of a time-signature, e.g. the 4 of 3/4.
    }

    //==============================================================================
    /** Represents a tempo in BPM. */
    struct Tempo
    {
        float bpm;  // beats per minute
    }

    //==============================================================================
    /** */
    float quarterNotesPerBeat (TimeSignature timeSig)                                       { return 4.0f / timeSig.denominator; }
    /** */
    float beatsPerQuarterNote (TimeSignature timeSig)                                       { return timeSig.denominator / 4.0f; }

    /** */
    float secondsPerBeat (Tempo tempo)                                                      { return tempo.bpm <= 0 ? 0.0f : (60.0f / tempo.bpm); }
    /** */
    float secondsPerQuarterNote (Tempo tempo, TimeSignature timeSig)                        { return tempo.secondsPerBeat() * timeSig.beatsPerQuarterNote(); }

    /** */
    float64 framesPerBeat (Tempo tempo, float64 sampleRate)                                 { return sampleRate * tempo.secondsPerBeat(); }
    /** */
    float64 framesPerQuarterNote (Tempo tempo, TimeSignature timeSig, float64 sampleRate)   { return sampleRate * tempo.secondsPerQuarterNote (timeSig); }

    //==============================================================================
)soul_code"
R"soul_code(

    /** Represents the state of a host which can play timeline-based material. */
    struct TransportState
    {
        /** In the absence of enums, the valid values for the state are:
            0 = stopped, 1 = playing, 2 = recording.
        */
        int state;
    }

    /** */
    bool isStopped   (TransportState t)       { return t.state == 0; }
    /** */
    bool isPlaying   (TransportState t)       { return t.state == 1; }
    /** */
    bool isRecording (TransportState t)       { return t.state == 2; }

    //==============================================================================
    /** Represents a position along a timeline, in terms of frames and also (where
        appropriate) quarter notes.
    */
    struct Position
    {
        /** A number of frames from the start of the timeline. */
        int64 currentFrame;

        /** The number of quarter-notes since the beginning of the timeline.
            A host may not have a meaningful value for this, so it may just be 0.
            Bear in mind that a timeline may contain multiple changes of tempo and
            time-signature, so this value will not necessarily keep increasing at
            a constant rate.
        */
        float64 currentQuarterNote;

        /** The number of quarter-notes from the beginning of the timeline to the
            start of the current bar.
            A host may not have a meaningful value for this, so it may just be 0.
            You can subtract this from currentQuarterNote to find out how which
            quarter-note the position represents within the current bar.
        */
        float64 lastBarStartQuarterNote;
    }
}
)soul_code";

    if (moduleName == "soul.filters")  return R"soul_code(
/** Title: Filters

    This module contains a collection of filter implementations, including more traditional
    biquad based filters and TPT variants, which are more suitable for modulation.

    Notes:
    - Filter coefficients are typically labelled a and b. Different literature uses different
      conventions for what a and b mean. This code uses the following:
        * `b`: feed-forward, numerator, zero, FIR coefficients.
        * `a`: feed-back, denominator, pole, IIR coefficients.
    - Although each filter comes with a Processor, they are designed so that their state,
      coefficients and functions can be used independently.
    - Each filter has a `State` struct and a `Coeffs` struct.
    - `process()` functions generate an output y for an input x.
    - `update()` functions take high level parameters such as mode, frequency, Q, mode and
      update the filter coefficients.
    - `reset()` functions reset the filter state/histories to 0.
    - `clear()` functions clears filter coefficients.
    - The filters are designed for channel-wise vectorisation, which can be achieved by
      specialising the filter's namespace with a vector type.
    - Some filters are suitable for modulation, others are not, check the comments at the top
      of the namespace.
*/

namespace soul::filters (using SampleType = float32,
                         using CoeffType  = float64,
                         int updateInterval = 16)
{
    // Processor parameter min/max/defaults
    let minFreqHz = 5.0f;
    let maxFreqHz = 22000.0f;
    let defaultFreqHz = 1000.0f;
    let defaultQuality = 0.707107f; // 1.0 / sqrt (2.0) butterworth response
    let defaultGain = 0.f;

    /** The frequency upper-bound for update functions.
        (Just below nyquist, e.g 0.49 * 44100 = ~22kHz).
    */
    let normalisedFreqLimit = 0.49;

    //==============================================================================
    /** Highpass filter for removing DC offset.

            y[n] = x[n] - x[n-1] + b0 * y[n-1]

)soul_code"
R"soul_code(

        https://ccrma.stanford.edu/~jos/fp/DC_Blocker.html
    */
    namespace dc_blocker
    {
        /** Holds a set of filter coefficients. */
        struct Coeffs
        {
            CoeffType b1;
            CoeffType a0;
        }

        /** Holds the filter state. */
        struct State
        {
            SampleType[1] x;
            SampleType[1] y;
        }

        /** Resets a filter state. */
        void reset (State& s)
        {
            s.x[0] = SampleType();
            s.y[0] = SampleType();
        }

        /** Updates a set of coefficients for the given settings. */
        void update (Coeffs& c, float64 sampleRate, float64 freqHz)
        {
            let w = pi * freqHz / sampleRate;
            c.a0 = CoeffType (1.0 / (1.0 + w));
            c.b1 = CoeffType ((1.0 - w) * c.a0);
        }

        SampleType process (State& s, SampleType x, Coeffs& c)
        {
            let y = SampleType (x - s.x[0] + c.a0 * s.y[0]);
            s.x[0] = x;
            s.y[0] = y;
            return SampleType (c.b1 * y);
        }

        processor Processor (float64 frequency = 30.0f)
        {
            input stream SampleType in;
            output stream SampleType out;

            void run()
            {
                State s;
                Coeffs c;

                c.update (processor.frequency, frequency);

                loop
                {
                    out << s.process (in, c);
                    advance();
                }
            }
        }
    }

    //==============================================================================
    /** Biquadratic (two-pole-two-zero) IIR filter building block.

        Direct Form I (DFI):

            y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] - a1 * y[n-1] - a2 * y[n-2]

        Transposed Direct Form II (TDFII):

            y[n] = b0 * x[n] + s1
            s1 = b1 * x[n] + a1 * y[n] + s2
            s2 = b2 * x[n] + a2 * y[n]
    */
    namespace biquad
    {
)soul_code"
R"soul_code(

        // Coefficients and related functions

        /** Holds a set of filter coefficients. */
        struct Coeffs
        {
            CoeffType<3> b, /**< feed-forward, numerator, zero, FIR coefficients */
                         a; /**< feed-back, denominator, pole, IIR coefficients */
        }

        /** Initialises a set of coefficients. */
        void set (Coeffs &c,
                  CoeffType b0, CoeffType b1, CoeffType b2,
                  CoeffType a0, CoeffType a1, CoeffType a2)
        {
            c.b[0] = b0; c.b[1] = b1; c.b[2] = b2; c.a[0] = a0; c.a[1] = a1; c.a[2] = a2;
        }

        /** Sets the coefficients, normalising based on the first feedback coefficient v.a[0] */
        void setNonNormalised (Coeffs &c, const Coeffs& v)
        {
            let oneOverA0 = 1.0 / v.a[0];
            c.b = v.b * oneOverA0;
            c.a = v.a * oneOverA0;
            c.a[0] = v.a[0]; // keep original a0
        }

        /** Sets the coefficients, assuming v is already normalised. */
        void setNormalised (Coeffs &c, Coeffs v)
        {
            c = v;
        }

        /** Clears a set of coefficients. */
        void clear (Coeffs &c)
        {
            c.b = CoeffType();
            c.a = CoeffType();
        }

        /** Holds the filter state. */
        struct State
        {
            SampleType[2] x;
            SampleType[2] y;
        }

        /** Resets a filter state. */
        void reset (State &s)
        {
            s.x[0] = SampleType();
            s.x[1] = SampleType();
            s.y[0] = SampleType();
            s.y[1] = SampleType();
        }

        /** */
        SampleType processDFI (State& s, SampleType x, Coeffs& c)
        {
            let y  = SampleType (c.b[0] * x
                               + c.b[1] * s.x[0]
                               + c.b[2] * s.x[1]
                               - c.a[1] * s.y[0]
                               - c.a[2] * s.y[1]);

            s.x[1] = s.x[0];
            s.x[0] = x;
)soul_code"
R"soul_code(

            s.y[1] = s.y[0];
            s.y[0] = y;

            return y;
        }

        /** See https://www.earlevel.com/DigitalAudio/images/BiquadTDFII.gif */
        SampleType processTDFII (State& s, SampleType x, Coeffs& c)
        {
            let y  = s.x[0] + SampleType (c.b[0]) * x;
            s.x[0] = s.y[0] + SampleType (c.b[1]) * x - SampleType (c.a[1]) * y;
            s.y[0] = SampleType (c.b[2]) * x - SampleType (c.a[2]) * y;
            return y;
        }

        /** Like processTDFII, but optimised for c.b[2] and c.a[2] == 0. */
        SampleType processOnePole (State& s, SampleType x, Coeffs& c)
        {
            let y  = s.x[0] + SampleType (c.b[0]) * x;
            s.x[0] = SampleType (c.b[1]) * x - SampleType (c.a[1]) * y;
            return y;
        }

        /** */
        SampleType processCascadeDFI<StateArrayType, CoeffsArrayType> (SampleType x,
                                                                       StateArrayType& s,
                                                                       CoeffsArrayType& c)
        {
            static_assert (StateArrayType.isArray, "states argument is not an array");
            static_assert (CoeffsArrayType.isArray, "coeffs argument is not an array");
            static_assert (s.size == c.size, "states and coeffs arrays are not the same size");

            var y = x;

            for (wrap<s.size> i)
                y = processDFI (s[i], y, c[i]);

            return y;
        }

        /** */
        SampleType processCascadeTDFII<StateArrayType, CoeffsArrayType> (SampleType x,
                                                                         StateArrayType& s,
                                                                         CoeffsArrayType& c)
        {
            static_assert (StateArrayType.isArray, "states argument is not an array");
            static_assert (CoeffsArrayType.isArray, "coeffs argument is not an array");
)soul_code"
R"soul_code(

            static_assert (s.size == c.size, "states and coeffs arrays are not the same size");

            var y = x;

            for (wrap<s.size> i)
                y = processTDFII (s[i], y, c[i]);

            return y;
        }
    }

    //==============================================================================
    /**
        First-order IIR filter.

            y[n] = b0 * x[n] - a1 * y[n-1]

        Coefficients derived from Pirkle.
        This filter is not suitable for modulation.
    */
    namespace onepole
    {
        /** Constants for use in specifying the filter mode. */
        namespace Mode
        {
            let lowpass  = 0;
            let highpass = 1;
            let allpass  = 2;
        }

        /** Updates a set of coefficients for the given settings. */
        void update (biquad::Coeffs& c, float64 sampleRate, int mode, float64 freqHz)
        {
            biquad::Coeffs nc; // normalised coefficients
            nc.a[0] = 1.0;

            let theta = twoPi * (freqHz / sampleRate);

            if (mode == Mode::lowpass)
            {
                let gamma = CoeffType (cos (theta) / (1.0 + sin (theta)));

                nc.b[0] = (1 - gamma) / 2;
                nc.b[1] = (1 - gamma) / 2;
                nc.a[1] = -gamma;
            }
            else if (mode == Mode::highpass)
            {
                let gamma = CoeffType (cos (theta) / (1.0 + sin (theta)));

                nc.b[0] = (1 + gamma) / 2;
                nc.b[1] = -(1 + gamma) / 2;
                nc.a[1] = -gamma;
            }
            else if (mode == Mode::allpass)
            {
                let w = CoeffType (tan (theta * 0.5));
                let alpha = (w - 1) / (w + 1);

                nc.b[0] = alpha;
                nc.b[1] = 1.0;
                nc.a[1] = alpha;
            }

            c.setNormalised (nc);
        }

        /** A processor to render a onepole filter. */
        processor Processor (int initialMode = 0,
)soul_code"
R"soul_code(

                             float initialFrequency = defaultFreqHz)
        {
            input stream SampleType in;
            output stream SampleType out;

            input event
            {
                float frequencyIn [[ name: "Frequency", min: minFreqHz,   max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
                float modeIn      [[ name: "Mode",      min: 0,           max: 2,         init: 0,         text: "Lowpass|Highpass|Allpass"]];
            }

            event frequencyIn (float v)   { frequency = v;  recalc = true; }
            event modeIn      (float v)   { mode = int (v); recalc = true; }

            float frequency  = initialFrequency;
            int   mode       = initialMode;
            bool  recalc     = true;

            void run()
            {
                biquad::State s;
                biquad::Coeffs c;

                loop
                {
                    if (recalc)
                    {
                        recalc = false;
                        let clippedFrequency = clamp (float64 (frequency),
                                                      float64 (minFreqHz),
                                                      processor.frequency * normalisedFreqLimit);
                        update (c, processor.frequency, mode, clippedFrequency);
                    }

                    loop (updateInterval)
                    {
                        out << s.processOnePole (in, c);
                        advance();
                    }
                }
            }
        }
    }

    //==============================================================================
    /** RBJ biquad EQ, 2nd Order IIR Filter.

        This filter is not suitable for modulation.
        See https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
    */
    namespace rbj_eq
    {
        /** Constants for use in specifying the filter mode. */
        namespace Mode
        {
            let lowpass   = 0;
)soul_code"
R"soul_code(

            let highpass  = 1;
            let bandpass  = 2;
            let lowShelf  = 3;
            let highShelf = 4;
            let peaking   = 5;
            let notch     = 6;
            let allpass   = 7;
        }

        /** Updates a set of coefficients for the given settings. */
        void update (biquad::Coeffs& c,
                     float64 sampleRate, int mode, float64 freqHz,
                     float64 quality, float64 gaindB)
        {
            biquad::Coeffs nnc; // non-normalised coefficients

            let theta = CoeffType (twoPi * (freqHz / sampleRate));
            let sinTheta = sin (theta);
            let cosTheta = cos (theta);
            let Q = max (quality, 0.001);
            let alpha = CoeffType (sinTheta / (2.0 * Q));

            if (mode == Mode::lowpass)
            {
                nnc.b[0] = (1 - cosTheta) / 2;
                nnc.b[1] =  1 - cosTheta;
                nnc.b[2] = (1 - cosTheta) / 2;
                nnc.a[0] =  1 + alpha;
                nnc.a[1] = -2 * cosTheta;
                nnc.a[2] =  1 - alpha;
            }
            else if (mode == Mode::highpass)
            {
                nnc.b[0] = (1 + cosTheta) / 2;
                nnc.b[1] = -(1 + cosTheta);
                nnc.b[2] = (1 + cosTheta) / 2;
                nnc.a[0] =  1 + alpha;
                nnc.a[1] = -2 * cosTheta;
                nnc.a[2] =  1 - alpha;
            }
            else if (mode == Mode::bandpass) // (constant 0 dB peak gain)
            {
                nnc.b[0] = alpha;
                nnc.b[1] = 0;
                nnc.b[2] = -alpha;
                nnc.a[0] =  1 + alpha;
                nnc.a[1] = -2 * cosTheta;
                nnc.a[2] =  1 - alpha;
            }
            else if (mode == Mode::lowShelf)
            {
                let A = CoeffType (pow (10.0, gaindB / 40.0));
                nnc.b[0] = A * ((A + 1) - (A - 1) * cosTheta + 2 * sqrt (A) * alpha);
                nnc.b[1] = 2 * A * ( (A - 1) - (A + 1) * cosTheta);
)soul_code"
R"soul_code(

                nnc.b[2] = A * ((A + 1) - (A - 1) * cosTheta - 2 * sqrt (A) * alpha);
                nnc.a[0] = (A + 1) + (A - 1) * cosTheta + 2 * sqrt (A) * alpha;
                nnc.a[1] = -2 * ( (A - 1) + (A + 1) * cosTheta);
                nnc.a[2] = (A + 1) + (A - 1) * cosTheta - 2 * sqrt (A) * alpha;
            }
            else if (mode == Mode::highShelf)
            {
                let A = CoeffType (pow (10.0, gaindB / 40.0));
                nnc.b[0] = A * ((A + 1) + (A - 1) * cosTheta + 2 * sqrt (A) * alpha);
                nnc.b[1] = -2 * A * ( (A - 1) + (A + 1) * cosTheta);
                nnc.b[2] = A * ((A + 1) + (A - 1) * cosTheta - 2 * sqrt (A) * alpha);
                nnc.a[0] = (A + 1) - (A - 1) * cosTheta + 2 * sqrt (A) * alpha;
                nnc.a[1] = 2 * ( (A - 1) - (A + 1) * cosTheta);
                nnc.a[2] = (A + 1) - (A - 1) * cosTheta - 2 * sqrt (A) * alpha;
            }
            else if (mode == Mode::peaking)
            {
                let A = CoeffType (pow (10.0, gaindB / 40.0));
                nnc.b[0] = 1 + alpha * A;
                nnc.b[1] = -2 * cosTheta;
                nnc.b[2] = 1 - alpha * A;
                nnc.a[0] = 1 + alpha / A;
                nnc.a[1] = -2 * cosTheta;
                nnc.a[2] = 1 - alpha / A;
            }
            else if (mode == Mode::notch)
            {
                nnc.b[0] = 1;
                nnc.b[1] = -2 * cosTheta;
                nnc.b[2] = 1;
                nnc.a[0] = 1 + alpha;
                nnc.a[1] = -2 * cosTheta;
                nnc.a[2] = 1 - alpha;
            }
            else if (mode == Mode::allpass)
            {
                nnc.b[0] = 1 - alpha;
                nnc.b[1] = -2 * cosTheta;
                nnc.b[2] = 1 + alpha;
                nnc.a[0] = 1 + alpha;
                nnc.a[1] = -2 * cosTheta;
                nnc.a[2] = 1 - alpha;
            }

            c.setNonNormalised (nnc);
        }

        /** */
        processor Processor (int initialMode = 0,
)soul_code"
R"soul_code(

                             float initialFrequency = defaultFreqHz,
                             float initialQuality   = defaultQuality,
                             float initialGain      = defaultGain)
        {
            input stream SampleType in;
            output stream SampleType out;

            input event
            {
                float modeIn      [[ name: "Mode",      min: 0,           max: 7,         init: 0,         text: "Lowpass|Highpass|Bandpass|LowShelf|HighShelf|Peaking|Notch|Allpass"]];
                float frequencyIn [[ name: "Frequency", min: minFreqHz,   max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
                float qualityIn   [[ name: "Q",         min: 0.,          max: 10.0,      init: defaultQuality]];
                float gainIn      [[ name: "Gain",      min: -36.0,       max: 36.0,      init: defaultGain, unit: "dB"]];
            }

            event frequencyIn (float v) { frequency = v;   recalc = true; }
            event qualityIn   (float v) { quality = v;     recalc = true; }
            event gainIn      (float v) { gain = v;        recalc = true; }
            event modeIn      (float v) { mode = int (v);  recalc = true; }

            float  frequency = initialFrequency;
            float  quality   = initialQuality;
            float  gain      = initialGain;
            int    mode      = initialMode;
            bool   recalc    = true;

            void run()
            {
                biquad::State s;
                biquad::Coeffs c;

                loop
                {
                    if (recalc)
                    {
                        recalc = false;
                        let clippedFrequency = clamp (float64 (frequency), float64 (minFreqHz), processor.frequency * normalisedFreqLimit);
                        c.update (processor.frequency, mode, clippedFrequency, quality, gain);
                    }

                    loop (updateInterval)
                    {
)soul_code"
R"soul_code(

                        out << s.processTDFII (in, c);
                        advance();
                    }
                }
            }
        }
    }

    //==============================================================================
    /**
        Generic second-order-section cascade biquad processors.
    */
    namespace sos_cascade
    {
        /** Supply an array of coefficients for each SOS. The size of the array should be a multiple of 6,
            and the coefficients are expected to be normalised already
        */
        processor Processor (const CoeffType[] coeffs)
        {
            input stream SampleType in;
            output stream SampleType out;

            let numSOS = size (coeffs) / 6;
            biquad::State[numSOS] s;
            biquad::Coeffs[numSOS] c;

            void init()
            {
                static_assert (size (coeffs) % 6 == 0, "coeffs is not a multiple of 6");

                wrap<numSOS> section = 0;
                wrap<coeffs.size> coeffIdx = 0;

                loop (numSOS)
                {
                    // TODO: replace with dynamic array slice
                    biquad::set (c[section], coeffs[wrap<coeffs.size> (coeffIdx + 0)],
                                             coeffs[wrap<coeffs.size> (coeffIdx + 1)],
                                             coeffs[wrap<coeffs.size> (coeffIdx + 2)],
                                             coeffs[wrap<coeffs.size> (coeffIdx + 3)],
                                             coeffs[wrap<coeffs.size> (coeffIdx + 4)],
                                             coeffs[wrap<coeffs.size> (coeffIdx + 5)]);
                    section++;
                    coeffIdx += 6;
                }
            }

            void run()
            {
                loop
                {
                    out << biquad::processCascadeTDFII (in, s, c);
                    advance();
                }
            }
        }
    }

)soul_code"
R"soul_code(

    //==============================================================================
    /**
        N-th order Butterworth filter, made by cascading multiple second order sections.

        This filter is not suitable for modulation.
    */
    namespace butterworth
    {
        // namespace biquad = soul::filters::biquad;
        // namespace onepole = soul::filters::onepole;
        // namespace rbj = soul::filters::rbj_eq;

        /** Constants for use in specifying the filter mode. */
        namespace Mode
        {
            let lowpass  = 0;
            let highpass = 1;
        }

        /** Updates a set of coefficients for the given settings. */
        void update<CoeffsArrayType> (CoeffsArrayType& coeffs, float64 sampleRate, int order, int mode, float64 freqHz)
        {
            static_assert (CoeffsArrayType.isArray, "coeffs argument is not an array");

            biquad::Coeffs nc; // normalised coefficients

            bool oddOrder = (order % 2) == 1;

            if (oddOrder)
            {
                onepole::update (nc, sampleRate, mode, freqHz);
                coeffs.at (0).setNormalised (nc);

                for (int i=0; i<order/2; i++)
                {
                    let Q = 1.0 / (2.0 * cos ((CoeffType (i) + 1.0) * pi / order));
                    rbj_eq::update (nc, sampleRate, mode, freqHz, Q, 0.);
                    coeffs.at (i+1).setNormalised (nc);
                }
            }
            else
            {
                for (wrap<coeffs.size> i)
                {
                    let Q = 1.0 / (2.0 * cos ((2.0 * CoeffType (i) + 1.0) * pi / (order * 2.0)));
                    rbj_eq::update (nc, sampleRate, mode, freqHz, Q, 0.);
                    coeffs.at (i).setNormalised (nc);
                }
            }
        }

        /** Butterworth processor.
            The order must be > 0
        */
        processor Processor (int order,
                             int initialMode = 0,
)soul_code"
R"soul_code(

                             float initialFrequency = defaultFreqHz)
        {
            input stream SampleType in;
            output stream SampleType out;

            input event
            {
                float frequencyIn [[ name: "Frequency", min: minFreqHz,   max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
                float modeIn      [[ name: "Mode",      min: 0,           max: 1,         init: 0,         text: "Lowpass|Highpass"]];
            }

            event frequencyIn (float v) { frequency = v; recalc = true; }
            event modeIn      (float v) { mode = int (v); recalc = true; clear = true; }

            float frequency = initialFrequency;
            int mode = initialMode;
            bool recalc = true;
            bool clear = true;

            void run()
            {
                let numSOS = (order / 2) + (order % 2);
                biquad::State[numSOS] s;
                biquad::Coeffs[numSOS] c;

                loop
                {
                    if (recalc)
                    {
                        if (clear) // mode change requires clearing histories
                        {
                            for (wrap<s.size> i)
                            {
                                biquad::reset (s[i]);
                            }
                        }

                        recalc = false;
                        let clippedFrequency = clamp (float64 (frequency), float64 (minFreqHz), processor.frequency * normalisedFreqLimit);
                        update (c, processor.frequency, order, mode, clippedFrequency);
                    }

                    loop (updateInterval)
                    {
                        out << biquad::processCascadeTDFII (in, s, c);
                        advance();
                    }
                }
            }
        }
    }

    //==============================================================================
    /** Analytic filter / IIR Hilbert transformer.

)soul_code"
R"soul_code(

        https://dsp.stackexchange.com/a/59157

        Increasing numFilters will increase the accuracy of the quadrature output across the
        pass band but introduce more delay with respect to the input signal.
        The filter passband is from transitionBandwidthHz to nyquist - transitionBandwidthHz.
    */
    namespace analytic (int numFilters = 6,
                        float transitionBandwidthHz = 20.0f)
    {
        /** Polyphase IIR Designer.
            Based on HIIR http://ldesoras.free.fr/prod.html#src_hiir
        */
        namespace polyphase_iir_design
        {
            let numCoefficients = numFilters * 2;
            let order = numCoefficients * 2 + 1;

            /** Holds a set of filter coefficients. */
            struct Coeffs
            {
                CoeffType[numCoefficients] coeffs;
            }

            /** */
            Coeffs compute (float64 transition)
            {
                Coeffs c;

                let p = computeTransitionParam (transition);

                for (wrap<numCoefficients> i)
                    c.coeffs.at (i) = computeCoeff (i, p);

                return c;
            }

            struct TransitionParams
            {
                float64 k, q;
            }

            TransitionParams computeTransitionParam (float64 transition)
            {
                var k = tan ((1.0 - transition * 2.0) * (pi / 4));
                k *= k;
                let kksqrt = pow (1.0 - k * k, 0.25);
                let e = 0.5 * (1.0 - kksqrt) / (1 + kksqrt);
                let e2 = e * e;
                let e4 = e2 * e2;
                let q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));

                return TransitionParams (k, q);
            }

            float64 computeAccNum (float64 q, int c)
            {
                float64 result, i = 0, j = 1;

                loop
                {
                    let next = pow (q, float64 (i * (i + 1))) * sin ((i * 2 + 1) * c * pi / order) * j;
)soul_code"
R"soul_code(

                    result += next;

                    if (abs (next) < 1e-100)
                        return result;

                    j = -j;
                    i++;
                }
            }

            float64 computeAccDen (float64 q, int c)
            {
                float64 result, i = 1.0, j = -1.0;

                loop
                {
                    let next = pow (q, (i * i)) * cos (i * 2 * c * pi / order) * j;
                    result += next;

                    if (abs (next) < 1e-100)
                        return result;

                    j = -j;
                    i = i + 1.0;
                }
            }

            CoeffType computeCoeff (int index, TransitionParams params)
            {
                let num = computeAccNum (params.q, index + 1)  * pow (params.q, 0.25);
                let den = computeAccDen (params.q, index + 1) + 0.5;
                let ww = num / den;
                let wwsq = CoeffType (ww * ww);

                let x = CoeffType (sqrt ((1 - wwsq * params.k) * (1 - wwsq / params.k)) / (1 + wwsq));
                return (1 - x) / (1 + x);
            }
        }

        /** Parallel 2nd-order all-pass.

                y[n] = c * (x[n] + y[n - 2]) - x[n - 2]
        */
        namespace dual_apf
        {
            /** Holds the filter state. */
            struct State
            {
                SampleType[2] x1, y1, x2, y2;
                CoeffType[2] c;
            }

            /** */
            SampleType[2] process (State& s, SampleType[2] x)
            {
                let y = SampleType[2] (SampleType (s.c[0]) * (x[0] + s.y2[0]) - s.x2[0],
                                       SampleType (s.c[1]) * (x[1] + s.y2[1]) - s.x2[1]);
                s.x2 = s.x1;
                s.x1 = x;
                s.y2 = s.y1;
                s.y1 = y;

                return y;
            }
        }

        /** Holds the filter state. */
        struct State
        {
            dual_apf::State[numFilters] apfs;
)soul_code"
R"soul_code(

            SampleType[1] x;
        }

        /** Updates a filter state with the given sample rate. */
        void update (State& s, float64 sampleRate)
        {
            let design = polyphase_iir_design::compute (2.0 * (transitionBandwidthHz / sampleRate));

            for (wrap<numFilters> i)
            {
                s.apfs.at (i).c[0] = design.coeffs.at (i * 2);
                s.apfs.at (i).c[1] = design.coeffs.at (i * 2 + 1);
            }
        }

        /** */
        SampleType[2] process (State& s, SampleType x)
        {
            var y = SampleType[2] (x, s.x[0]);

            for (wrap<numFilters> i)
            {
                y = s.apfs[i].process (y);
            }

            s.x[0] = x;
            return y;
        }

        /** */
        processor Processor
        {
            input stream SampleType in;
            output stream SampleType realOut;
            output stream SampleType imagOut;

            State s;

            void init()
            {
                s.update (processor.frequency);
            }

            void run()
            {
                loop
                {
                    let y = s.process (in);
                    realOut << y[0];
                    imagOut << y[1];
                    advance();
                }
            }
        }
    }

    //==============================================================================
    /** Complex Resonator filter.

        See https://ccrma.stanford.edu/~jos/smac03maxjos/smac03maxjos.pdf
        This filter is suitable for modulation
    */
    namespace complex_resonator
    {
        /** Holds the filter state. */
        struct State
        {
            SampleType yReal;
            SampleType yImag;
        }

        /** Holds a set of filter coefficients. */
        struct Coeffs
        {
            complex64 v;
        }

        complex64 cexp (complex64 c)
        {
            // TODO: if cexp becomes a global function, use that
)soul_code"
R"soul_code(

            let e = exp (c.real);
            return complex64 (e * cos (c.imag), e * sin (c.imag));
        }

        /** Updates a set of coefficients for the given settings. */
        void update (Coeffs& c, float64 sampleRate, float64 freqHz, float64 t60)
        {
            let jImag = (0.0 + 1.0i);
            let decayFactor = tau2pole (t60, sampleRate);
            let oscCoef = cexp ((jImag * twoPi) * (freqHz / sampleRate));
            c.v = decayFactor * oscCoef;
        }

        /** */
        SampleType[2] process (State& s, Coeffs& c, SampleType x)
        {
            let xReal = x;
            let xImag = SampleType();
            let vReal = SampleType (c.v.real);
            let vImag = SampleType (c.v.imag);
            let yReal = xReal + ((vReal * s.yReal) - (vImag * s.yImag));
            let yImag = xImag + ((vReal * s.yImag) + (vImag * s.yReal));
            s.yReal = yReal;
            s.yImag = yImag;
            return SampleType[2] (yReal, yImag);
        }

        /** */
        processor Processor (float initialFrequency = defaultFreqHz,
                             float initialDecay = 1.0f,
                             float initialGain = 1.0f)
        {
            input stream SampleType in;
            output stream SampleType realOut;
            output stream SampleType imagOut;

            input event
            {
                float frequencyIn [[ name: "Frequency", min: minFreqHz,   max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
                float decayIn     [[ name: "Decay",     min: 0.01,        max: 10.0,      init: 1.0, unit: "Seconds"]];
                float gainIn      [[ name: "Gain",      min: 0.01,        max: 10.0,      init: 1.0]];
            }

            event frequencyIn (float v) { frequency = v; recalc = true; }
            event decayIn     (float v) { decay = v; recalc = true; }
            event gainIn      (float v) { gain = v; recalc = true; }

            float frequency = initialFrequency,
)soul_code"
R"soul_code(

                      decay = initialDecay,
                       gain = initialGain;
            bool recalc = true;

            void run()
            {
                State s;
                Coeffs c;

                dc_blocker::Coeffs dcc;
                dc_blocker::State realDCBlocker, imagDCBlocker;

                dcc.update (processor.frequency, 30.0f);

                loop
                {
                    if (recalc)
                    {
                        recalc = false;
                        let clippedFrequency = clamp (float64 (frequency), float64 (minFreqHz), processor.frequency * normalisedFreqLimit);
                        c.update (processor.frequency, clippedFrequency, decay);
                    }

                    loop (updateInterval)
                    {
                        let y = s.process (c, in);
                        realOut << realDCBlocker.process (y[0] * gain, dcc);
                        imagOut << imagDCBlocker.process (y[1] * gain, dcc);
                        advance();
                    }
                }
            }
        }
    }

    //==============================================================================
    /**
        This namespace contains a set of "Topology preserving transform" filters.
    */
    namespace tpt
    {
        //==============================================================================
        /** "Topology preserving transform" one-pole filter.

            Derived from work by Zavalishin and Pirkle.
            This filter is suitable for modulation.
        */
        namespace onepole
        {
            /** Constants for use in specifying the filter mode. */
            namespace Mode
            {
                let lowpass  = 0;
                let highpass = 1;
                let allpass  = 2;
            }

            /** Holds a set of filter coefficients. */
            struct Coeffs
            {
                CoeffType b;
            }

)soul_code"
R"soul_code(

            /** Holds the filter state. */
            struct State
            {
                SampleType z1;
            }

            /** Clears a set of coefficients. */
            void clear (Coeffs& c)
            {
                c.b = CoeffType (1);
            }

            /** Resets a filter state. */
            void reset (State& s)
            {
                s.z1 = SampleType();
            }

            /** Updates a set of coefficients for the given settings. */
            void update (Coeffs& c, float64 sampleRate, float64 freqHz)
            {
                let wd = twoPi * freqHz;
                let T  = 1.0 / sampleRate;
                let wa = (2.0 / T) * tan (wd * T / 2.0);
                let g  = wa * T / 2.0;

                c.b = CoeffType (g / (1.0 + g));
            }

            /** */
            SampleType processLPF (State& s, SampleType x, Coeffs& c)
            {
                let vn = (x - s.z1) * SampleType (c.b);
                let lpf = vn + s.z1;
                s.z1 = vn + lpf;
                return lpf;
            }

            /** */
            SampleType processHPF (State& s, SampleType x, Coeffs& c)
            {
                return x - processLPF (s, x, c);
            }

            /** */
            SampleType processAPF (State& s, SampleType x, Coeffs& c)
            {
                let lpf = processLPF (s, x, c);
                let hpf = x - lpf;
                return lpf - hpf;
            }

            /** */
            processor Processor (int initialMode = 0,
                                 float initialFrequency = defaultFreqHz)
            {
                input stream SampleType in;
                output stream SampleType out;

                input event
                {
                    float frequencyIn [[ name: "Frequency", min: minFreqHz, max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
)soul_code"
R"soul_code(

                    float modeIn      [[ name: "Mode",      min: 0,         max: 2,         init: 0,         text: "Lowpass|Highpass|Allpass"]];
                }

                event frequencyIn (float v) { frequency = v; recalc = true; }
                event modeIn      (float v) { mode = int (v); recalc = true; }

                float frequency = initialFrequency;
                int mode = initialMode;
                bool recalc = true;

                void run()
                {
                    State s;
                    Coeffs c;
                    c.clear ();

                    loop
                    {
                        if (recalc)
                        {
                            recalc = false;
                            let clippedFrequency = clamp (float64 (frequency),
                                                          float64 (minFreqHz),
                                                          processor.frequency * normalisedFreqLimit);
                            c.update (processor.frequency, clippedFrequency);
                        }

                        loop (updateInterval)
                        {
                            if (mode == Mode::lowpass)        out << s.processLPF (in, c);
                            else if (mode == Mode::highpass)  out << s.processHPF (in, c);
                            else if (mode == Mode::allpass)   out << s.processAPF (in, c);

                            advance();
                        }
                    }
                }
            }
        }

        //==============================================================================
        /** "Topology preserving transform" multi-mode state variable filter (SVF).

            Derived from work by Zavalishin and Pirkle.
            This filter is suitable for modulation.
        */
        namespace svf
        {
            /** Constants for use in specifying the filter mode. */
            namespace Mode
            {
)soul_code"
R"soul_code(

                let lowpass  = 0;
                let highpass = 1;
                let bandpass = 2;
            }

            /** Holds a set of filter coefficients. */
            struct Coeffs
            {
                CoeffType a0;
                CoeffType a;
                CoeffType p;
            }

            /** Holds the filter state. */
            struct State
            {
                SampleType[2] z;
            }

            /** Resets a filter state. */
            void reset (State& s)
            {
                s.z[0] = SampleType();
                s.z[1] = SampleType();
            }

            /** Updates a set of coefficients for the given settings. */
            void update (Coeffs& c, float64 sampleRate, float64 freqHz, float64 quality)
            {
                let Q = CoeffType (max (quality, 0.001));
                let wd = CoeffType (twoPi * freqHz);
                let T  = CoeffType (1 / sampleRate);
                let wa = (2 / T) * tan (wd * T / 2);
                let g  = wa * T / 2;
                let R = 1 / (2 * Q);
                c.a0 = 1 / (1 + 2 * R * g + g * g);
                c.a = g;
                c.p = 2 * R + g;
            }

            /** */
            SampleType[3] process (State& s, SampleType x, Coeffs& c)
            {
                let hpf = SampleType (c.a0 * (x - c.p * s.z[0] - s.z[1]));
                let bpf = SampleType (c.a * hpf + s.z[0]);
                let lpf = SampleType (c.a * bpf + s.z[1]);

                s.z[0] = SampleType (c.a * hpf + bpf);
                s.z[1] = SampleType (c.a * bpf + lpf);

                return SampleType[3] (lpf, hpf, bpf);
            }

            /** */
            processor Processor (float initialFrequency = defaultFreqHz,
                                 float initialQuality = defaultQuality)
            {
                input stream SampleType in;
                output stream SampleType lowpassOut, bandpassOut, highpassOut;

                input event
)soul_code"
R"soul_code(

                {
                    float frequencyIn [[ name: "Frequency", min: minFreqHz,   max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
                    float qualityIn   [[ name: "Q",         min: 0.01,        max: 100.0,     init: defaultQuality]];
                }

                event frequencyIn (float v) { frequency = v; recalc = true; }
                event qualityIn   (float v) { quality = v; recalc = true; }

                float frequency = initialFrequency,
                        quality = initialQuality;
                bool recalc = true;

                void run()
                {
                    State s;
                    Coeffs c;

                    loop
                    {
                        if (recalc)
                        {
                            recalc = false;
                            let clippedFrequency = clamp (float64 (frequency),
                                                          float64 (minFreqHz),
                                                          processor.frequency * normalisedFreqLimit);
                            c.update (processor.frequency, clippedFrequency, quality);
                        }

                        loop (updateInterval)
                        {
                            let y = s.process (in, c);
                            lowpassOut  << y[0];
                            highpassOut << y[1];
                            bandpassOut << y[2];
                            advance();
                        }
                    }
                }
            }
        }

        //==============================================================================
        /**
            N-th order Butterworth filter, made by cascading TPT filters.
            This filter is suitable for modulation
        */
        namespace butterworth
        {
            /** Constants for use in specifying the filter mode. */
            namespace Mode
            {
                let lowpass  = 0;
)soul_code"
R"soul_code(

                let highpass = 1;
            }

            /** Updates a set of coefficients for the given settings. */
            void update<SVFCoeffsArrayType> (SVFCoeffsArrayType& svfCoeffs,
                                             onepole::Coeffs& onepoleCoeffs,
                                             float64 sampleRate, int order, float64 freqHz)
            {
                static_assert (SVFCoeffsArrayType.isArray, "coeffs argument is not an array");

                bool oddOrder = (order % 2) == 1;

                if (oddOrder)
                {
                    onepole::update (onepoleCoeffs, sampleRate, freqHz);

                    for (wrap<svfCoeffs.size> i)
                    {
                        let Q = 1.0 / (2.0 * cos ((CoeffType (i) + 1.0) * pi / order));
                        svf::update (svfCoeffs.at (i), sampleRate, freqHz, Q);
                    }
                }
                else
                {
                    for (wrap<svfCoeffs.size> i)
                    {
                        let Q = 1.0 / (2.0 * cos ((2.0 * CoeffType (i) + 1.0) * pi / (order * 2.0)));
                        svf::update (svfCoeffs.at (i), sampleRate, freqHz, Q);
                    }
                }
            }

            /** */
            SampleType process<StateArrayType, CoeffsArrayType> (SampleType x,
                                                                 StateArrayType& svfStates,
                                                                 CoeffsArrayType& svfCoeffs,
                                                                 onepole::State& onepoleState,
                                                                 onepole::Coeffs& onepoleCoeffs,
                                                                 int mode, bool oddOrder)
            {
                static_assert (StateArrayType.isArray, "states argument is not an array");
                static_assert (CoeffsArrayType.isArray, "coeffs argument is not an array");
)soul_code"
R"soul_code(

                static_assert (svfStates.size == svfCoeffs.size, "states and coeffs arrays are not the same size");

                var y = x;

                if (oddOrder)
                {
                    if (mode == Mode::lowpass)
                        y = onepoleState.processLPF (y, onepoleCoeffs);
                    else if (mode == Mode::highpass)
                        y = onepoleState.processHPF (y, onepoleCoeffs);
                }

                for (wrap<svfStates.size> i)
                    y = svf::process (svfStates[i], y, svfCoeffs[i])[wrap<2> (mode)]; // TODO: tidy this

                return y;
            }

            /* Butterworth processor.
               The order must be > 1
            */
            processor Processor (int order,
                                 int initialMode = 0,
                                 float initialFrequency = defaultFreqHz)
            {
                input stream SampleType in;
                output stream SampleType out;

                input event
                {
                    float frequencyIn [[ name: "Frequency", min: minFreqHz,   max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
                    float modeIn      [[ name: "Mode",      min: 0,           max: 1,         init: 0,         text: "Lowpass|Highpass"]];
                }

                event frequencyIn (float v)   { frequency = v; recalc = true; }
                event modeIn      (float v)   { mode = int (v); recalc = true; }

                float  frequency  = initialFrequency;
                int    mode       = initialMode;
                bool   recalc     = true;

                void run()
                {
                    let numSVFs = order / 2;
                    let oddOrder = (order % 2) == 1;

                    svf::State[numSVFs] svfStates;
                    svf::Coeffs[numSVFs] svfCoeffs;
                    onepole::State onepoleState;
                    onepole::Coeffs onepoleCoeffs;

                    loop
)soul_code"
R"soul_code(

                    {
                        if (recalc)
                        {
                            recalc = false;
                            let clippedFrequency = clamp (float64 (frequency),
                                                          float64 (minFreqHz),
                                                          processor.frequency * normalisedFreqLimit);
                            update (svfCoeffs, onepoleCoeffs, processor.frequency, order, clippedFrequency);
                        }

                        loop (updateInterval)
                        {
                            out << process (in, svfStates, svfCoeffs, onepoleState, onepoleCoeffs, mode, oddOrder);
                            advance();
                        }
                    }
                }
            }
        }

        //==============================================================================
        /**
            4th-order Linkwitz-Riley crossover filter, which outputs two bands of audio.

            The channels should sum together to produce a flat response.
            This filter is suitable for modulation.
        */
        namespace crossover
        {
            /** Holds the filter state. */
            struct State
            {
                svf::State svf1, svf2;
            }

            /** Holds a set of filter coefficients. */
            struct Coeffs
            {
                svf::Coeffs svf1, svf2;
            }

            /** */
            processor Processor (float initialFrequency = defaultFreqHz)
            {
                input stream SampleType in;
                output stream SampleType lowOut;
                output stream SampleType highOut;

                input event float frequencyIn [[ name: "Split Frequency", min: minFreqHz, max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];

                event frequencyIn (float v) { frequency = v; recalc = true; }

                float frequency = initialFrequency;
)soul_code"
R"soul_code(

                bool recalc = true;

                /** Updates a set of coefficients for the given settings. */
                void update (Coeffs& c, float64 sampleRate, float64 freqHz)
                {
                    c.svf1.update (sampleRate, freqHz, defaultQuality);
                    c.svf2.update (sampleRate, freqHz, defaultQuality);
                }

                SampleType[2] process (State& s, SampleType x, Coeffs& c)
                {
                    let svf1 = s.svf1.process (x, c.svf1);
                    let lpf1 = svf1[0], hpf1 = svf1[1], bpf1 = svf1[2];
                    let apf1 = lpf1 - SampleType (sqrt (2.0)) * bpf1 + hpf1;
                    let svf2 = s.svf2.process (lpf1, c.svf2);
                    let lpf2 = svf2[0];

                    return SampleType[2] (lpf2, apf1 - lpf2);
                }

                void run()
                {
                    State s;
                    Coeffs c;

                    loop
                    {
                        if (recalc)
                        {
                            recalc = false;
                            let clippedFrequency = clamp (float64 (frequency),
                                                          float64 (minFreqHz),
                                                          processor.frequency * normalisedFreqLimit);
                            update (c, processor.frequency, clippedFrequency);
                        }

                        loop (updateInterval)
                        {
                            let y = s.process (in, c);
                            lowOut  << y[0];
                            highOut << y[1];
                            advance();
                        }
                    }
                }
            }
        }

        //==============================================================================
        /** SVF EQ.

            Based on the work of Andy Simper:
)soul_code"
R"soul_code(

            https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf

            This filter is suitable for modulation.
        */
        namespace simper_eq
        {
            /** Constants for use in specifying the filter mode. */
            namespace Mode
            {
                let lowpass   = 0;
                let highpass  = 1;
                let bandpass  = 2;
                let lowShelf  = 3;
                let highShelf = 4;
                let peaking   = 5;
                let notch     = 6;
                let allpass   = 7;
                let bell      = 8;
            }

            /** Holds a set of filter coefficients. */
            struct Coeffs
            {
                CoeffType a1, a2, a3;
                CoeffType m0, m1, m2;
            }

            /** Holds the filter state. */
            struct State
            {
                SampleType ic1eq, ic2eq;
            }

            /** Resets a filter state. */
            void reset (State& s)
            {
                s.ic1eq = SampleType();
                s.ic2eq = SampleType();
            }

            /** Updates a set of coefficients for the given settings. */
            void update (Coeffs& c, float64 sampleRate, int mode, float64 freqHz, float64 quality, float64 gain)
            {
                let w = CoeffType (tan (pi * freqHz / sampleRate));
                let k = CoeffType (1  / clamp (quality, 0.01, 100.0));

                if (mode == Mode::lowpass)
                {
                    let g = w;
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 0;
                    c.m1 = 0;
                    c.m2 = 1;
                }
                else if (mode == Mode::highpass)
                {
                    let g = w;
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 1;
)soul_code"
R"soul_code(

                    c.m1 = -k;
                    c.m2 = -1;
                }
                else if (mode == Mode::bandpass)
                {
                    let g = w;
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 0;
                    c.m1 = 1;
                    c.m2 = 0;
                }
                else if (mode == Mode::lowShelf)
                {
                    let A = CoeffType (pow (10.0, gain / 40.0));
                    let g = w / sqrt (A);
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 1;
                    c.m1 = k * (A - 1);
                    c.m2 = (A * A - 1);
                }
                else if (mode == Mode::highShelf)
                {
                    let A = CoeffType (pow (10.0, gain / 40.0));
                    let g = w / sqrt (A);
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = A * A;
                    c.m1 = k * (1 - A) * A;
                    c.m2 = (1 - A * A);
                }
                else if (mode == Mode::peaking)
                {
                    let g = w;
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 1;
                    c.m1 = -k;
                    c.m2 = -2;
                }
                else if (mode == Mode::notch)
                {
                    let g = w;
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 1;
                    c.m1 = -k;
                    c.m2 = 0;
                }
                else if (mode == Mode::allpass)
                {
                    let g = w;
)soul_code"
R"soul_code(

                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 1;
                    c.m1 = -2 * k;
                    c.m2 = 0;
                }
                else if (mode == Mode::bell)
                {
                    let A = CoeffType (pow (10.0, gain / 40.0));
                    let g = w;
                    c.a1 = 1 / (1 + g * (g + k));
                    c.a2 = g * c.a1;
                    c.a3 = g * c.a2;
                    c.m0 = 1;
                    c.m1 = k * (A * A - 1);
                    c.m2 = 0;
                }
            }

            /** */
            SampleType process (State& s, SampleType x, Coeffs& c)
            {
                let v0 = x;
                let v3 = v0 - s.ic2eq;
                let v1 = SampleType (c.a1 * s.ic1eq + c.a2 * v3);
                let v2 = SampleType (s.ic2eq + c.a2 * s.ic1eq + c.a3 * v3);
                s.ic1eq = SampleType (2.0 * v1 - s.ic1eq);
                s.ic2eq = SampleType (2.0 * v2 - s.ic2eq);
                return    SampleType (c.m0 * v0 + c.m1 * v1 + c.m2 * v2);
            }

            /** */
            processor Processor (int initialMode = 0,
                                 float initialFrequency = defaultFreqHz,
                                 float initialQuality = defaultQuality,
                                 float initialGain = defaultGain)
            {
                input stream SampleType in;
                output stream SampleType out;

                input event
                {
                    float frequencyIn [[ name: "Frequency", min: minFreqHz,   max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];
                    float qualityIn   [[ name: "Q",         min: 0.001,       max: 100.0,     init: defaultQuality ]];
                    float gainIn      [[ name: "Gain",      min: -36.0,       max: 36.0,      init: 0.0,           unit: "dB"]];
)soul_code"
R"soul_code(

                    float modeIn      [[ name: "Mode",      min: 0,           max: 8,         init: 0,             text: "Lowpass|Highpass|Bandpass|LowShelf|HighShelf|Peaking|Notch|Allpass|Bell"]];
                }

                event frequencyIn (float v) { frequency = v; recalc = true; }
                event qualityIn   (float v) { quality = v; recalc = true; }
                event gainIn      (float v) { gain = v; recalc = true; }
                event modeIn      (float v) { mode = int (v); recalc = true; }

                float frequency = initialFrequency,
                        quality = initialQuality,
                           gain = initialGain;
                int mode = initialMode;
                bool recalc = true;

                void run()
                {
                    State s;
                    Coeffs c;

                    loop
                    {
                        if (recalc)
                        {
                            recalc = false;
                            let clippedFrequency = clamp (float64 (frequency),
                                                          float64 (minFreqHz),
                                                          processor.frequency * normalisedFreqLimit);
                            c.update (processor.frequency, mode, clippedFrequency, quality, gain);
                        }

                        loop (updateInterval)
                        {
                            out << s.process (in, c);
                            advance();
                        }
                    }
                }
            }
        }

    } // (namespace tpt)
}
)soul_code";

    if (moduleName == "soul.oscillators")  return R"soul_code(
/** Title: Oscillators

    This module contains a collection of oscillator algorithms for generating
    various wave-shapes. The collection covers low frequency designs and high quality alias
    free algotithms.
*/

/**
    The `oscillators` namespace is parameterised with a `SampleType` and `PhaseType`. The `SampleType` specifies
    the output type, and can be either `float32` or `float64`. The `PhaseType` is used internally to track
    phase position, so higher resolution data types can be used to reduce frequency inaccuracy for very critical
    applications.

    By default the `SampleType` and `PhaseType` are float32.
*/
namespace soul::oscillators (using SampleType = float32,
                             using PhaseType  = float32)
{
    static_assert (SampleType.isFloat, "The SampleType must be a float");
    static_assert (PhaseType.isFloat && ! PhaseType.isVector, "The PhaseType must be a float");

    let minFreqHz = 0.0f;
    let maxFreqHz = 22000.0f;
    let defaultFreqHz = 1000.0f;

    //==============================================================================
    /// A unipolar ramp (phasor) oscillator.
    /// This is a non-bandlimited oscillator that will cause aliasing, but is used internally by the BLEP oscillator.
    namespace phasor
    {
        struct State
        {
            /// The phase uses a range 0 to 1.0
            PhaseType phase;
            /// The increment of phase per frame
            PhaseType phaseIncrement;
        }

        /// Resets a phaser::State object
        void reset (State& s)
        {
            s.phase = PhaseType();
        }

        /// Updates a phaser::State object for a new rate
        void update (State& s, float64 samplePeriod, float64 freqHz)
        {
            s.phaseIncrement = PhaseType (samplePeriod * freqHz);
        }

        /// Increments a phaser::State object and returns the new phase
        PhaseType process (State& s)
        {
            s.phase += s.phaseIncrement;

)soul_code"
R"soul_code(

            while (s.phase >= PhaseType (1))
                s.phase -= PhaseType (1);

            return s.phase;
        }

        /// A processor that produces a stream of phase values as its output.
        processor Processor (float initialFrequency = 1000)
        {
            output stream SampleType out;
            input event float frequencyIn [[ name: "Frequency", min: minFreqHz, max: maxFreqHz, init: defaultFreqHz, unit: "Hz" ]];
            event frequencyIn (float v) { s.update (processor.period, v); }

            State s;

            void init()
            {
                s.update (processor.period, initialFrequency);
            }

            void run()
            {
                loop
                {
                    out << SampleType (s.process());
                    advance();
                }
            }
        }
    }

    //==============================================================================
    /// A simple sinewave oscillator which receives input events to control its frequency.
    processor Sine (float initialFrequency = 1000)
    {
        output stream SampleType out;
        input event float frequencyIn  [[ name: "Frequency", min: minFreqHz, max: maxFreqHz, init: defaultFreqHz, unit: "Hz" ]];
        event frequencyIn (float v) { phaseIncrement = float (v * twoPi * processor.period); }

        var phaseIncrement = float (initialFrequency * twoPi * processor.period);

        void run()
        {
            float phase;

            loop
            {
                out << sin (phase);
                phase = addModulo2Pi (phase, phaseIncrement);
                advance();
            }
        }
    }

    //==============================================================================
    /// A semi band-limited oscillator with sawtooth, square and triangle wave shapes
    /// using the PolyBLEP (Polynomial Band-Limited Step) technique.
    /// You may want to oversample this oscillator, in order to reduce aliasing.
    ///
)soul_code"
R"soul_code(

    namespace poly_blep
    {
        /// Contains different wave-shaping functions.
        namespace shapers
        {
            /// Generates a polyblep
            PhaseType polyblep (PhaseType phase, PhaseType phaseIncrement)
            {
                if (phase < phaseIncrement)
                {
                    phase /= phaseIncrement;
                    return (phase + phase) - (phase * phase) - PhaseType (1);
                }

                if (phase > PhaseType (1) - phaseIncrement)
                {
                    phase = (phase - PhaseType (1)) / phaseIncrement;
                    return (phase * phase) + (phase + phase) + PhaseType (1);
                }

                return PhaseType (0);
            }

            /// Generates a sawtooth wave from a phasor state
            PhaseType sawtooth (phasor::State& s)
            {
                return PhaseType (-1) + (PhaseType (2) * s.phase) - polyblep (s.phase, s.phaseIncrement);
            }

            /// Generates a square wave from a phasor state
            PhaseType square (phasor::State& s)
            {
                return (s.phase < PhaseType (0.5) ? PhaseType (-1) : PhaseType (1))
                        - polyblep (s.phase, s.phaseIncrement)
                        + polyblep (fmod (s.phase + PhaseType (0.5), PhaseType (1)), s.phaseIncrement);
            }
        }

        /// Contains constants for different wave-shapes
        namespace Shape
        {
            let sawtooth = 0;
            let triangle = 1;
            let square   = 2;
        }

        /// A processor which generates a dynamically adjustable wave-shape.
        processor Processor (int initialShape = 0,
                            float initialFrequency = 1000)
        {
            output stream SampleType out;
            input event float shapeIn       [[ name: "Shape",     min: 0,         max: 2,         init: 0,    text: "Sawtooth|Triangle|Square"]];
)soul_code"
R"soul_code(

            input event float frequencyIn   [[ name: "Frequency", min: minFreqHz, max: maxFreqHz, init: defaultFreqHz, unit: "Hz"]];

            event shapeIn (float v)        { shape = wrap<3> (floor (v)); }
            event frequencyIn (float v)    { s.update (processor.period, v); }

            phasor::State s;
            var shape = wrap<3> (initialShape);

            void init()
            {
                s.update (processor.period, initialFrequency);
            }

            void run()
            {
                PhaseType y = PhaseType (0);
                PhaseType triAccumulator = PhaseType (0);

                loop
                {
                    if (shape == Shape::sawtooth)
                    {
                        y = shapers::sawtooth (s);
                    }
                    else if (shape == Shape::triangle)
                    {
                        // leaky integrator
                        let coefficient = PhaseType (1.0 - (0.25 * s.phaseIncrement));
                        triAccumulator = s.phaseIncrement * shapers::square (s) + coefficient * triAccumulator;
                        y = triAccumulator * PhaseType (4);
                    }
                    else if (shape == Shape::square)
                    {
                        y = shapers::square (s);
                    }

                    s.process();

                    out << SampleType (y);

                    advance();
                }
            }
        }
    }

    //==============================================================================
    /// A quadrature sinusoidal oscillator producing sine and cosine outputs simultaneously.
    /// The `updateInterval` defines the samples between updates to the frequency taking effect.
    /// https://vicanek.de/articles/QuadOsc.pdf
    ///
    namespace quadrature (int updateInterval = 16)
    {
        struct State
        {
            SampleType u, v;
        }

        struct Coeffs
        {
            SampleType k1, k2;
)soul_code"
R"soul_code(

        }

        /// Resets a quadrature::State object.
        void reset (State& s)
        {
            s.u = SampleType (1);
            s.v = SampleType();
        }

        /// Updates a quadrature::State object for a new rate.
        void update (Coeffs& c, float64 freqHz, float64 sampleRate)
        {
            let w = twoPi * (freqHz / sampleRate);
            c.k1 = SampleType (tan (0.5 * w));
            c.k2 = SampleType (2.0 * c.k1 / (1.0 + c.k1 * c.k1));
        }

        /// Generates the next samples for a quadrature::State object.
        SampleType[2] process (State& s, Coeffs& c)
        {
            let tmp = s.u - c.k1 * s.v;
            s.v = s.v + c.k2 * tmp;
            s.u = tmp - c.k1 * s.v;
            return SampleType[2] (s.v, s.u);
        }

        /// A processor that generates a pair of sine/cosine output streams.
        processor Processor (float initialFrequency = 1000)
        {
            output stream SampleType sineOut;
            output stream SampleType cosineOut;

            input event float frequencyIn [[ name: "Frequency", min: -maxFreqHz, max: maxFreqHz, init: 0.0, unit: "Hz"]]; // Default meta data allows negative frequencies

            event frequencyIn (float v) { frequency = v; recalc = true; }

            float frequency  = initialFrequency;
            bool  recalc     = true;

            void run()
            {
                State s;
                Coeffs c;

                s.reset();

                loop
                {
                    if (recalc)
                    {
                        recalc = false;
                        c.update (frequency, processor.frequency);
                    }

                    loop (updateInterval)
                    {
                        let y = s.process (c);
                        sineOut << y[0];
                        cosineOut << y[1];
                        advance();
                    }
                }
            }
        }
    }

)soul_code"
R"soul_code(

    //==============================================================================
    ///
    /// A tempo-syncable LFO with some common shapes and options for uni-polar or bi-polar output.
    /// Unipolar LFO shapes run between 0 and 1, whilst Bipolar run between -1 and 1.
    ///
    namespace lfo
    {
        /// Various LFO shape generator functions.
        namespace shapers
        {
            SampleType rampUp           (PhaseType phase) { return (phase * 2.0f) - 1.0f; }
            SampleType rampUpUnipolar   (PhaseType phase) { return phase; }
            SampleType rampDown         (PhaseType phase) { return ((1.0f - phase) * 2.0f) - 1.0f; }
            SampleType rampDownUnipolar (PhaseType phase) { return (1.0f - phase); }
            SampleType triangle         (PhaseType phase) { return (2.0f * (1.0f - abs ((wrap (phase + 0.25f, 1.0f) * 2.0f) -1.0f))) - 1.0f; }
            SampleType triangleUnipolar (PhaseType phase) { return 1.0f - abs((phase * 2.0f) - 1.0f ); }
            SampleType square           (PhaseType phase) { return (float (phase > 0.5f) * 2.0f) - 1.0f; }
            SampleType squareUnipolar   (PhaseType phase) { return float (phase > 0.5f); }
            SampleType sine             (PhaseType phase) { return sin (phase * float32 (twoPi)); }
            SampleType sineUnipolar     (PhaseType phase) { return (sin (phase * float32 (twoPi)) + 1.0f) * 0.5f; }
        }

        /// A set of constants to specify different LFO shapes.
        namespace Shape
        {
            let triangle      = 0;
            let square        = 1;
            let rampUp        = 2;
            let rampDown      = 3;
            let sine          = 4;
            let sampleAndHold = 5;
        }

        /// Polarity control constants.
        namespace Polarity
        {
            let unipolar = 0;
            let bipolar  = 1;
        }

        /// Functions for applying a smoothing filter on a changing value.
        namespace smoother
        {
            struct State
)soul_code"
R"soul_code(

            {
                float currentValue;
                float targetValue;
                float increment;
                int steps;
            }

            /// Resets a smoother::State object
            void reset (State& state, float initialValue)
            {
                state.currentValue = initialValue;
                state.targetValue = initialValue;
                state.increment = 0.0f;
                state.steps = 0;
            }

            /// Updates a smoother::State object with a new target value and the
            /// number of steps to reach the target
            void setTarget (State& state, float targetValue, int steps)
            {
                state.targetValue = targetValue;
                state.increment = (state.targetValue - state.currentValue) / steps;
                state.steps = steps;
            }

            /// Advances the state of a smoother and returns the new smoothed value
            float tick (State& state)
            {
                if (state.steps == 0)
                    return state.currentValue;

                state.currentValue += state.increment;
                state.steps--;

                if (state.steps == 0)
                    state.currentValue = state.targetValue;

                return state.currentValue;
            }
        }

        /// A processor which implements an LFO with events to control its parameters.
        /// Changes to the frequency and depth are smoothed, but expect discontinuities if the shape or polarity
        /// are updated.
        processor Processor (int initialShape = 0,
                             int initialPolarity = 0,
                             float initialDepth = 1.0f,
                             float initialFrequency = 1.0f)
        {
            output stream SampleType out;

            input event
            {
                soul::timeline::Position positionIn;
                soul::timeline::TransportState transportStateIn;
)soul_code"
R"soul_code(

                soul::timeline::Tempo tempoIn;

                float rateHzIn     [[ name: "Rate (Hz)",     min: 0.01,  max: 40.0,   init: 1.,   unit: "hz", step: 0.01 ]];
                float rateTempoIn  [[ name: "Rate (Beats)",  min: 0.001, max: 32,     init: 0,    unit: "beat", step: 0.001]];
                float depthIn      [[ name: "Depth",         min: 0,     max: 100,    init: 100,  unit: "%",  step: 1 ]];
                float shapeIn      [[ name: "Shape",         min: 0,     max: 5,      init: 0,    text: "Triangle|Square|Ramp Up|Ramp Down|Sine|Sample & Hold"]];
                float polarityIn   [[ name: "Polarity",      min: 0,     max: 1,      init: 0,    text: "Unipolar|Bipolar"]];
                float rateModeIn   [[ name: "Rate Mode",     min: 0,     max: 1,      init: 0,    text: "Hz|Tempo"]];
                float syncIn       [[ name: "Timeline Sync", min: 0,     max: 1,      init: 0,    text: "Off|On"]];
            }

            event positionIn (soul::timeline::Position v)             { qnPos = v.currentQuarterNote; }
            event transportStateIn (soul::timeline::TransportState v) { transportRunning = v.state > 0; }
            event tempoIn (soul::timeline::Tempo v)                   { qnPhaseIncrement = (v.bpm / 60.0f) * float (processor.period); }

            event rateHzIn (float v)
            {
                if (! qnMode)
                    phaseIncrement = PhaseType (v * processor.period);
            }

            event rateTempoIn (float v)   { qnScalar = v; }
            event depthIn (float v)       { depth.setTarget (v * 0.01f, smoothingSamples); }
            event shapeIn (float v)       { shape = int (floor (v)); }
            event polarityIn (float v)    { polarity = (v < 0.5f) ? Polarity::unipolar : Polarity::bipolar; }
            event rateModeIn (float v)    { qnMode = v > 0.5f; }
            event syncIn (float v)        { timelineSync = v > 0.5f; }

            PhaseType phase;
)soul_code"
R"soul_code(

            var phaseIncrement = float32 (initialFrequency * processor.period);
            int shape = initialShape;
            int polarity = initialPolarity;
            smoother::State depth;

            let smoothingSamples = int (float (processor.frequency) * 0.02f);
            bool transportRunning = false;
            bool qnMode = false;
            bool timelineSync = false;
            float64 qnScalar = 1.0;
            float64 qnPos = 0.0;
            float32 qnPhaseIncrement = (120.0f / 60.0f) * (1.0f / 44100.0f); // Default = 1qn @ 120bpm / 44.1 sr

            PhaseType prevPhase = 1.0f;
            soul::random::RandomNumberState rng;
            SampleType noiseSample;

            SampleType getNoiseSample()
            {
                if (phase < prevPhase)
                    noiseSample = SampleType (polarity == Polarity::bipolar ? rng.getNextBipolar()
                                                                            : rng.getNextUnipolar());

                prevPhase = phase;
                return noiseSample;
            }

            SampleType getNextSample()
            {
                if (polarity == Polarity::bipolar)
                {
                    if (shape == Shape::triangle)      return shapers::triangle (phase);
                    if (shape == Shape::square)        return shapers::square (phase);
                    if (shape == Shape::rampUp)        return shapers::rampUp (phase);
                    if (shape == Shape::rampDown)      return shapers::rampDown (phase);
                    if (shape == Shape::sine)          return shapers::sine (phase);
                    if (shape == Shape::sampleAndHold) return getNoiseSample();
                }
                else
                {
                    if (shape == Shape::triangle)      return shapers::triangleUnipolar (phase);
                    if (shape == Shape::square)        return shapers::squareUnipolar (phase);
)soul_code"
R"soul_code(

                    if (shape == Shape::rampUp)        return shapers::rampUpUnipolar (phase);
                    if (shape == Shape::rampDown)      return shapers::rampDownUnipolar (phase);
                    if (shape == Shape::sine)          return shapers::sineUnipolar (phase);
                    if (shape == Shape::sampleAndHold) return getNoiseSample();
                }

                return SampleType();
            }

            void init()
            {
                depth.reset (initialDepth * 0.01f);
            }

            void run()
            {
                rng.reset (processor.id + 10);

                loop
                {
                    out << getNextSample() * depth.tick();

                    if (qnMode)
                    {
                        if (timelineSync && transportRunning)
                        {
                            let oneOverQNScalar = 1.0 / qnScalar;
                            qnPos += qnPhaseIncrement;
                            phase = float32 (fmod (qnPos, oneOverQNScalar) / oneOverQNScalar);
                        }
                        else // freewheel
                        {
                            phase += (qnPhaseIncrement * float32 (qnScalar));

                            while (phase >= 1.0f)
                                phase -= 1.0f;
                        }
                    }
                    else
                    {
                        phase += phaseIncrement;

                        while (phase >= 1.0f)
                            phase -= 1.0f;
                    }

                    advance();
                }
            }
        }
    }
}
)soul_code";

    if (moduleName == "soul.frequency")  return R"soul_code(
/**
    Title: Frequency-domain utilities
*/

/// Discrete Fourier Transform functions.
namespace soul::DFT
{
    /// Performs a real forward DFT from an input buffer to an output buffer.
    void forward<SampleBuffer> (const SampleBuffer& inputData, SampleBuffer& outputData)
    {
        static_assert (SampleBuffer.isFixedSizeArray || SampleBuffer.isVector, "The buffers for DFT::forward() must be fixed size arrays");
        static_assert (SampleBuffer.elementType.isFloat && SampleBuffer.elementType.isPrimitive, "The element type for DFT::forward() must be floating point");
        let harmonics = inputData.size / 2;

        SampleBuffer inputImag, outputReal, outputImag;

        performComplex (inputData, inputImag, outputReal, outputImag, 1.0f / float (harmonics));

        outputData[0:harmonics]             = outputReal[0:harmonics];
        outputData[harmonics:harmonics * 2] = outputImag[0:harmonics];
    }

    /// Performs a real inverse DFT from an input buffer to an output buffer.
    void inverse<SampleBuffer> (const SampleBuffer& inputData, SampleBuffer& outputData)
    {
        static_assert (SampleBuffer.isFixedSizeArray || SampleBuffer.isVector, "The buffers for DFT::inverse() must be fixed size arrays");
        static_assert (SampleBuffer.elementType.isFloat && SampleBuffer.elementType.isPrimitive, "The element type for DFT::inverse() must be floating point");
        let harmonics = inputData.size / 2;

        SampleBuffer inputReal, inputImag, outputReal;

        inputReal[0:harmonics] = inputData[harmonics:harmonics * 2];
        inputImag[0:harmonics] = inputData[0:harmonics];

        performComplex (inputReal, inputImag, outputReal, outputData, 1.0f);
    }

    // For internal use by the other functions: performs a O(N^2) complex DFT.
    void performComplex<SampleBuffer> (const SampleBuffer& inputReal,
                                       const SampleBuffer& inputImag,
                                       SampleBuffer& outputReal,
)soul_code"
R"soul_code(

                                       SampleBuffer& outputImag,
                                       SampleBuffer.elementType scaleFactor)
    {
        let size = SampleBuffer.size;

        for (int i = 0; i < size; ++i)
        {
            float64 sumReal, sumImag;

            for (int j = 0; j < size; ++j)
            {
                let angle = SampleBuffer.elementType (twoPi * j * i / size);
                let sinAngle = sin (angle);
                let cosAngle = cos (angle);

                sumReal += inputImag.at(j) * cosAngle + inputReal.at(j) * sinAngle;
                sumImag += inputImag.at(j) * sinAngle - inputReal.at(j) * cosAngle;
            }

            outputImag.at(i) = SampleBuffer.elementType (sumImag) * scaleFactor;
            outputReal.at(i) = SampleBuffer.elementType (sumReal) * scaleFactor;
        }
    }
}
)soul_code";

    if (moduleName == "soul.notes")  return R"soul_code(
/** Title: Musical note utilities

    This module contains various structs which can be used to model note-control events
    coming from sources such as MIDI keyboards.

    The SOUL policy is to keep the use of actual MIDI to a minimum, so we encourage
    incoming MIDI data to be converted into these strongly-typed structures, and to use
    them internally, rather than attempting to work with MIDI bytes like it's the 1980s.
*/

/**
    This namespace contains some types which are handy for representing synthesiser
    note events. They do a similar job to MIDI events, but as strongly-typed structs
    instead of a group of bytes. Things like the midi::MPEParser class generate them.

    The events do contain a channel number, in the same way that MIDI does, but there
    are no restrictions on its range.
*/
namespace soul::note_events
{
    /// Represents a note-on (key-down) event.
    struct NoteOn
    {
        int channel;
        float note;
        float velocity;
    }

    /// Represents a note-off (key-up) event.
    struct NoteOff
    {
        int channel;
        float note;
        float velocity;
    }

    /// Represents a change to the pitch that should be applied to any notes
    /// being played on the channel specified.
    struct PitchBend
    {
        int channel;
        float bendSemitones;
    }

    /// Represents a change to the pressure that should be applied to any notes
    /// being played on the channel specified.
    struct Pressure
    {
        int channel;
        float pressure;
    }

    /// Represents a change to the Y-axis parameter that should be applied to any notes
    /// being played on the channel specified.
    struct Slide
    {
        int channel;
        float slide;
    }

    /// Represents a change to a user-defined control parameter that should be applied
    /// to any notes being played on the channel specified.
    struct Control
    {
        int channel;
        int control;
        float value;
    }
}

)soul_code"
R"soul_code(

//==============================================================================
/**
    Simple voice allocation helpers, which take a single stream of input events,
    and redirect them to an array of target voice processors.
*/
namespace soul::voice_allocators (int mpeMasterChannel = 0)
{
    /** A simple voice-allocator which will find either an inactive voice, or the
        least-recently used active voice if it needs to steal one.
    */
    processor Basic (int voiceCount)  [[ main: false ]]
    {
        input event (soul::note_events::NoteOn,
                     soul::note_events::NoteOff,
                     soul::note_events::PitchBend,
                     soul::note_events::Pressure,
                     soul::note_events::Slide,
                     soul::note_events::Control) eventIn;

        output event (soul::note_events::NoteOn,
                      soul::note_events::NoteOff,
                      soul::note_events::PitchBend,
                      soul::note_events::Pressure,
                      soul::note_events::Slide,
                      soul::note_events::Control) voiceEventOut[voiceCount];

        event eventIn (soul::note_events::NoteOn e)
        {
            wrap<voiceCount> allocatedVoice = 0;
            var allocatedVoiceAge = voiceInfo[allocatedVoice].voiceAge;

            // Find the oldest voice to reuse
            for (int i = 1; i < voiceCount; ++i)
            {
                let age = voiceInfo.at(i).voiceAge;

                if (age < allocatedVoiceAge)
                {
                    allocatedVoiceAge = age;
                    allocatedVoice = wrap<voiceCount>(i);
                }
            }

            // If the voice was previously active, we're stealing it, so send a note off too
            if (voiceInfo[allocatedVoice].active)
            {
                soul::note_events::NoteOff noteOff;

                noteOff.channel = voiceInfo[allocatedVoice].channel;
                noteOff.note    = voiceInfo[allocatedVoice].note;

)soul_code"
R"soul_code(

                voiceEventOut[allocatedVoice] << noteOff;
            }

            // Send the note on to the voice
            voiceEventOut[allocatedVoice] << e;

            // Update the VoiceInfo for our chosen voice
            voiceInfo[allocatedVoice].active       = true;
            voiceInfo[allocatedVoice].noteReleased = false;
            voiceInfo[allocatedVoice].channel      = e.channel;
            voiceInfo[allocatedVoice].note         = e.note;
            voiceInfo[allocatedVoice].voiceAge     = nextAllocatedVoiceAge++;
        }

        event eventIn (soul::note_events::NoteOff e)
        {
            // Release all voices associated with this note/channel
            wrap<voiceCount> voice = 0;

            bool pedalDown = masterSustainActive ? true : channelSustainActive.at (e.channel);

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == e.channel
                     && voiceInfo[voice].note == e.note)
                {
                    if (pedalDown)
                    {
                        // Mark the note as released
                        voiceInfo[voice].noteReleased = true;
                    }
                    else
                    {
                        // Mark the voice as being unused
                        voiceInfo[voice].active   = false;
                        voiceInfo[voice].voiceAge = nextUnallocatedVoiceAge++;

                        voiceEventOut[voice] << e;
                    }
                }

                ++voice;
            }
        }

        event eventIn (soul::note_events::PitchBend e)
        {
            // Forward the pitch bend to all notes on this channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == e.channel)
                    voiceEventOut[voice] << e;

                ++voice;
            }
        }

        event eventIn (soul::note_events::Pressure p)
        {
)soul_code"
R"soul_code(

            // Forward the event to all notes on this channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == p.channel)
                    voiceEventOut[voice] << p;

                ++voice;
            }
        }

        event eventIn (soul::note_events::Slide s)
        {
            // Forward the event to all notes on this channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == s.channel)
                    voiceEventOut[voice] << s;

                ++voice;
            }
        }

        event eventIn (soul::note_events::Control c)
        {
            if (c.control == sustainCC)
            {
                bool pedalDown = c.value >= 0.5f;
                bool isMasterChannel = c.channel == mpeMasterChannel;

                channelSustainActive.at (c.channel) = pedalDown;

                if (isMasterChannel)
                    masterSustainActive = pedalDown;
                    
                if (! pedalDown)
                {
                    // Release any released notes for this channel
                    wrap<voiceCount> voice = 0;

                    loop (voiceCount)
                    {
                        if ((isMasterChannel || voiceInfo[voice].channel == c.channel) &&
                            voiceInfo[voice].active == true &&
                            voiceInfo[voice].noteReleased == true)
                        {
                            soul::note_events::NoteOff noteOff;

                            noteOff.channel = voiceInfo[voice].channel;
                            noteOff.note    = voiceInfo[voice].note;

                            voiceEventOut[voice] << noteOff;

                            voiceInfo[voice].active   = false;
                            voiceInfo[voice].voiceAge = nextUnallocatedVoiceAge++;
                        }

                        ++voice;
)soul_code"
R"soul_code(

                    }
                }

                return;
            }

            // Forward the event to all notes on this channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == c.channel)
                    voiceEventOut[voice] << c;

                ++voice;
            }
        }

        struct VoiceInfo
        {
            bool active;
            int channel;
            float note;
            int voiceAge;
            bool noteReleased;
        }

        int nextAllocatedVoiceAge   = 1000000000;
        int nextUnallocatedVoiceAge = 1;

        let sustainCC = 64;

        VoiceInfo[voiceCount] voiceInfo;
        bool[16] channelSustainActive;
        bool masterSustainActive;
    }
}
)soul_code";

    if (moduleName == "soul.midi")  return R"soul_code(
/** Title: MIDI utilities

    In general, the SOUL policy towards MIDI is to avoid it as much as is humanly possible,
    so most of these helper functions are concerned with converting MIDI messages to
    `soul::note_events` types, and then the other libraries use these strongly-typed
    events to model note actions, rather than dealing with raw MIDI events.
*/

/// Various MIDI-related types and functions.
namespace soul::midi
{
    /// This type is used to represent a packed short MIDI message. When you create
    /// an input event endpoint and would like it to receive MIDI, this is the type
    /// that you should use for it.
    struct Message
    {
        int midiBytes;  /**< Format: (byte[0] << 16) | (byte[1] << 8) | byte[2] */
    }

    /// Extracts the first MIDI byte from a Message object
    int getByte1 (Message m)     { return (m.midiBytes >> 16) & 0xff; }
    /// Extracts the second MIDI byte from a Message object
    int getByte2 (Message m)     { return (m.midiBytes >> 8) & 0xff; }
    /// Extracts the third MIDI byte from a Message object
    int getByte3 (Message m)     { return m.midiBytes & 0xff; }

    /// This event processor receives incoming MIDI events, parses them as MPE,
    /// and then emits a stream of note events using the types in soul::note_events.
    /// A synthesiser can then handle the resulting events without needing to go
    /// near any actual MIDI or MPE data.
    ///
    processor MPEParser  [[ main: false ]]
    {
        input event Message parseMIDI;

        output event (soul::note_events::NoteOn,
                      soul::note_events::NoteOff,
                      soul::note_events::PitchBend,
                      soul::note_events::Pressure,
                      soul::note_events::Slide,
                      soul::note_events::Control) eventOut;

        let MPESlideControllerID = 74;

        event parseMIDI (Message message)
        {
            let messageByte1 = message.getByte1();
            let messageByte2 = message.getByte2();
)soul_code"
R"soul_code(

            let messageByte3 = message.getByte3();

            let messageType  = messageByte1 & 0xf0;
            let channel      = messageByte1 & 0x0f;

            if (messageType == 0x80)
            {
                eventOut << soul::note_events::NoteOff (channel, float (messageByte2), normaliseValue (messageByte3));
            }
            else if (messageType == 0x90)
            {
                // Note on with zero velocity should be treated as a note off
                if (messageByte3 == 0)
                    eventOut << soul::note_events::NoteOff (channel, float (messageByte2), 0);
                else
                    eventOut << soul::note_events::NoteOn (channel, float (messageByte2), normaliseValue (messageByte3));
            }
            else if (messageType == 0xb0)
            {
                if (messageByte2 == MPESlideControllerID)
                    eventOut << soul::note_events::Slide (channel, normaliseValue (messageByte3));
                else
                    eventOut << soul::note_events::Control (channel, messageByte2, normaliseValue (messageByte3));
            }
            else if (messageType == 0xd0)
            {
                eventOut << soul::note_events::Pressure (channel, normaliseValue (messageByte2));
            }
            else if (messageType == 0xe0)
            {
                eventOut << soul::note_events::PitchBend (channel, translateBendSemitones (messageByte3, messageByte2));
            }
        }

        float normaliseValue (int i)
        {
            return i * (1.0f / 127.0f);
        }

        float translateBendSemitones (int msb, int lsb)
        {
            let value = msb * 128 + lsb;
            let bendRange = 48.0f;
            return float (value - 8192) / (8192.0f / bendRange);
        }
    }
}
)soul_code";

    // END_INCLUDED_LIBRARY_CODE

    return nullptr;
}

} // namespace soul
