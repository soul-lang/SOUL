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

/** A sinc interpolator that can resample a chunk of audio data to fit a new number of frames. */
template <typename DestType, typename SourceType>
void resampleToFit (DestType dest, SourceType source, int zeroCrossings = 50)
{
    SOUL_ASSERT (dest.numChannels == source.numChannels);
    using SampleType = typename DestType::SampleType;
    static constexpr auto localPi = SampleType (soul::pi);

    struct Resampler
    {
        static void resample (DestType dest, const SourceType& source, int zeroCrossings)
        {
            if (dest.numFrames < source.numFrames)
            {
                AllocatedChannelSet<DestType> bandlimitedInput (1, source.numFrames);
                resample (bandlimitedInput.channelSet, source, float (dest.numFrames) / float (source.numFrames), zeroCrossings);
                resample (dest, bandlimitedInput.channelSet, 1.0f, zeroCrossings);
            }
            else
            {
                resample (dest, source, 1.0f, zeroCrossings);
            }
        }

        static void resample (DestType dest, const SourceType& source, float ratio, int zeroCrossings) noexcept
        {
            auto sampleIncrement = double (source.numFrames) / double (dest.numFrames);
            auto destFrames = dest.getChannel (0);

            for (uint32_t i = 0; i < dest.numFrames; ++i)
            {
                *destFrames = static_cast<SampleType> (ratio * getBandlimitedSample (source, sampleIncrement * i, ratio, zeroCrossings));
                destFrames += dest.stride;
            }
        }

        static SampleType getBandlimitedSample (const SourceType& source, double pos, float ratio, int numZeroCrossings) noexcept
        {
            auto intPos  = (int64_t) pos;
            auto fracPos = (float) (pos - (double) intPos);
            auto result = SampleType();
            auto floatZeroCrossings = SampleType (numZeroCrossings);
            auto data = source.getChannel (0);

            if (fracPos > 0)
            {
                fracPos = 1.0f - fracPos;
                ++intPos;
            }

            int crossings = int (float (numZeroCrossings) / ratio);

            for (int i = -crossings; i <= crossings; ++i)
            {
                auto sincPosition = SampleType (fracPos + (ratio * float (i)));
                auto samplePosition = (uint32_t) (intPos + i);

                if (samplePosition < source.numFrames)
                    result += static_cast<SampleType> (data[samplePosition] * windowedSinc (sincPosition, floatZeroCrossings));
            }

            return result;
        }

        static SampleType windowedSinc (SampleType f, SampleType numZeroCrossings) noexcept
        {
            if (f == SampleType())
                return static_cast<SampleType> (1);

            if (f > numZeroCrossings || f < -numZeroCrossings)
                return {};

            f *= localPi;
            const auto window = SampleType (0.5) + SampleType (0.5) * std::cos (f / numZeroCrossings);
            return window * std::sin (f) / f;
        }
    };

    if (dest.numFrames == source.numFrames)
        return copyChannelSet (dest, source);

    for (uint32_t channel = 0; channel < source.numChannels; ++channel)
        Resampler::resample (dest.getChannelSet (channel, 1), source.getChannelSet (channel, 1), zeroCrossings);
}

}
