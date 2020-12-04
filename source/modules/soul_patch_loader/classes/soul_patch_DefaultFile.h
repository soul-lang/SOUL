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
/** A simple URL-based implementation of VirtualFile */
struct RemoteFile final  : public RefCountHelper<VirtualFile, RemoteFile>
{
    RemoteFile (juce::URL u) : url (std::move (u)) {}
    ~RemoteFile() = default;

    String* getName() override                     { return makeString (url.getFileName()); }
    String* getAbsolutePath() override             { return makeString (url.toString (true)); }
    VirtualFile* getParent() override              { return new RemoteFile (url.getParentURL()); }
    int64_t getSize() override                     { return -1; }
    int64_t getLastModificationTime() override     { return -1; }

    VirtualFile* getChildFile (const char* path) override
    {
        if (sanityCheckString (path))
            return new RemoteFile (url.getChildURL (juce::CharPointer_UTF8 (path)));

        return {};
    }

    int64_t read (uint64_t start, void* targetBuffer, uint64_t size) override
    {
        if (targetBuffer == nullptr)
            return -1;

        if (size == 0)
            return 0;

        if (! hasLoadedContent)
        {
            int statusCode = 0;

            if (auto stream = std::unique_ptr<juce::InputStream> (url.createInputStream (false, nullptr, nullptr, {},
                                                                                         10000, // timeout in millisecs
                                                                                         nullptr, &statusCode)))
            {
                if (statusCode != 200)
                    return -1;

                auto bytesRead = cachedContent.writeFromInputStream (*stream, -1);

                if (bytesRead > 0)
                    hasLoadedContent = true;
            }

            if (! hasLoadedContent)
                return -1;
        }

        if (start >= cachedContent.getDataSize())
            return 0;

        auto numToRead = std::min (size, (uint64_t) cachedContent.getDataSize() - start);
        memcpy (targetBuffer, juce::addBytesToPointer (cachedContent.getData(), start), numToRead);
        return (int64_t) numToRead;
    }

    juce::URL url;
    juce::MemoryOutputStream cachedContent;
    bool hasLoadedContent = false;
};

//==============================================================================
/** A local-file-based implementation of VirtualFile */
struct LocalFile final  : public RefCountHelper<VirtualFile, LocalFile>
{
    LocalFile (juce::File f) : file (std::move (f)) {}
    LocalFile (const std::string& path) : LocalFile (juce::File::getCurrentWorkingDirectory().getChildFile (path)) {}
    ~LocalFile() = default;

    String* getName() override                     { return makeStringPtr (file.getFileName().toStdString()); }
    String* getAbsolutePath() override             { return makeStringPtr (file.getFullPathName().toStdString()); }
    VirtualFile* getParent() override              { return new LocalFile (file.getParentDirectory()); }
    int64_t getSize() override                     { return file.exists() ? file.getSize() : 0; }
    int64_t getLastModificationTime() override     { return file.exists() ? file.getLastModificationTime().toMilliseconds() : -1; }

    VirtualFile* getChildFile (const char* path) override
    {
        if (sanityCheckString (path))
            return new LocalFile (file.getChildFile (juce::CharPointer_UTF8 (path)));

        return {};
    }

    int64_t read (uint64_t start, void* targetBuffer, uint64_t size) override
    {
        if (targetBuffer == nullptr)
            return -1;

        if (size == 0)
            return 0;

        juce::FileInputStream in (file);

        if (in.openedOk())
        {
            if ((juce::int64) start > 0)
                if (! in.setPosition ((juce::int64) start))
                    return -1;

            int64_t totalRead = 0;

            while (size > 0)
            {
                auto numToRead = (int) std::min (size, static_cast<uint64_t> (0x70000000ul));
                auto numRead = in.read (juce::addBytesToPointer (targetBuffer, totalRead), numToRead);

                if (numRead < 0)   return -1;
                if (numRead == 0)  break;

                size -= (uint64_t) numRead;
                totalRead += numRead;
            }

            return totalRead;
        }

        return -1;
    }

    juce::File file;
};

//==============================================================================
/** Creates either a LocalFile or RemoteFile object, based on the path provided */
VirtualFile* createLocalOrRemoteFile (const char* pathOrURL)
{
    if (! sanityCheckString (pathOrURL))
        return {};

    auto path = juce::String::fromUTF8 (pathOrURL);

    for (auto* protocol : { "http:", "https:", "ftp:", "sftp:", "file:" })
        if (path.startsWithIgnoreCase (protocol))
            return new RemoteFile (juce::URL (path));

    return new LocalFile (path);
}

}
