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
namespace soul
{
    namespace complex_lib
    {
        namespace imp (using FloatType, int vectorSize)
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

)soul_code"
R"soul_code(

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

            complex_element::ComplexType setElement (ComplexType& c, int element, complex_element::ComplexType value)
            {
                c.real.at (element) = value.real;
                c.imag.at (element) = value.imag;

                return value;
            }
        }
    }
}
)soul_code";

    if (moduleName == "soul.noise")  return R"soul_code(
/**
    This namespace contains some random number generation helpers.
*/
namespace soul::random
{
    /** State for a Park-Miller random number generator. */
    struct RandomNumberState
    {
        /** The current seed.
            Top tip: when generating a seed, you might want to use the processor.id constant,
            to make sure that each instance of a processor has a differently-seeded RNG. If you
            want the RNG to be different each time the program runs, you could also throw the
            processor.session constant into the mix too.
        */
        int64 seed;
    }

    void reset (RandomNumberState& state, int64 seed)
    {
        state.seed = seed;
    }

    /** Returns the next number in the full 32-bit integer range. */
    int32 getNextInt32 (RandomNumberState& state)
    {
        let s = (state.seed * 48271) % 0x7fffffff;
        state.seed = s + 1;
        return int32 (s);
    }

    /** Advances the given RNG state and returns a value 0 to 1 */
    float getNextUnipolar (RandomNumberState& state)
    {
        return float (getNextInt32 (state)) * (1.0f / 2147483647.0f);
    }

    /** Advances the given RNG state and returns a value -1 to 1 */
    float getNextBipolar (RandomNumberState& state)
    {
        return (float (getNextInt32 (state)) * (2.0f / 2147483647.0f)) - 1.0f;
    }
}

/**
    This namespace contains various noise-generation utilities.
*/
namespace soul::noise
{
    /** White noise generator */
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

    /** Brown noise generator */
    processor Brown
    {
        output stream float out;

        void run()
        {
            let limit = 32.0f;
            float runningTotal;
            var rng = random::RandomNumberState (processor.id + 20);

)soul_code"
R"soul_code(

            loop
            {
                let white = rng.getNextBipolar();
                runningTotal += white;

                if (runningTotal > limit || runningTotal < -limit)
                    runningTotal -= white;

                runningTotal *= 0.998f;
                out << runningTotal * (1.0f / limit);
                advance();
            }
        }
    }

    /** Pink noise generator */
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
namespace soul
{
    //==============================================================================
    /*
        Some audio-themed helper functions
    */
    float32 dBtoGain (float32 decibels)   { return decibels > -100.0f ? pow (10.0f, decibels * 0.05f) : 0.0f; }
    float64 dBtoGain (float64 decibels)   { return decibels > -100.0  ? pow (10.0,  decibels * 0.05)  : 0.0; }

    float32 gainTodB (float32 gain)       { return gain > 0 ? log10 (gain) * 20.0f : -100.0f; }
    float64 gainTodB (float64 gain)       { return gain > 0 ? log10 (gain) * 20.0  : -100.0; }

    /** Converts a MIDI note (usually in the range 0-127) to a frequency in Hz. */
    float32 noteNumberToFrequency (int note)            { return 440.0f * pow (2.0f, (note - 69) * (1.0f / 12.0f)); }
    /** Converts a MIDI note (usually in the range 0-127) to a frequency in Hz. */
    float32 noteNumberToFrequency (float32 note)        { return 440.0f * pow (2.0f, (note - 69.0f) * (1.0f / 12.0f)); }
    /** Converts a frequency in Hz to an equivalent MIDI note number. */
    float32 frequencyToNoteNumber (float32 frequency)   { return 69.0f + (12.0f / log (2.0f)) * log (frequency * (1.0f / 440.0f)); }

    /** Returns the ratio by which a sample's playback must be sped-up in order to map
        from it native rate and note to a target rate and note.
    */
    float64 getSpeedRatioForPitchedSample (float64 sourceSampleRate, float32 sourceMIDINote,
                                           float64 targetSampleRate, float32 targetMIDINote)
    {
        return (sourceSampleRate * pow (2.0f, targetMIDINote * (1.0f / 12.0f)))
             / (targetSampleRate * pow (2.0f, sourceMIDINote * (1.0f / 12.0f)));
    }
}

