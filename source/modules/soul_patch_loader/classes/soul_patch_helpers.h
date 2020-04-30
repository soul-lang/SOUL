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

namespace soul::patch
{

//==============================================================================
bool operator== (PatchPlayerConfiguration s1, PatchPlayerConfiguration s2)    { return s1.sampleRate == s2.sampleRate && s1.maxFramesPerBlock == s2.maxFramesPerBlock; }
bool operator!= (PatchPlayerConfiguration s1, PatchPlayerConfiguration s2)    { return ! (s1 == s2); }

static bool isValidPathString (const char* s)
{
    constexpr int maxLength = 8192;

    if (s != nullptr)
        for (int i = 0; i < maxLength; ++i)
            if (s[i] == 0)
                return juce::CharPointer_UTF8::isValidString (s, i);

    return false;
}

//==============================================================================
struct PatchLoadError
{
    std::string message;
};

[[noreturn]] static void throwPatchLoadError (std::string message)
{
    throw PatchLoadError { std::move (message) };
}

[[noreturn]] static void throwPatchLoadError (const juce::String& file, const juce::String& message)
{
    throwPatchLoadError ((file + ": error: " + message).toStdString());
}

//==============================================================================
inline uint32_t getFrameIndex (MIDIMessage m)        { return (uint32_t) m.frameIndex; }
inline uint32_t getPackedMIDIEvent (MIDIMessage m)   { return (((uint32_t) m.byte0) << 16) | (((uint32_t) m.byte1) << 8) | (uint32_t) m.byte2; }
inline void createMIDIMessage (MIDIMessage& m, uint32_t frameIndex, uint32_t packedData)    { m = { frameIndex, m.byte0 = (uint8_t) (packedData >> 16), (uint8_t) (packedData >> 8), (uint8_t) packedData }; }

//==============================================================================
/** Converts a JSON value from a manifest file into a Value. */
inline Value convertJSONToValue (const juce::var& json,
                                 const Type& targetType,
                                 const std::function<Value(const Type&, const juce::String&)>& convertStringToValue,
                                 const ConvertFixedToUnsizedArray& convertToUnsizedArray)

{
    struct JSONtoValue
    {
        const std::function<Value(const Type&, const juce::String&)>& convertStringToValue;
        const ConvertFixedToUnsizedArray& convertFixedToUnsizedArray;

        Value createValue (const Type& targetType, const juce::var& value)
        {
            if (value.isString() && convertStringToValue != nullptr)
                return convertStringToValue (targetType, value.toString());

            if (value.isInt())        return castValue (targetType, Value::createInt32 (static_cast<int> (value)));
            if (value.isInt64())      return castValue (targetType, Value::createInt64 (static_cast<juce::int64> (value)));
            if (value.isDouble())     return castValue (targetType, Value (static_cast<double> (value)));
            if (value.isBool())       return castValue (targetType, Value (static_cast<bool> (value)));

            if (targetType.isArrayOrVector())
                if (auto a = value.getArray())
                    return createArrayOrVector (targetType, *a);

            if (targetType.isStruct())
                if (auto o = value.getDynamicObject())
                    return createObjectValue (targetType.getStructRef(), o->getProperties());

            throwPatchLoadError ("Cannot parse JSON value " + quoteName (value.toString().toStdString()));
            return {};
        }

        Value castValue (const Type& targetType, Value value)
        {
            if (! targetType.hasIdenticalLayout (value.getType()))
                return value.castToTypeWithError (targetType, CodeLocation());

            return value;
        }

        Value createArrayOrVector (const Type& arrayType, const juce::Array<juce::var>& elements)
        {
            auto numElementsProvided = (size_t) elements.size();
            auto numElementsExpected = (size_t) arrayType.getArraySize();

            if (numElementsProvided != numElementsExpected && ! arrayType.isUnsizedArray())
                throwPatchLoadError ("Wrong number of elements for array: expected " + std::to_string (numElementsExpected)
                                       + ", but found " + std::to_string (numElementsProvided));

            ArrayWithPreallocation<Value, 16> elementValues;
            elementValues.reserve (numElementsProvided);
            auto elementType = arrayType.getElementType();

            for (auto& e : elements)
                elementValues.push_back (createValue (elementType, e));

            if (arrayType.isUnsizedArray())
                return convertFixedToUnsizedArray (Value::createArrayOrVector (elementType.createArray ((Type::ArraySize) numElementsProvided), elementValues));

            return Value::createArrayOrVector (arrayType, elementValues);
        }

        Value createObjectValue (Structure& structure, const juce::NamedValueSet& values)
        {
            for (auto& v : values)
                if (! structure.hasMemberWithName (v.name.toString().toStdString()))
                    throwPatchLoadError ("The structure " + quoteName (structure.name)
                                           + " does not contain a member called " + quoteName (v.name.toString().toStdString()));

            ArrayWithPreallocation<Value, 16> members;

            for (auto& m : structure.members)
            {
                if (auto value = values.getVarPointer (juce::Identifier (m.name.c_str())))
                    members.push_back (createValue (m.type, *value));
                else
                    members.push_back (Value::zeroInitialiser (m.type));
            }

            return Value::createStruct (structure, members);
        }
    };

    return JSONtoValue { convertStringToValue, convertToUnsizedArray }
            .createValue (targetType, json);
}

//==============================================================================
/** Attempts to read some sort of audio file and convert it into a suitable Value
    contains the content.

    This will also look at the annotation to work out the required sample rate etc
    and will attempt to wrangle the data into the format needed
*/
struct AudioFileToValue
{
    static Value load (VirtualFile::Ptr file, const Type& type, const Annotation& annotation,
                       const ConvertFixedToUnsizedArray& convertFixedToUnsizedArray)
    {
        SOUL_ASSERT (file != nullptr);
        std::string fileName (file->getAbsolutePath()->getCharPointer());

        if (auto reader = createAudioFileReader (file))
            return loadAudioFileAsValue (*reader, fileName, type, annotation, convertFixedToUnsizedArray);

        throwPatchLoadError ("Failed to read file " + quoteName (fileName));
        return {};
    }

private:
    static constexpr unsigned int maxNumChannels = 8;
    static constexpr uint64_t maxNumFrames = 48000 * 60;

