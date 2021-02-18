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

#include <iostream>

namespace soul
{

//==============================================================================
template <typename ChannelSet>
choc::value::Value createArrayFromChannelSet (ChannelSet source, uint32_t targetNumChans)
{
    if (targetNumChans <= source.getNumChannels())
        return choc::value::createArray (source.getNumFrames(), targetNumChans,
                                         [&] (uint32_t frame, uint32_t chan) { return source.getSample (chan, frame); });

    if (source.getNumChannels() == 1 && targetNumChans == 2)
        return choc::value::createArray (source.getNumFrames(), targetNumChans,
                                         [&] (uint32_t frame, uint32_t) { return source.getSample (0, frame); });

    return choc::value::createArray (source.getNumFrames(), targetNumChans,
                                     [&] (uint32_t frame, uint32_t chan) { return source.getSampleIfInRange (chan, frame); });
}

choc::value::Value convertChannelSetToArray (choc::buffer::ChannelArrayView<const float> source)
{
    return createArrayFromChannelSet (source, source.getNumChannels());
}

choc::value::Value convertChannelSetToArray (choc::buffer::ChannelArrayView<const float> source, uint32_t targetNumChannels)
{
    return createArrayFromChannelSet (source, targetNumChannels);
}

choc::value::ValueView getChannelSetAsArrayView (choc::buffer::InterleavedView<float> source)
{
    return choc::value::create2DArrayView (source.data.data, source.getNumFrames(), source.getNumChannels());
}

choc::value::ValueView getChannelSetAsArrayView (choc::buffer::InterleavedView<const float> source)
{
    return choc::value::create2DArrayView (const_cast<float*> (source.data.data), source.getNumFrames(), source.getNumChannels());
}

choc::buffer::InterleavedView<float> getChannelSetFromArray (const choc::value::ValueView& sourceArray)
{
    auto frameType = sourceArray.getType().getElementType();
    uint32_t numChannels = 1;

    if (frameType.isVector())
    {
        SOUL_ASSERT (frameType.getElementType().isFloat32());
        numChannels = frameType.getNumElements();
    }
    else
    {
        SOUL_ASSERT (frameType.isFloat32());
    }

    return choc::buffer::createInterleavedView (static_cast<float*> (const_cast<void*> (sourceArray.getRawData())),
                                                numChannels, sourceArray.size());
}

choc::value::Value createAudioDataObject (const choc::value::ValueView& frames, double sampleRate)
{
    return choc::value::createObject ("soul::AudioFile",
                                      "frames", frames,
                                      "sampleRate", sampleRate);
}

choc::value::Value convertAudioDataToObject (choc::buffer::InterleavedView<const float> source, double sampleRate)
{
    return createAudioDataObject (getChannelSetAsArrayView (source), sampleRate);
}

choc::value::Value convertAudioDataToObject (choc::buffer::ChannelArrayView<const float> source, double sampleRate)
{
    return createAudioDataObject (convertChannelSetToArray (source), sampleRate);
}

choc::value::Value coerceAudioFileObjectToTargetType (const Type& targetType, const choc::value::ValueView& sourceValue)
{
    if (sourceValue.isObject())
    {
        auto isFrameArray = [] (const choc::value::ValueView& member)
        {
            if (member.isArray())
            {
                auto isAudioSample = [] (const choc::value::ValueView& c) { return c.isInt32() || c.isFloat32() || c.isFloat64(); };

                auto first = member[0];

                return (first.isPrimitive() && isAudioSample (first))
                         || (first.isVector() && isAudioSample (first[0]));
            }

            return false;
        };

        auto isRateName = [] (const std::string& s)
        {
            return s == "rate" || s == "sampleRate" || s == "frequency";
        };

        choc::value::Value sourceFrameArray, sourceRate;

        for (uint32_t i = 0; i < sourceValue.size(); ++i)
        {
            auto member = sourceValue.getObjectMemberAt (i);

            if (isFrameArray (member.value))
                sourceFrameArray = member.value;
            else if (isRateName (member.name))
                sourceRate = member.value;
        }

        SOUL_ASSERT (! (sourceFrameArray.isVoid() || sourceRate.isVoid()));

        if (targetType.isArray())
            return sourceFrameArray;

        if (targetType.isStruct())
        {
            auto o = choc::value::createObject ("soul::AudioSample");

            for (auto& m : targetType.getStructRef().getMembers())
            {
                if (m.type.isArray() && m.type.getArrayElementType().isPrimitiveOrVector())
                    o.addMember (m.name, sourceFrameArray);
                else if ((m.type.isFloatingPoint() || m.type.isPrimitiveInteger()) && isRateName (m.name))
                    o.addMember (m.name, sourceRate);
            }

            return o;
        }
    }

    return choc::value::Value (sourceValue);
}

//==============================================================================
template <typename Oscillator>
static choc::value::Value renderOscillator (double frequency, double sampleRate,
                                            int64_t numFrames, uint32_t oversamplingFactor)
{
    choc::buffer::ChannelArrayBuffer<float> data (1, (uint32_t) (numFrames * oversamplingFactor));

    choc::oscillator::render<Oscillator> (data, frequency, sampleRate * oversamplingFactor);

    if (oversamplingFactor == 1)
        return convertAudioDataToObject (data, sampleRate);

    // Resample to the right size
    choc::buffer::ChannelArrayBuffer<float> resampledData (1, (uint32_t) numFrames);
    resampleToFit (resampledData, data);
    return convertAudioDataToObject (resampledData, sampleRate);
}

choc::value::Value generateWaveform (const Annotation& annotation)
{
    auto freq    = annotation.getDouble ("frequency");
    auto rate    = annotation.getDouble ("rate");
    auto frames  = annotation.getInt64 ("numFrames");

    if (frames > 0 && freq > 0 && rate > 0 && frames < 48000 * 60 * 60 * 2)
    {
        if (annotation.getBool ("sinewave") || annotation.getBool ("sine"))
            return renderOscillator<choc::oscillator::Sine<double>> (freq, rate, frames, 1);

        if (annotation.getBool ("sawtooth") || annotation.getBool ("saw"))
            return renderOscillator<choc::oscillator::Saw<double>> (freq, rate, frames, 2);

        if (annotation.getBool ("triangle"))
            return renderOscillator<choc::oscillator::Triangle<double>> (freq, rate, frames, 2);

        if (annotation.getBool ("squarewave") || annotation.getBool ("square"))
            return renderOscillator<choc::oscillator::Square<double>> (freq, rate, frames, 2);
    }

    return {};
}

}
