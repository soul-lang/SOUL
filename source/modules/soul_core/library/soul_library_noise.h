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
    This namespace contains some random number generation helpers.
*/
namespace soul::Random
{
    /** State for a Park-Miller random number generator */
    struct RandomNumberState
    {
        int64 seed;
    }

    /** Advances the given RNG state and returns a value 0 to 1 */
    float getNextUnipolar (RandomNumberState& state)
    {
        let s = (state.seed * 48271) % 0x7fffffff;
        state.seed = s;
        return float (s) * (1.0f / 2147483647.0f);
    }

    /** Advances the given RNG state and returns a value -1 to 1 */
    float getNextBipolar (RandomNumberState& state)
    {
        let s = (state.seed * 48271) % 0x7fffffff;
        state.seed = s;
        return (float (s) * (2.0f / 2147483647.0f)) - 1.0f;
    }
}

/**
    This namespace contains various noise-generation utilities.
*/
namespace soul::Noise
{
    /** White noise generator */
    processor White
    {
        output stream float out;

        void run()
        {
            var rng = Random::RandomNumberState (1);

            loop
            {
                out << Random::getNextBipolar (rng);
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
            var rng = Random::RandomNumberState (1);

            loop
            {
                let white = Random::getNextBipolar (rng);
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
            var rng = Random::RandomNumberState (1);

            loop
            {
                let white = Random::getNextBipolar (rng);
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

)library"