//==============================================================================
/** This namespace contains some handy stuctures to use when declaring external
    variables which are going to be loaded with data from audio files.
*/
namespace soul::audio_samples
{
)soul_code"
R"soul_code(

    /** An external variable declared with the type soul::audio_samples::Mono
        can be loaded with monoised data from an audio file.
    */
    struct Mono
    {
        float[] frames;
        float64 sampleRate;
    }

    /** An external variable declared with the type soul::audio_samples::Stereo
        can be loaded with stereo data from an audio file.
    */
    struct Stereo
    {
        float<2>[] frames;
        float64 sampleRate;
    }
}

/** This namespace contains various pan-related helper functions */
namespace soul::pan_law
{
    /** Applies a simple linear pan law to convert a pan position (-1.0 to 1.0) to
        a (left, right) pair of stereo gains.
    */
    float<2> linear (float pan)
    {
        return (1.0f - pan,
                1.0f + pan);
    }

    /** Applies a 3dB-centre pan law to convert a pan position (-1.0 to 1.0) to
        a (left, right) pair of stereo gains.
    */
    float<2> centre3dB (float pan)
    {
        let quarterPi = float (pi / 4);

        return (sin ((1.0f - pan) * quarterPi),
                sin ((1.0f + pan) * quarterPi));
    }
}
)soul_code";

    if (moduleName == "soul.mixing")  return R"soul_code(
/** Utility processors for common tasks like mixing together sources. */
namespace soul::mixers
{
    //==============================================================================
    /** Simple processor which simply sums two sources together with fixed gains
        for each source.
    */
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
    /** Simple processor which simply sums two sources together with streams for the
        gains to apply to each source.
    */
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
)soul_code"
R"soul_code(

                out << lerp (in1, in2, mix * (1.0f / mixRange));
                advance();
            }
        }
    }
}

//==============================================================================
/** Utility processors for common tasks like applying gain in various ways. */
namespace soul::gain
{
    //==============================================================================
    /** Simple processor which applies a fixed gain to a signal. */
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
    /** Simple processor which applies a changable gain level to a signal. */
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
    /** Converts an input event parameter in decibels to a smoothed stream of
        raw gain levels.
    */
    processor SmoothedGainParameter (float slewRateSeconds)
    {
        input event float volume   [[ label: "Volume", unit: "dB", min: -85, max: 6 ]];
        output stream float gain;

        event volume (float targetDB)
        {
            targetGain = soul::dBtoGain (targetDB);
            let maxDelta = float (processor.period / slewRateSeconds);
            remainingRampSamples = max (1, int (abs (targetGain - currentGain) / maxDelta));
            increment = (targetGain - currentGain) / remainingRampSamples;
        }

        float targetGain, currentGain, increment;
)soul_code"
R"soul_code(

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
}

//==============================================================================
/** Generators for common envelope shapes. */
namespace soul::envelope
{
    //==============================================================================
    /** Creates an envelope which applies convex attack and release curves based
        on a stream of NoteOn and NoteOff events.

        The envelope implements fixed-length attack and release ramps where the hold
        level is based on the velocity of the triggering NoteOn event, multiplied
        by the holdLevelMultiplier parameter.
    */
    processor FixedAttackReleaseEnvelope (float holdLevelMultiplier,
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
)soul_code"
R"soul_code(

