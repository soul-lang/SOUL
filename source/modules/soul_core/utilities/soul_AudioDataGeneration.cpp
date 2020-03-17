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

namespace WaveGenerators
{
    //==============================================================================
    struct Generator
    {
        Generator() = default;
        virtual ~Generator() = default;

        void init (double frequency, double sampleRate)
        {
            phaseIncrement = frequency / sampleRate;
        }

        virtual double getSample() = 0;

        void advance()
        {
            currentPhase += phaseIncrement;

            while (currentPhase >= 1.0)
                currentPhase -= 1.0;
        }

        double phaseIncrement = 0;
        double currentPhase   = 0;
    };


    //==============================================================================
    struct Sine  : public Generator
    {
        double getSample() override
        {
            return std::sin (currentPhase * twoPi);
        }
    };

    //==============================================================================
    struct Blep  : public Generator
    {
        double blep (double phase)
        {
            if (phase < phaseIncrement)
            {
                phase = phase / phaseIncrement;
                return (phase + phase) - (phase * phase) - 1.0f;
            }

            if (phase > (1.0f - phaseIncrement))
            {
                phase = (phase - 1.0f) / phaseIncrement;
                return (phase * phase) + (phase + phase) + 1.0f;
            }

            return 0;
        }
    };

    //==============================================================================
    struct Saw  : public Blep
    {
        double getSample() override
        {
            return -1.0 + (2.0 * currentPhase) - blep (currentPhase);
        }
    };

    //==============================================================================
    struct Square  : public Blep
    {
        double getSample() override
        {
            return (currentPhase < 0.5 ? -1.0f : 1.0f) - blep (currentPhase)
                    + blep (std::fmod (currentPhase + 0.5, 1.0));
        }
    };

    //==============================================================================
    struct Triangle  : public Square
    {
        double getSample() override
        {
            sum += 4.0 * phaseIncrement * Square::getSample();
            return sum;
        }

        double sum = 1;
    };
}

Value convertAudioDataToType (const Type& requestedType, ConstantTable& constantTable,
                              DiscreteChannelSet<float> data, double sampleRate)
{
    if (requestedType.isUnsizedArray())
    {
        auto elementType = requestedType.getElementType();

        if (elementType.isPrimitiveOrVector() && elementType.isFloat32())
        {
            auto content = Value::zeroInitialiser (elementType.createArray (data.numFrames));
            copyChannelSetToFit (content.getAsChannelSet32(), data);
            return Value::createUnsizedArray (elementType, constantTable.getHandleForValue (content));
        }
    }

    if (requestedType.isStruct())
    {
        auto isSampleRateField = [] (const Structure::Member& m)
        {
            return m.type.isPrimitive()
                    && (m.name == "sampleRate" || m.name == "rate" || m.name == "frequency")
                    && (m.type.isFloatingPoint() || m.type.isInteger());
        };

        auto& s = requestedType.getStructRef();
        std::vector<Value> memberValues;
        memberValues.reserve (s.members.size());

        for (auto& m : s.members)
        {
            if (isSampleRateField (m))
            {
                memberValues.push_back (Value (sampleRate).castToTypeExpectingSuccess (m.type));
            }
            else if (m.type.isUnsizedArray())
            {
                auto dataValue = convertAudioDataToType (m.type, constantTable, data, sampleRate);
                memberValues.push_back (std::move (dataValue));
            }
            else
            {
                memberValues.push_back (Value::zeroInitialiser (m.type));
            }
        }

        return Value::createStruct (s, memberValues);
    }

    return Value::createFloatVectorArray (data);
}

//==============================================================================
Value generateWaveform (const Type& requiredType, ConstantTable& constantTable,
                        double frequency, double sampleRate, int64_t numFrames,
                        WaveGenerators::Generator& generator,
                        uint32_t oversamplingFactor)
{
    if (numFrames > 0 && frequency > 0 && sampleRate > 0 && numFrames < 48000 * 60 * 60 * 2)
    {
        AllocatedChannelSet<DiscreteChannelSet<float>> data (1, (uint32_t) (numFrames * oversamplingFactor));
        auto* samples = data.channelSet.getChannel (0);

        generator.init (frequency, sampleRate * oversamplingFactor);

        for (uint32_t i = 0; i < data.channelSet.numFrames; ++i)
        {
            samples[i] = (float) generator.getSample();
            generator.advance();
        }

        if (oversamplingFactor == 1)
            return convertAudioDataToType (requiredType, constantTable, data.channelSet, sampleRate);

        // Resample to the right size
        AllocatedChannelSet<DiscreteChannelSet<float>> resampledData (1, (uint32_t) (numFrames));
        resampleToFit (resampledData.channelSet, data.channelSet);
        return convertAudioDataToType (requiredType, constantTable, resampledData.channelSet, sampleRate);
    }

    return {};
}

template <class Generator>
static Value generateWaveform (const Type& requiredType, ConstantTable& constantTable,
                               const Annotation& annotation, uint32_t oversamplingFactor)
{
    Generator g;

    return generateWaveform (requiredType, constantTable,
                             annotation.getDouble ("frequency"),
                             annotation.getDouble ("rate"),
                             annotation.getInt64 ("numFrames"),
                             g,
                             oversamplingFactor);
}

Value generateWaveform (const Type& requiredType, ConstantTable& constantTable, const Annotation& annotation)
{
    if (annotation.getBool ("sinewave") || annotation.getBool ("sine"))
        return generateWaveform<WaveGenerators::Sine> (requiredType, constantTable, annotation, 1);

    if (annotation.getBool ("sawtooth") || annotation.getBool ("saw"))
        return generateWaveform<WaveGenerators::Saw> (requiredType, constantTable, annotation, 2);

    if (annotation.getBool ("triangle"))
        return generateWaveform<WaveGenerators::Triangle> (requiredType, constantTable, annotation, 2);

    if (annotation.getBool ("squarewave") || annotation.getBool ("square"))
        return generateWaveform<WaveGenerators::Square> (requiredType, constantTable, annotation, 2);

    return {};
}

}
