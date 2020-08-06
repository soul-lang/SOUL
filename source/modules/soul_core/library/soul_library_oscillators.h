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

/*  The following string literal forms part of a set of SOUL code chunks that form
    the built-in library. (See the soul::getBuiltInLibraryCode() function)
*/
R"library(

/**
    This namespace contains various simple oscillators, including sine, square,
    sawtooth and triangle wave generators.
*/
namespace soul::oscillator
{
    //==============================================================================
    /** A sinewave oscillator.
        This processor has inputs which can be sent events to change its frequency and gain level.
    */
    processor Sine (using SampleType, float initialFrequency, float initialOutputLevel)
    {
        output stream SampleType out;

        input event float frequency; /**< Send these events to change the frequency */
        input event float level;     /**< Send these events to change the gain level */

        event frequency (float newFrequency)    { phaseIncrement = float (newFrequency * twoPi * processor.period); }
        event level (float newLevel)            { gain = newLevel; }

        var phaseIncrement = float (initialFrequency * twoPi * processor.period);
        var gain = initialOutputLevel;

        void run()
        {
            float phase;

            loop
            {
                out << SampleType (gain * sin (phase));
                phase = addModulo2Pi (phase, phaseIncrement);
                advance();
            }
        }
    }

    //==============================================================================
    /** A sawtooth oscillator.
        This processor has inputs which can be sent events to change its frequency and gain level.
    */
    processor Sawtooth (using SampleType, float initialFrequency, float initialOutputLevel)
    {
        output stream SampleType out;

        input event float frequency; /**< Send these events to change the frequency */
        input event float level;     /**< Send these events to change the gain level */

        event frequency (float newFrequency)    { phaseIncrement = float (newFrequency * processor.period); }
        event level (float newLevel)            { gain = newLevel; }

        var phaseIncrement = float (initialFrequency * processor.period);
        var gain = initialOutputLevel;

        void run()
        {
            float phase;

            loop
            {
                out << SampleType (gain * polyblep::sawtooth (phase, phaseIncrement));
                phase += phaseIncrement;

                while (phase >= 1.0f)
                    phase -= 1.0f;

                advance();
            }
        }
    }

    //==============================================================================
    /** A square-wave oscillator.
        This processor has inputs which can be sent events to change its frequency and gain level.
    */
    processor Square (using SampleType, float initialFrequency, float initialOutputLevel)
    {
        output stream SampleType out;

        input event float frequency; /**< Send these events to change the frequency */
        input event float level;     /**< Send these events to change the gain level */

        event frequency (float newFrequency)    { phaseIncrement = float (newFrequency * processor.period); }
        event level (float newLevel)            { gain = newLevel; }

        var phaseIncrement = float (initialFrequency * processor.period);
        var gain = initialOutputLevel;

        void run()
        {
            float phase;

            loop
            {
                out << SampleType (gain * polyblep::square (phase, phaseIncrement));
                phase += phaseIncrement;

                while (phase >= 1.0f)
                    phase -= 1.0f;

                advance();
            }
        }
    }

    //==============================================================================
    /** A triangle oscillator.
        This processor has inputs which can be sent events to change its frequency and gain level.
    */
    processor Triangle (using SampleType, float initialFrequency, float initialOutputLevel)
    {
        output stream SampleType out;

        input event float frequency; /**< Send these events to change the frequency */
        input event float level;     /**< Send these events to change the gain level */

        event frequency (float newFrequency)    { phaseIncrement = float (newFrequency * processor.period); }
        event level (float newLevel)            { gain = newLevel; }

        var phaseIncrement = float (initialFrequency * processor.period);
        var gain = initialOutputLevel;

        void run()
        {
            float phase;
            float currentValue = 1.0f;

            loop
            {
                currentValue += 4.0f * phaseIncrement * polyblep::square (phase, phaseIncrement);
                out << SampleType (gain * currentValue);
                phase += phaseIncrement;

                while (phase >= 1.0f)
                    phase -= 1.0f;

                advance();
            }
        }
    }

    //==============================================================================
    /** Some PolyBLEP (Polynomial Band-Limited Step) functions, mainly for use by the oscillator classes. */
    namespace polyblep
    {
        //==============================================================================
        /** A simple PolyBLEP (Polynomial Band-Limited Step) function.
            This expects the phase to be in the range 0 -> 1
        */
        Type polyblep<Type> (Type phase, Type phaseIncrement)
        {
            if (phase < phaseIncrement)
            {
                phase /= phaseIncrement;
                return (phase + phase) - (phase * phase) - Type (1);
            }

            if (phase > Type (1) - phaseIncrement)
            {
                phase = (phase - Type (1)) / phaseIncrement;
                return (phase * phase) + (phase + phase) + Type (1);
            }

            return Type (0);
        }

        /** Uses a PolyBLEP (Polynomial Band-Limited Step) to calculate a sawtooth wave.
            This expects the phase to be in the range 0 -> 1
        */
        Type sawtooth<Type> (Type phase, Type phaseIncrement)
        {
            return Type (-1) + (Type (2) * phase) - polyblep (phase, phaseIncrement);
        }

        /** Uses a PolyBLEP (Polynomial Band-Limited Step) to calculate a square wave.
            This expects the phase to be in the range 0 -> 1
        */
        Type square<Type> (Type phase, Type phaseIncrement)
        {
            return (phase < Type (0.5f) ? Type (-1) : Type (1))
                    - polyblep (phase, phaseIncrement)
                    + polyblep (fmod (phase + Type (0.5f), Type (1)), phaseIncrement);
        }
    }
}

)library"