                else
                {
                    let attackSamples = int (processor.frequency * attackTimeSeconds);
                    let attackMultiplier = float (pow (2.0, -1.0 / attackSamples) * pow (targetLevel + 2.0, 1.0 / attackSamples));

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
                    let releaseMultiplier = float (pow (0.0001, processor.period / releaseTimeSeconds));

                    while (! active && level > silenceThreshold)
                    {
                        levelOut << level;
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
/*  The intrinsics namespace contains basic functions, many of which will be performed
    by a JIT engine in more efficient ways than the default function implementations
    that are supplied here. The "[[intrin]]" annotation is used as a hint to the performer
    back-end that this function should be replaced by a native implementation if
    one is available. (Note that some of the functions here have non-functional
    implementations and rely on the performer to use a native implementation for them to
    work at all).
*/
namespace soul::intrinsics
{
    /** Familiar abs() function, accepting most scalar types. */
    T.removeReference abs<T> (T n)                         [[intrin: "abs"]]       { static_assert (T.isScalar, "abs() only works with scalar types"); return n < 0 ? -n : n; }
    /** Returns the greater of two scalar values. */
    T.removeReference max<T>       (T a, T b)              [[intrin: "max"]]       { static_assert (T.isScalar, "max() only works with scalar types"); return a > b ? a : b; }
    /** Returns the lesser of two scalar values. */
    T.removeReference min<T>       (T a, T b)              [[intrin: "min"]]       { static_assert (T.isScalar, "min() only works with scalar types"); return a < b ? a : b; }
    /** Returns the lesser of two scalar values. */
    int32 min                      (int32 a, int32 b)      [[intrin: "min"]]       { return a < b ? a : b; }

    /** Clamps a scalar value to the nearest value within a given range. */
    T.removeReference clamp<T>     (T n, T low, T high)    [[intrin: "clamp"]]     { static_assert (T.isScalar, "clamp() only works with scalar types"); return n < low ? low : (n > high ? high : n); }
    /** Performs a negative-number-aware modulo operation to wrap a number to a zero-based range. */
    T.removeReference wrap<T>      (T n, T range)          [[intrin: "wrap"]]      { static_assert (T.isScalar, "wrap() only works with scalar types");  if (range == 0) return 0; let x = n % range; if (x < 0) return x + range; return x; }
)soul_code"
R"soul_code(

    /** Performs a negative-number-aware integer modulo operation. */
    int32 wrap                     (int32 n, int32 range)  [[intrin: "wrap"]]      { if (range == 0) return 0; let x = n % range; if (x < 0) return x + range; return x; }
    /** Performs a C++-compatible floor function on a scalar floating point value. */
    T.removeReference floor<T>     (T n)                   [[intrin: "floor"]]     { static_assert (T.isScalar && T.primitiveType.isFloat, "floor() only works with scalar floating point types");     let r = T (int64 (n)); return (r == n) ? n : (n >= 0 ? r : r - 1); }
    /** Performs a C++-compatible ceil function on a scalar floating point value. */
    T.removeReference ceil<T>      (T n)                   [[intrin: "ceil"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "ceil() only works with scalar floating point types");      let r = T (int64 (n)); return (r == n) ? n : (n >= 0 ? r + 1 : r); }
    /** Returns a linearly-interpolated value between two scalar values. */
    T.removeReference lerp<T>      (T start, T stop, T amount)                     { static_assert (T.isScalar && T.primitiveType.isFloat, "lerp() only works with scalar floating point types");      return start + (stop - start) * amount; }

    /** Performs a C++-compatible fmod function on some scalar floating point values. */
    T.removeReference fmod<T>      (T x, T y)              [[intrin: "fmod"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "fmod() only works with scalar floating point types");      return x - (y * T (int (x / y))); }
    /** Performs a C++-compatible remainder function on some scalar floating point values. */
    T.removeReference remainder<T> (T x, T y)              [[intrin: "remainder"]] { static_assert (T.isScalar && T.primitiveType.isFloat, "remainder() only works with scalar floating point types"); return x - (y * T (int (0.5f + x / y))); }
    /** Returns the square root of a scalar floating point value. */
)soul_code"
R"soul_code(

    T.removeReference sqrt<T>      (T n)                   [[intrin: "sqrt"]]      { static_assert (T.isScalar && T.primitiveType.isFloat, "sqrt() only works with scalar floating point types");      return T(); }
    /** Raises a scalar floating point value to the given power. */
    T.removeReference pow<T>       (T a, T b)              [[intrin: "pow"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "pow() only works with scalar floating point types");       return T(); }
    /** Returns the exponential of a scalar floating point value. */
    T.removeReference exp<T>       (T n)                   [[intrin: "exp"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "exp() only works with scalar floating point types");       return T(); }
    /** Returns the log-e of a scalar floating point value. */
    T.removeReference log<T>       (T n)                   [[intrin: "log"]]       { static_assert (T.isScalar && T.primitiveType.isFloat, "log() only works with scalar floating point types");       return T(); }
    /** Returns the log 10 of a scalar floating point value. */
    T.removeReference log10<T>     (T n)                   [[intrin: "log10"]]     { static_assert (T.isScalar && T.primitiveType.isFloat, "log10() only works with scalar floating point types");     return T(); }

    /** Rounds a floating point number up or down to the nearest integer. */
    int32 roundToInt (float32 n)    [[intrin: "roundToInt"]]                       { return int32 (n + (n < 0 ? -0.5f : 0.5f)); }
    /** Rounds a floating point number up or down to the nearest integer. */
    int64 roundToInt (float64 n)    [[intrin: "roundToInt"]]                       { return int64 (n + (n < 0 ? -0.5 : 0.5)); }

    /** Returns true if the floating point argument is a NaN. */
    bool isnan<T> (T n)  [[intrin: "isnan"]]       { static_assert (T.isPrimitive && T.primitiveType.isFloat, "isnan() only works with floating point types"); return false; }
)soul_code"
R"soul_code(

    /** Returns true if the floating point argument is an INF. */
    bool isinf<T> (T n)  [[intrin: "isinf"]]       { static_assert (T.isPrimitive && T.primitiveType.isFloat, "isinf() only works with floating point types"); return false; }