    static Value loadAudioFileAsValue (juce::AudioFormatReader& reader, const std::string& fileName,
                                       const Type& type, const Annotation& annotation,
                                       const ConvertFixedToUnsizedArray& convertFixedToUnsizedArray)
    {
        if (reader.sampleRate > 0)
        {
            if (reader.numChannels > maxNumChannels)
                throwPatchLoadError ("Too many channels in audio file: " + quoteName (fileName));

            if (reader.lengthInSamples > (juce::int64) maxNumFrames)
                throwPatchLoadError ("Audio file was too long to load into memory: " + quoteName (fileName));

            auto numSourceChannels = (uint32_t) reader.numChannels;
            auto numFrames         = (uint32_t) reader.lengthInSamples;

            if (numFrames == 0)
                return {};

            AllocatedChannelSet<DiscreteChannelSet<float>> buffer (numSourceChannels, numFrames);
            reader.read (buffer.channelSet.channels, (int) numSourceChannels, 0, (int) numFrames);

            resampleAudioDataIfNeeded (buffer, reader.sampleRate, annotation.getValue ("resample"));
            extractChannelIfNeeded (buffer, annotation.getValue ("sourceChannel"));

            auto result = convertAudioDataToType (type, convertFixedToUnsizedArray, buffer, reader.sampleRate);

            if (! result.isValid())
                throwPatchLoadError ("Could not convert audio file to type " + quoteName (type.getDescription()));

            return result;
        }

        return {};
    }

    static void resampleAudioDataIfNeeded (AllocatedChannelSet<DiscreteChannelSet<float>>& buffer,
                                           double currentRate, const Value& resampleRate)
    {
        if (resampleRate.isValid())
        {
            double newRate = 0;

            if (resampleRate.getType().isPrimitiveFloat() || resampleRate.getType().isPrimitiveInteger())
                newRate = resampleRate.getAsDouble();

            static constexpr double maxResamplingRatio = 32.0;

            if (newRate > currentRate / maxResamplingRatio && newRate < currentRate * maxResamplingRatio)
            {
                auto ratio = newRate / currentRate;
                SOUL_ASSERT (ratio >= 1.0 / maxResamplingRatio && ratio <= maxResamplingRatio);

                auto newNumFrames = (uint64_t) (buffer.getNumFrames() * ratio + 0.5);

                if (newNumFrames == buffer.getNumFrames())
                    return;

                if (newNumFrames > 0 && newNumFrames < maxNumFrames)
                {
                    AllocatedChannelSet<DiscreteChannelSet<float>> newBuffer (buffer.getNumChannels(), (uint32_t) newNumFrames);
                    resampleToFit (newBuffer.channelSet, buffer.channelSet);
                    std::swap (newBuffer.channelSet, buffer.channelSet);
                    return;
                }
            }

            throwPatchLoadError ("The value of the 'resample' annotation was out of range");
        }
    }

    static void extractChannelIfNeeded (AllocatedChannelSet<DiscreteChannelSet<float>>& buffer,
                                        const Value& channelToExtract)
    {
        if (channelToExtract.isValid())
        {
            if (channelToExtract.getType().isPrimitiveInteger())
            {
                auto sourceChannel = channelToExtract.getAsInt64();

                if (sourceChannel >= 0 && sourceChannel < buffer.getNumFrames())
                {
                    AllocatedChannelSet<DiscreteChannelSet<float>> newBuffer (1, buffer.getNumFrames());
                    copyChannelSet (newBuffer.channelSet, buffer.getChannelSet ((uint32_t) sourceChannel, 1));
                    std::swap (newBuffer.channelSet, buffer.channelSet);
                    return;
                }
            }

            throwPatchLoadError ("The value of the 'sourceChannel' annotation was out of range");
        }
    }

    static std::unique_ptr<juce::AudioFormatReader> createAudioFileReader (VirtualFile::Ptr file)
    {
        SOUL_ASSERT (file != nullptr);

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        if (auto* reader = formats.createReaderFor (std::make_unique<VirtualFileInputStream> (file)))
            return std::unique_ptr<juce::AudioFormatReader> (reader);

        return {};
    }
};

//==============================================================================
/** Wraps a CompilerCache object and presents it as via the LinkerCache interface */
struct CacheConverter  : public LinkerCache
{
    CacheConverter (CompilerCache& c) : cache (c) {}

    static std::unique_ptr<CacheConverter> create (CompilerCache* source)
    {
        if (source != nullptr)
            return std::make_unique<CacheConverter> (*source);

        return {};
    }

    void storeItem (const char* key, const void* sourceData, uint64_t size) override
    {
        cache.storeItemInCache (key, sourceData, size);
    }

    uint64_t readItem (const char* key, void* destAddress, uint64_t destSize) override
    {
        return cache.readItemFromCache (key, destAddress, destSize);
    }

    CompilerCache& cache;
};

}
