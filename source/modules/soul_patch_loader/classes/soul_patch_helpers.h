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

[[noreturn]] static void throwPatchLoadError (const std::string& file, const std::string& message)
{
    throwPatchLoadError (file + ": error: " + message);
}

//==============================================================================
/** Attempts to read some sort of audio file and convert it into a suitable Value
    contains the content.

    This will also look at the annotation to work out the required sample rate etc
    and will attempt to wrangle the data into the format needed
*/
struct AudioFileToValue
{
    static choc::value::Value load (VirtualFile::Ptr file, const choc::value::ValueView& annotation)
    {
        SOUL_ASSERT (file != nullptr);
        std::string fileName (file->getAbsolutePath()->getCharPointer());

        if (auto reader = createAudioFileReader (file))
            return loadAudioFileAsValue (*reader, fileName, annotation);

        throwPatchLoadError ("Failed to read file " + quoteName (fileName));
        return {};
    }

private:
    static constexpr unsigned int maxNumChannels = 8;
    static constexpr uint64_t maxNumFrames = 48000 * 60;

    static choc::value::Value loadAudioFileAsValue (juce::AudioFormatReader& reader, const std::string& fileName, const choc::value::ValueView& annotation)
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

            choc::buffer::ChannelArrayBuffer<float> buffer (numSourceChannels, numFrames);
            reader.read (buffer.getView().data.channels, (int) numSourceChannels, 0, (int) numFrames);

            resampleAudioDataIfNeeded (buffer, reader.sampleRate, annotation["resample"]);
            extractChannelIfNeeded (buffer, annotation["sourceChannel"]);

            auto result = convertAudioDataToObject (buffer, reader.sampleRate);

            if (result.isVoid())
                throwPatchLoadError ("Could not load audio file");

            return result;
        }

        return {};
    }

    static void resampleAudioDataIfNeeded (choc::buffer::ChannelArrayBuffer<float>& buffer,
                                           double currentRate, const choc::value::ValueView& resampleRate)
    {
        if (! resampleRate.isVoid())
        {
            double newRate = resampleRate.getWithDefault<double> (0);
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
                    choc::buffer::ChannelArrayBuffer<float> newBuffer (buffer.getNumChannels(), (uint32_t) newNumFrames);
                    resampleToFit (newBuffer, buffer);
                    buffer = std::move (newBuffer);
                    return;
                }
            }

            throwPatchLoadError ("The value of the 'resample' annotation was out of range");
        }
    }

    static void extractChannelIfNeeded (choc::buffer::ChannelArrayBuffer<float>& buffer,
                                        const choc::value::ValueView& channelToExtract)
    {
        if (! channelToExtract.isVoid())
        {
            auto sourceChannel = channelToExtract.getWithDefault<int64_t> (-1);

            if (sourceChannel >= 0 && sourceChannel < buffer.getNumFrames())
            {
                choc::buffer::ChannelArrayBuffer<float> newBuffer (1, buffer.getNumFrames());
                copy (newBuffer, buffer.getChannelRange ({ (uint32_t) sourceChannel, (uint32_t) (sourceChannel + 1) }));
                buffer = std::move (newBuffer);
                return;
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
