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

namespace soul::Mixers
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
                out << lerp (in1, in2, mix * (1.0f / mixRange));
                advance();
            }
        }
    }
}

namespace soul::Gain
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
}

)library"
