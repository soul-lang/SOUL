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

namespace soul
{

std::unique_ptr<AudioFileFactory::DataSource> AudioFileFactory::createMemoryDataSource (const void* dataToUse, size_t sizeToUse)
{
    struct Source  : public AudioFileFactory::DataSource
    {
        Source (const void* d, size_t s) : data (d), size (s)
        {
            SOUL_ASSERT (data != nullptr && size != 0);
        }

        uint64_t getTotalSize() override    { return size; }

        size_t read (uint64_t start, void* dest, size_t numBytes) override
        {
            if (start >= size)
                return 0;

            if (start + numBytes > size)
                numBytes = static_cast<size_t> (size - start);

            memcpy (dest, static_cast<const char*> (data) + start, numBytes);
            return numBytes;
        }

        const void* const data;
        const size_t size;
    };

    return std::make_unique<Source> (dataToUse, sizeToUse);
}

std::unique_ptr<AudioFileFactory::DataSource> AudioFileFactory::createFileDataSource (const std::string& fullPath)
{
    struct Source  : public AudioFileFactory::DataSource
    {
        Source (std::ifstream&& i) : in (std::move (i))
        {
            in.seekg (0, std::ios::end);
            auto len = in.tellg();
            SOUL_ASSERT (len >= 0);
            totalSize = static_cast<uint64_t> (len);
        }

        uint64_t getTotalSize() override    { return totalSize; }

        size_t read (uint64_t start, void* dest, size_t numBytes) override
        {
            try
            {
                if (start >= totalSize)
                    return 0;

                in.seekg (static_cast<std::streamoff> (start));

                if (in.fail())
                    return 0;

                if (start + numBytes > totalSize)
                    numBytes = static_cast<size_t> (totalSize - start);

                in.read (static_cast<char*> (dest), static_cast<std::streamsize> (numBytes));
                return in.fail() ? 0 : numBytes;
            }
            catch (...) {}

            return 0;
        }

        std::ifstream in;
        uint64_t totalSize = 0;
    };

    std::ifstream i (fullPath, std::ios::binary);

    if (! i)
        throwError (Errors::cannotReadFile (fullPath));

    return std::make_unique<Source> (std::move (i));
}

std::unique_ptr<AudioFileFactory::DataSink> AudioFileFactory::createFileDataSink (const std::string& fullPath)
{
    struct Sink  : public AudioFileFactory::DataSink
    {
        Sink (std::fstream&& f) : out (std::move (f)) {}

        bool seek (uint64_t pos) override
        {
            try
            {
                out.seekp (static_cast<std::streamoff> (pos));
                return true;
            }
            catch (...) {}

            return false;
        }

        uint64_t getPosition() override
        {
            return static_cast<uint64_t> (out.tellp());
        }

        bool write (const void* data, size_t size) override
        {
            try
            {
                out.write (static_cast<const char*> (data), static_cast<std::streamsize> (size));
                return ! out.fail();
            }
            catch (...) {}

            return false;
        }

        bool close() override
        {
            out.close();
            return true;
        }

        std::fstream out;
    };

    std::fstream o (fullPath, std::ios::binary | std::ios::trunc | std::ios_base::in | std::ios_base::out);

    if (! o)
        throwError (Errors::cannotWriteFile (fullPath));

    return std::make_unique<Sink> (std::move (o));
}

}
