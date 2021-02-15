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
/// Some basic properties used for reading or writing audio files
struct AudioFileProperties
{
    double sampleRate = 0;
    uint64_t numFrames = 0;
    uint32_t numChannels = 0;
    uint32_t bitDepth = 0;

    /// if empty then this just means "default", which is most likely "wav"
    std::string fileType;
};

//==============================================================================
/// A base class for objects that can read from an audio file format.
class AudioFileReader
{
public:
    virtual ~AudioFileReader() = default;

    virtual AudioFileProperties getProperties() = 0;
    virtual bool read (int64_t startFrameInFile, choc::buffer::ChannelArrayView<float> destination) = 0;
};

/// A base class for objects that can write to an audio file format.
class AudioFileWriter
{
public:
    virtual ~AudioFileWriter() = default;

    virtual bool append (choc::buffer::ChannelArrayView<float>) = 0;
    virtual bool close() = 0;
};

//==============================================================================
/// A base for implementations that can create audio file readers and writers
class AudioFileFactory
{
public:
    virtual ~AudioFileFactory() = default;

    /// A simplified byte stream for use by implementations of audio codecs
    struct DataSource
    {
        virtual ~DataSource() = default;
        virtual uint64_t getTotalSize() = 0;
        virtual size_t read (uint64_t start, void* destBuffer, size_t numBytes) = 0;
    };

    /// A simplified byte stream for use by implementations of audio codecs
    struct DataSink
    {
        virtual ~DataSink() = default;
        virtual bool seek (uint64_t) = 0;
        virtual uint64_t getPosition() = 0;
        virtual bool write (const void* sourceData, size_t numBytes) = 0;
        virtual bool close() = 0;
    };

    /// Attempts to create a reader that can read from the given data source
    virtual std::unique_ptr<AudioFileReader> createFileReader (std::unique_ptr<DataSource>) = 0;

    /// Attempts to create a writer with the given properties, which will write into the given data sink
    virtual std::unique_ptr<AudioFileWriter> createFileWriter (AudioFileProperties, std::unique_ptr<DataSink>) = 0;

    // Helper functions to create various types of source and sink - these may throw errors if creation fails
    static std::unique_ptr<DataSource> createMemoryDataSource (const void* data, size_t size);
    static std::unique_ptr<DataSource> createFileDataSource (const std::string& fullPath);
    static std::unique_ptr<DataSink>   createFileDataSink (const std::string& fullPath);
};

}