    /** Adds an delta to a value, and returns the resulting value modulo PI/2.
        A typical use-case for this is in incrementing the phase of an oscillator.
    */
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

    /** Returns the sum of an array or vector of scalar values. */
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
                total += sum (r);
            }

            return total;
        }
        else if const (T.isFixedSizeArray || T.isVector)
        {
            var total = t[0];
            wrap<t.size> i;

            loop (t.size - 1)
)soul_code"
R"soul_code(

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

    /** Returns the product of an array or vector of scalar values. */
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

            var result = t[0];

            for (int i = 1; i < t.size; ++i)
                result *= t[i];

            return result;
        }
    }

    /** Reads an element from an array, allowing the index to be any type of floating point type.
)soul_code"
R"soul_code(

        If a floating point index is used, it will be rounded down to an integer index - for an
        interpolated read operation, see readLinearInterpolated(). Indexes beyond the range of the
        array will be wrapped.
    */
    Array.elementType read<Array, IndexType> (const Array& array, IndexType index) [[intrin: "read"]]
    {
        static_assert (Array.isArray, "read() only works with array types");
        static_assert (IndexType.isPrimitive || IndexType.isInt, "The index for read() must be a floating point or integer value");

        return array.at (int (index));
    }

    /** Reads a linearly-interpolated value from an array of some kind of scalar values (probably
        a float or float-vector type). Indexes beyond the range of the array will be wrapped.
    */
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

        return sample1 + (sample2 - sample1) * Array.elementType (index - indexFloor);
    }

    // NB: this is used internally, not something you'd want to call from user code
    int get_array_size<Array> (const Array& array) [[intrin: "get_array_size"]];


    // Trigonometry functions

)soul_code"
R"soul_code(

    T sin<T>   (T n)  [[intrin: "sin"]]      { static_assert (T.isPrimitive || T.isVector, "sin() only works with floating point types");   static_assert (T.primitiveType.isFloat, "sin() only works with floating point types");   return T(); }
    T cos<T>   (T n)  [[intrin: "cos"]]      { static_assert (T.isPrimitive || T.isVector, "cos() only works with floating point types");   static_assert (T.primitiveType.isFloat, "cos() only works with floating point types");   return T(); }
    T tan<T>   (T n)  [[intrin: "tan"]]      { static_assert (T.isPrimitive || T.isVector, "tan() only works with floating point types");   static_assert (T.primitiveType.isFloat, "tan() only works with floating point types");   return sin (n) / cos (n); }
    T sinh<T>  (T n)  [[intrin: "sinh"]]     { static_assert (T.isPrimitive || T.isVector, "sinh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "sinh() only works with floating point types");  return (exp (n) - exp (-n)) / 2; }
    T cosh<T>  (T n)  [[intrin: "cosh"]]     { static_assert (T.isPrimitive || T.isVector, "cosh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "cosh() only works with floating point types");  return (exp (n) + exp (-n)) / 2; }
    T tanh<T>  (T n)  [[intrin: "tanh"]]     { static_assert (T.isPrimitive || T.isVector, "tanh() only works with floating point types");  static_assert (T.primitiveType.isFloat, "tanh() only works with floating point types");  return sinh (n) / cosh (n); }
    T asinh<T> (T n)  [[intrin: "asinh"]]    { static_assert (T.isPrimitive || T.isVector, "asinh() only works with floating point types"); static_assert (T.primitiveType.isFloat, "asinh() only works with floating point types"); return log (n + sqrt (n * n + 1)); }
)soul_code"
R"soul_code(

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

            return 0;  // Undefined case: return 0
        }

        let atanYoverX = atan (y / x);

        if (x > 0)
            return atanYoverX;

        return y >= 0 ? atanYoverX + T (pi)
                      : atanYoverX - T (pi);
    }

    namespace helpers
)soul_code"
R"soul_code(

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
/**
    This namespace contains various types and functions which are used when
    dealing with positions and tempos along a timeline.
*/
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
    float quarterNotesPerBeat (TimeSignature timeSig)                                       { return 4.0f / timeSig.denominator; }
    float beatsPerQuarterNote (TimeSignature timeSig)                                       { return timeSig.denominator / 4.0f; }

    float secondsPerBeat (Tempo tempo)                                                      { return tempo.bpm <= 0 ? 0.0f : (60.0f / tempo.bpm); }
    float secondsPerQuarterNote (Tempo tempo, TimeSignature timeSig)                        { return tempo.secondsPerBeat() * timeSig.beatsPerQuarterNote(); }

    float64 framesPerBeat (Tempo tempo, float64 sampleRate)                                 { return sampleRate * tempo.secondsPerBeat(); }
    float64 framesPerQuarterNote (Tempo tempo, TimeSignature timeSig, float64 sampleRate)   { return sampleRate * tempo.secondsPerQuarterNote (timeSig); }

    //==============================================================================
    /** Represents the state of a host which can play timeline-based material. */
    struct TransportState
    {
        /** In the absence of enums, the valid values for the state are:
            0 = stopped, 1 = playing, 2 = recording.
        */
        int state;
    }

)soul_code"
R"soul_code(

    bool isStopped   (TransportState t)       { return t.state == 0; }
    bool isPlaying   (TransportState t)       { return t.state == 1; }
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

    if (moduleName == "soul.oscillators")  return R"soul_code(
