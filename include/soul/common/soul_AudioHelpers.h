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

// This file contains a few handy JUCE-based utility classes and functions for
// audio file and device tasks

#pragma once

namespace soul::utilities
{

//==============================================================================
#ifdef JUCE_AUDIO_FORMATS_H_INCLUDED

struct JUCEAudioFileFactory  : public soul::AudioFileFactory
{
    JUCEAudioFileFactory() = default;

    std::unique_ptr<soul::AudioFileReader> createFileReader (std::unique_ptr<soul::AudioFileFactory::DataSource> dataSource) override
    {
        if (dataSource == nullptr)
            return {};

        struct InputStreamWrapper  : public juce::InputStream
        {
            InputStreamWrapper (std::unique_ptr<soul::AudioFileFactory::DataSource>&& s)
                : source (std::move (s)), size ((juce::int64) source->getTotalSize()) {}

            juce::int64 getTotalLength() override                       { return size; }
            juce::int64 getPosition() override                          { return position; }
            bool setPosition (juce::int64 newPosition) override         { if (newPosition < 0) return false; position = newPosition; return true;}
            bool isExhausted() override                                 { return position >= size; }

            int read (void* dest, int numBytes) override
            {
                if (numBytes > 0)
                {
                    if (auto numRead = source->read ((uint64_t) position, dest, (size_t) numBytes))
                    {
                        position += numRead;
                        return (int) numRead;
                    }
                }

                return numBytes == 0 ? 0 : -1;
            }

            std::unique_ptr<soul::AudioFileFactory::DataSource> source;
            const juce::int64 size;
            juce::int64 position = 0;
        };

        struct Reader  : public soul::AudioFileReader
        {
            Reader (juce::AudioFormatReader* r) : reader (r), offsetHelper (reader->numChannels) {}

            soul::AudioFileProperties getProperties() override
            {
                soul::AudioFileProperties p;
                p.sampleRate   = reader->sampleRate;
                p.numFrames    = (uint64_t) reader->lengthInSamples;
                p.numChannels  = (uint32_t) reader->numChannels;
                p.bitDepth     = (uint32_t) reader->bitsPerSample;
                return p;
            }

            bool read (int64_t startFrame, choc::buffer::ChannelArrayView<float> dest) override
            {
                return reader->read (offsetHelper.getArray (dest.data.channels, dest.getNumChannels(), dest.data.offset),
                                     (int) dest.getNumChannels(), (juce::int64) startFrame, (int) dest.getNumFrames());
            }

            std::unique_ptr<juce::AudioFormatReader> reader;
            PointerArrayOffsetHelper<float> offsetHelper;
        };

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        auto readWrapper = std::make_unique<InputStreamWrapper> (std::move (dataSource));

        if (auto juceReader = formats.createReaderFor (std::make_unique<juce::BufferedInputStream> (readWrapper.release(), 32768, true)))
            return std::make_unique<Reader> (juceReader);

        return {};
    }

    std::unique_ptr<soul::AudioFileWriter> createFileWriter (soul::AudioFileProperties properties,
                                                             std::unique_ptr<soul::AudioFileFactory::DataSink> dataSink) override
    {
        if (dataSink == nullptr)
            return {};

        if (choc::text::toLowerCase (properties.fileType) != "wav" && ! properties.fileType.empty())
            soul::throwError (soul::Errors::unsupportedAudioFileType (properties.fileType));

        struct OutputStreamWrapper  : public juce::OutputStream
        {
            OutputStreamWrapper (std::unique_ptr<soul::AudioFileFactory::DataSink>&& s) : sink (std::move (s)) {}
            ~OutputStreamWrapper() override     { sink->close(); }

            void flush() override {}

            bool setPosition (juce::int64 pos) override              { return sink->seek ((uint64_t) pos); }
            juce::int64 getPosition() override                       { return (juce::int64) sink->getPosition(); }
            bool write (const void* data, size_t numBytes) override  { return sink->write (data, numBytes); }

            std::unique_ptr<soul::AudioFileFactory::DataSink> sink;
        };

        struct Writer  : public soul::AudioFileWriter
        {
            Writer (juce::AudioFormatWriter* w) : writer (w), offsetHelper ((uint32_t) writer->getNumChannels()) {}

            bool append (choc::buffer::ChannelArrayView<float> source) override
            {
                return writer->writeFromFloatArrays (offsetHelper.getArray (source.data.channels, source.getNumChannels(), source.data.offset),
                                                     (int) source.getNumChannels(), (int) source.getNumFrames());
            }

            bool close() override
            {
                if (writer == nullptr || writer->flush())
                {
                    writer.reset();
                    return true;
                }

                return false;
            }

            std::unique_ptr<juce::AudioFormatWriter> writer;
            PointerArrayOffsetHelper<const float> offsetHelper;
        };

        juce::WavAudioFormat format;

        if (properties.bitDepth == 0)
            properties.bitDepth = format.getPossibleBitDepths().contains(32) ? 32 : 16;

        if (! format.getPossibleBitDepths().contains ((int) properties.bitDepth))
            soul::throwError (soul::Errors::unsupportedBitDepth());

        auto outputStream = std::make_unique<OutputStreamWrapper> (std::move (dataSink));

        if (auto juceWriter = format.createWriterFor (outputStream.get(), properties.sampleRate,
                                                      properties.numChannels, (int) properties.bitDepth, {}, 0))
        {
            outputStream.release();
            return std::make_unique<Writer> (juceWriter);
        }

        return {};
    }

private:
    template <typename SampleType>
    struct PointerArrayOffsetHelper
    {
        PointerArrayOffsetHelper (uint32_t numChannels) { scratch.resize (numChannels); }

        SampleType* const* getArray (SampleType* const* source, uint32_t numChans, uint32_t offset)
        {
            if (offset == 0)
                return source;

            SOUL_ASSERT (numChans <= scratch.size());

            for (uint32_t i = 0; i < numChans; ++i)
                scratch[i] = source[i] + offset;

            return scratch.data();
        }

        std::vector<SampleType*> scratch;
    };
};

#endif

//==============================================================================
#ifdef JUCE_AUDIO_DEVICES_H_INCLUDED

inline std::string getAudioDeviceDescription (juce::AudioIODevice& audioDevice)
{
    auto rate = audioDevice.getCurrentSampleRate();
    auto latencyMs = (int) ((audioDevice.getOutputLatencyInSamples()
                             + audioDevice.getInputLatencyInSamples())
                            * 1000.0 / rate);

    std::ostringstream out;

    out << "Opened " << audioDevice.getTypeName() << " device " << audioDevice.getName().quoted() << "\n"
        << "Sample rate: " << rate << "Hz,  block size: " << audioDevice.getCurrentBufferSizeSamples()
        << ",  latency: " << latencyMs << "ms";

    auto getListOfActiveBits = [] (const juce::BigInteger& b)
    {
        std::vector<std::string> bits;

        for (int i = 0; i <= b.getHighestBit(); ++i)
            if (b[i])
                bits.push_back (std::to_string (i));

        return choc::text::joinStrings (bits, ", ");
    };

    auto inChans  = getListOfActiveBits (audioDevice.getActiveInputChannels());
    auto outChans = getListOfActiveBits (audioDevice.getActiveOutputChannels());

    if (! inChans.empty())   out << ",  input chans: [" << inChans << "]";
    if (! outChans.empty())  out << ",  output chans: [" << outChans << "]";

    return out.str();
}

#endif

} // namespace soul::utilities