/**
    This namespace contains various simple oscillators
*/
namespace soul::oscillators
{
    using PhaseType = float32;  // TODO: this should probably be a defaulted namespace parameterisation
    let minFreqHz = 0.0;
    let maxFreqHz = 22000.0;
    let initFreqHz = 1000.0;

    //==============================================================================
    /** A unipolar ramp (phasor) oscillator.
        This is a non-bandlimited oscillator that will cause aliasing!
    */
    namespace phasor
    {
        struct State
        {
            PhaseType phase; // range 0 - 1
            PhaseType phaseIncrement;
        }

        void reset (State& s)
        {
            s.phase = PhaseType();
        }

        void update (State& s, float64 samplePeriod, float64 freqHz)
        {
            s.phaseIncrement = PhaseType (samplePeriod * freqHz);
        }

        PhaseType process (State& s)
        {
            s.phase += s.phaseIncrement;

            while (s.phase >= PhaseType (1.0))
                s.phase -= PhaseType (1.0);

            return s.phase;
        }

        processor Processor (using SampleType, float initialFrequency)
        {
            output stream SampleType out;

            input event float frequencyIn [[ name: "Frequency", min: minFreqHz, max: maxFreqHz, init: initFreqHz ]];

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
    /** A simple sinewave oscillator.
    */
    processor Sine (using SampleType, float initialFrequency)
    {
        output stream SampleType out;

)soul_code"
R"soul_code(

        input event float frequencyIn  [[ name: "Frequency", min: minFreqHz, max: maxFreqHz, init: initFreqHz ]];

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
    /** A semi band-limited oscillator with sawtooth, square and triangle wave shapes
        using the PolyBLEP (Polynomial Band-Limited Step) technique.
        You may want to oversample this oscillator, in order to reduce aliasing
    */
    namespace poly_blep (using SampleType)
    {
        namespace shapers
        {
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

            PhaseType sawtooth (phasor::State& s)
            {
                return PhaseType (-1) + (PhaseType (2) * s.phase) - polyblep (s.phase, s.phaseIncrement);
            }

            PhaseType square (phasor::State& s)
            {
                return (s.phase < PhaseType (0.5) ? PhaseType (-1) : PhaseType (1))
                        - polyblep (s.phase, s.phaseIncrement)
                        + polyblep (fmod (s.phase + PhaseType (0.5), PhaseType (1)), s.phaseIncrement);
)soul_code"
R"soul_code(

            }
        }

        namespace Shape
        {
            let sawtooth = 0;
            let triangle = 1;
            let square   = 2;
        }

        processor Processor (int initialShape, float initialFrequency)
        {
            output stream SampleType out;

            input event
            {
                float shapeIn       [[ name: "Shape",     min: 0,         max: 2,         init: 0,    text: "Sawtooth|Triangle|Square"]];
                float frequencyIn   [[ name: "Frequency", min: minFreqHz, max: maxFreqHz, init: initFreqHz ]];
            }

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

)soul_code"
R"soul_code(

    //==============================================================================
    /** A quadrature sinusoidal oscillator producing sine and cosine outputs simultaneously
        https://vicanek.de/articles/QuadOsc.pdf
    */
    namespace quadrature (using SampleType)
    {
        let updateInterval = 16; // limit coefficient updates to every 16 samples

        struct State
        {
            SampleType u, v;
        }

        struct Coeffs
        {
            SampleType k1, k2;
        }

        void reset (State& s)
        {
            s.u = SampleType (1);
            s.v = SampleType();
        }

        void update (Coeffs& c, float64 freqHz, float64 sampleRate)
        {
            let w = twoPi * (freqHz / sampleRate);
            c.k1 = SampleType (tan (0.5 * w));
            c.k2 = SampleType (2.0 * c.k1 / (1.0 + c.k1 * c.k1));
        }

        SampleType<2> process (State& s, Coeffs& c)
        {
            let tmp = s.u - c.k1 * s.v;
            s.v = s.v + c.k2 * tmp;
            s.u = tmp - c.k1 * s.v;
            return SampleType<2> (s.v, s.u);
        }

        processor Processor (float initialFrequency)
        {
            output stream SampleType sineOut;
            output stream SampleType cosineOut;

            input event float frequencyIn [[ name: "Frequency", min: -maxFreqHz, max: maxFreqHz, init: 0.0 ]]; // Default meta data allows negative frequencies

            event frequencyIn (float v) { frequency = v; recalc = true; }

            float frequency = initialFrequency;
            bool recalc = true;

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
)soul_code"
R"soul_code(

                        sineOut << y[0];
                        cosineOut << y[1];
                        advance();
                    }
                }
            }
        }
    }

    //==============================================================================
    /** A tempo-syncable LFO with some common shapes and options for uni-polar or bi-polar output
    */
    namespace lfo
    {
        using SampleType = float32;

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

        namespace Shape
        {
            let triangle = 0;
            let square   = 1;
            let rampUp   = 2;
            let rampDown = 3;
            let sine     = 4;
            let SAH      = 5;
        }

        namespace Divs
        {
            let k64n  = 0;
            let k32n  = 1;
            let k16nt = 2;
            let k16n  = 3;
            let k16nd = 4;
            let k8nt  = 5;
            let k8n   = 6;
            let k8nd  = 7;
            let k4n   = 8;
            let k4nd  = 9;
)soul_code"
R"soul_code(

            let k2n   = 10;
            let k1bar = 11;
            let k2bar = 12;
            let k4bar = 13;
            let k8bar = 14;
        }

        namespace DivScalars
        {
            let k64n  = 64.0   / 4.0; // 1 sixty fourth of a beat
            let k32n  = 32.0   / 4.0; // 1 thirty second of a beat
            let k16nt = 24.0   / 4.0; // 1 sixteenth note triplet
            let k16n  = 16.0   / 4.0; // 1 sixteenth note
            let k16nd = 12.0   / 4.0; // 1 dotted sixteenth note
            let k8nt  = 9.0    / 4.0; // 1 eighth note triplet
            let k8n   = 8.0    / 4.0; // 1 eighth note
            let k8nd  = 6.0    / 4.0; // 1 dotted eighth note
            let k4n   = 4.0    / 4.0; // 1 quater note / 1 beat @ 4/4
            let k4nd  = 3.0    / 4.0; // 1 dotted quater note @ 4/4
            let k2n   = 2.0    / 4.0; // 2 beats @ 4/4
            let k1bar = 1.0    / 4.0; // 1 bar @ 4/4
            let k2bar = 0.5    / 4.0; // 2 bars @ 4/4
            let k4bar = 0.25   / 4.0; // 4 bars @ 4/4
            let k8bar = 0.125  / 4.0; // 8 bars @ 4/4
        }

        namespace Polarity
        {
            let unipolar = 0;
            let bipolar  = 1;
        }

        namespace smoother
        {
            struct State
            {
                float currentValue;
                float targetValue;
                float increment;
                int steps;
            }

            void reset (State& state, float initialValue)
            {
                state.currentValue = initialValue;
                state.targetValue = initialValue;
                state.increment = 0.0f;
                state.steps = 0;
            }

            void setTarget (State& state, float targetValue, int steps)
            {
                state.targetValue = targetValue;
                state.increment = (state.targetValue - state.currentValue) / steps;
                state.steps = steps;
            }

            float tick (State& state)
            {
)soul_code"
R"soul_code(

                if (state.steps == 0)
                    return state.currentValue;

                state.currentValue += state.increment;
                state.steps--;

                if (state.steps == 0)
                    state.currentValue = state.targetValue;

                return state.currentValue;
            }
        }

        processor Processor (int initialShape, int initialPolarity, float initialDepth, float initialFreq)
        {
            output stream SampleType out;
            input event soul::timeline::Position positionIn;
            input event soul::timeline::TransportState transportStateIn;
            input event soul::timeline::Tempo tempoIn;

            input event
            {
                float rateHzIn     [[ name: "Rate (Hz)",     min: 0.01,  max: 40.0,   init: 1.,   unit: "hz", step: 0.01 ]];
                float rateTempoIn  [[ name: "Rate (Tempo)",  min: 0,     max: 14,     init: 0,    text: "1/64|1/32|1/16T|1/16|1/16D|1/8T|1/8|1/8D|1/4|1/4D|1/2|1/1|2/1|4/1|8/1"]];
                float depthIn      [[ name: "Depth",         min: 0,     max: 100,    init: 100,  unit: "%",  step: 1 ]];
                float shapeIn      [[ name: "Shape",         min: 0,     max: 5,      init: 0,    text: "Triangle|Square|Ramp Up|Ramp Down|Sine|Sample & Hold"]];
                float polarityIn   [[ name: "Polarity",      min: 0,     max: 1,      init: 0,    text: "Unipolar|Bipolar"]];
                float rateModeIn   [[ name: "Rate Mode",     min: 0,     max: 1,      init: 0,    text: "Hz|Tempo"]];
                float syncIn       [[ name: "Timeline Sync", min: 0,     max: 1,      init: 0,    text: "Off|On"]];
            }

            event positionIn (soul::timeline::Position v)             { qnPos = v.currentQuarterNote; }
            event transportStateIn (soul::timeline::TransportState v) { transportRunning = v.state > 0; }
            event tempoIn (soul::timeline::Tempo v)                   { qnPhaseIncrement = (v.bpm / 60.0f) * float (processor.period); }

)soul_code"
R"soul_code(

            event rateHzIn (float v)
            {
                if (!qnMode)
                    phaseIncrement = PhaseType (v * processor.period);
            }

            event rateTempoIn (float v)
            {
                let div = int(floor(v));

                if      (div == Divs::k64n)  qnScalar = DivScalars::k64n;
                else if (div == Divs::k32n)  qnScalar = DivScalars::k32n;
                else if (div == Divs::k16nt) qnScalar = DivScalars::k16nt;
                else if (div == Divs::k16n)  qnScalar = DivScalars::k16n;
                else if (div == Divs::k16nd) qnScalar = DivScalars::k16nd;
                else if (div == Divs::k8nt)  qnScalar = DivScalars::k8nt;
                else if (div == Divs::k8n )  qnScalar = DivScalars::k8n;
                else if (div == Divs::k8nd)  qnScalar = DivScalars::k8nd;
                else if (div == Divs::k4n )  qnScalar = DivScalars::k4n;
                else if (div == Divs::k4nd)  qnScalar = DivScalars::k4nd;
                else if (div == Divs::k2n )  qnScalar = DivScalars::k2n;
                else if (div == Divs::k1bar) qnScalar = DivScalars::k1bar;
                else if (div == Divs::k2bar) qnScalar = DivScalars::k2bar;
                else if (div == Divs::k4bar) qnScalar = DivScalars::k4bar;
                else if (div == Divs::k8bar) qnScalar = DivScalars::k8bar;
            }

            event depthIn (float v)       { depth.setTarget (v * 0.01f, smoothingSamples); }
            event shapeIn (float v)       { shape = int (floor (v)); }
            event polarityIn (float v)    { polarity = (v < 0.5f) ? Polarity::unipolar : Polarity::bipolar; }
            event rateModeIn (float v)    { qnMode = v > 0.5f; }
            event syncIn (float v)        { timelineSync = v > 0.5f; }

            PhaseType phase;
            var phaseIncrement = float32 (initialFreq * processor.period);
            int shape = initialShape;
            int polarity = initialPolarity;
            smoother::State depth;

)soul_code"
R"soul_code(

            let smoothingSamples = int (processor.frequency * 0.02);
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
                    noiseSample = SampleType (rng.getNextBipolar());

                prevPhase = phase;
                return noiseSample;
            }

            SampleType getNextSample()
            {
                if (polarity == Polarity::bipolar)
                {
                    if (shape == Shape::triangle)  return shapers::triangle (phase);
                    if (shape == Shape::square)    return shapers::square (phase);
                    if (shape == Shape::rampUp)    return shapers::rampUp (phase);
                    if (shape == Shape::rampDown)  return shapers::rampDown (phase);
                    if (shape == Shape::sine)      return shapers::sine (phase);
                    if (shape == Shape::SAH)       return getNoiseSample();
                }
                else
                {
                    if (shape == Shape::triangle)  return shapers::triangleUnipolar (phase);
                    if (shape == Shape::square)    return shapers::squareUnipolar (phase);
                    if (shape == Shape::rampUp)    return shapers::rampUpUnipolar (phase);
                    if (shape == Shape::rampDown)  return shapers::rampDownUnipolar (phase);
                    if (shape == Shape::sine)      return shapers::sineUnipolar (phase);
                    if (shape == Shape::SAH)       return getNoiseSample();
                }

                return 0.0f;
            }

            void init()
)soul_code"
R"soul_code(

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
/** Discrete Fourier Transform functions. */
namespace soul::DFT
{
    /** Performs a real forward DFT from an input buffer to an output buffer. */
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

    /** Performs a real inverse DFT from an input buffer to an output buffer. */
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

    /** For internal use by the other functions: performs a O(N^2) complex DFT. */
    void performComplex<SampleBuffer> (const SampleBuffer& inputReal,
                                       const SampleBuffer& inputImag,
                                       SampleBuffer& outputReal,
                                       SampleBuffer& outputImag,
)soul_code"
R"soul_code(

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
/**
    This namespace contains some types which are handy for representing synthesiser
    note events. They do a similar job to MIDI events, but as strongly-typed structs
    instead of a group of bytes. Things like the midi::MPEParser class generate them.
*/
namespace soul::note_events
{
    struct NoteOn
    {
        int channel;
        float note;
        float velocity;
    }

    struct NoteOff
    {
        int channel;
        float note;
        float velocity;
    }

    struct PitchBend
    {
        int channel;
        float bendSemitones;
    }

    struct Pressure
    {
        int channel;
        float pressure;
    }

    struct Slide
    {
        int channel;
        float slide;
    }

    struct Control
    {
        int channel;
        int control;
        float value;
    }
}

//==============================================================================
/**
    Various simple voice allocation processors, which take a single stream of
    input events, and redirect them to an array of target voices.
*/
namespace soul::voice_allocators
{
    /** A simple allocator which chooses either an inactive voice, or the
        least-recently used active one if it needs to steal.
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
)soul_code"
R"soul_code(

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

            // Send the note on to the voice
            voiceEventOut[allocatedVoice] << e;

            // If the voice was previously active, we're stealing it, so send a note off too
            if (voiceInfo[allocatedVoice].active)
            {
                soul::note_events::NoteOff noteOff;

                noteOff.channel = voiceInfo[allocatedVoice].channel;
                noteOff.note    = voiceInfo[allocatedVoice].note;

                voiceEventOut[allocatedVoice] << noteOff;
            }

            // Update the VoiceInfo for our chosen voice
            voiceInfo[allocatedVoice].active   = true;
            voiceInfo[allocatedVoice].channel  = e.channel;
            voiceInfo[allocatedVoice].note     = e.note;
            voiceInfo[allocatedVoice].voiceAge = nextAllocatedVoiceAge++;
        }

        event eventIn (soul::note_events::NoteOff e)
        {
            // Release all voices associated with this note/channel
            wrap<voiceCount> voice = 0;

            loop (voiceCount)
            {
                if (voiceInfo[voice].channel == e.channel
                     && voiceInfo[voice].note == e.note)
                {
                    // Mark the voice as being unused
                    voiceInfo[voice].active   = false;
                    voiceInfo[voice].voiceAge = nextUnallocatedVoiceAge++;

                    voiceEventOut[voice] << e;
                }

                ++voice;
            }
        }

        event eventIn (soul::note_events::PitchBend e)
        {
)soul_code"
R"soul_code(

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
        }

        int nextAllocatedVoiceAge   = 1000000000;
        int nextUnallocatedVoiceAge = 1;

        VoiceInfo[voiceCount] voiceInfo;
    }
}
)soul_code";

    if (moduleName == "soul.midi")  return R"soul_code(
/**
    Some MIDI-related functions and processors.

    In general, the SOUL policy towards MIDI is to avoid using it any more than is
    unavoidable, so most of the the helper functions are concerned with converting
    MIDI messages to soul::note_events types, and all the other synth helper libraries
    use these strongly-typed events rather than raw MIDI events.
*/
namespace soul::midi
{
    /** This type is used to represent a packed short MIDI message. When you create
        an input event endpoint and would like it to receive MIDI, this is the type
        that you should use for it.
    */
    struct Message
    {
        int midiBytes;  /**< Format: (byte[0] << 16) | (byte[1] << 8) | byte[2] */
    }

    int getByte1 (Message m)     { return (m.midiBytes >> 16) & 0xff; }
    int getByte2 (Message m)     { return (m.midiBytes >> 8) & 0xff; }
    int getByte3 (Message m)     { return m.midiBytes & 0xff; }

    /** This event processor receives incoming MIDI events, parses them as MPE,
        and then emits a stream of note events using the types in soul::note_events.
        A synthesiser can then handle the resulting events without needing to go
        near any actual MIDI or MPE data.
    */
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
            let messageByte3 = message.getByte3();

            let messageType  = messageByte1 & 0xf0;
            let channel      = messageByte1 & 0x0f;

            if (messageType == 0x80)
            {
)soul_code"
R"soul_code(

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
