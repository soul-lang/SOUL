/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#pragma once

#ifndef JUCE_CORE_H_INCLUDED
 #error "this header is designed to be included in JUCE projects that contain the juce_core module"
#endif

#include "../API/soul_patch.h"

/*
    This file contains some helper classes and functions that are handy for JUCE-based apps
    working with the patch API.
*/

namespace soul
{
namespace patch
{

//==============================================================================
template <typename BaseClass>
struct RefCountHelper  : public BaseClass
{
    void addRef() noexcept override   { ++refCount; }
    void release() noexcept override  { if (--refCount == 0) delete this; }
    std::atomic<int> refCount { 0 };
};

//==============================================================================
/** A simple soul::patch::String implementation.
    To create one, use the various makeString() functions.
*/
struct StringImpl : public RefCountHelper<String>
{
    StringImpl (std::string t) : text (std::move (t)), rawPointer (text.c_str()) {}
    const char* getCharPointer() const override    { return rawPointer; }

    std::string text;
    const char* const rawPointer;
};

inline String::Ptr makeString (std::string s)            { return String::Ptr (new StringImpl (std::move (s))); }
inline String::Ptr makeString (const juce::String& s)    { return makeString (s.toStdString()); }
inline String::Ptr makeString (const juce::var& s)       { return makeString (s.toString()); }

//==============================================================================
/** Creates a Span from a std::vector */
template <typename Type>
Span<Type> makeSpan (std::vector<Type>& v)
{
    if (v.empty())
        return { nullptr, nullptr };

    auto rawPointer = &v.front();
    return { rawPointer, rawPointer + v.size() };
}

//==============================================================================
inline juce::MemoryBlock loadVirtualFileAsMemoryBlock (VirtualFile& f, juce::String& error)
{
    juce::MemoryBlock m;
    auto size = f.getSize();

    if (size > 0)
    {
        m.setSize ((size_t) size);
        auto numRead = f.read (0, m.getData(), (uint64_t) size);

        if (numRead == size)
            return m;

        m.reset();
        error = "Failed to read from file: " + f.getAbsolutePath().toString<juce::String>();
    }
    else
    {
        juce::MemoryOutputStream stream (m, false);
        uint64_t readPos = 0;

        for (;;)
        {
            constexpr int blockSize = 16384;
            juce::HeapBlock<char> buffer (blockSize);

            auto numRead = f.read (readPos, buffer, (uint64_t) blockSize);

            if (numRead > 0)
            {
                readPos += (uint64_t) numRead;
                stream.write (buffer, (size_t) numRead);
            }

            if (numRead != blockSize)
            {
                if (numRead < 0)
                    error = "Failed to read from file: " + f.getAbsolutePath().toString<juce::String>();

                break;
            }
        }
    }

    return m;
}

//==============================================================================
inline juce::String loadVirtualFileAsString (VirtualFile& f, juce::String& error)
{
    auto data = loadVirtualFileAsMemoryBlock (f, error);
    return juce::String::createStringFromData (data.getData(), (int) data.getSize());
}

//==============================================================================
struct VirtualFileInputStream   : public juce::InputStream
{
    VirtualFileInputStream (VirtualFile::Ptr fileToRead)
       : file (std::move (fileToRead)), totalLength (file->getSize())
    {
    }

    juce::int64 getTotalLength() override                 { return totalLength; }
    juce::int64 getPosition() override                    { return position; }
    bool isExhausted() override                           { return position >= totalLength; }

    bool setPosition (juce::int64 newPos) override
    {
        if (newPos < 0 || (totalLength >= 0 && newPos > totalLength))
            return false;

        position = newPos;
        return true;
    }

    int read (void* destBuffer, int maxBytesToRead) override
    {
        jassert (destBuffer != nullptr && maxBytesToRead >= 0);

        auto numToRead = (juce::int64) maxBytesToRead;

        if (totalLength >= 0)
            numToRead = std::min (numToRead, totalLength - position);

        if (numToRead <= 0)
            return 0;

        auto numRead = file->read ((uint64_t) position, destBuffer, (uint64_t) numToRead);

        if (numRead > 0)
            position += numRead;

        return (int) numRead;
    }

    VirtualFile::Ptr file;
    juce::int64 position = 0, totalLength = 0;
};

//==============================================================================
inline const char* getManifestSuffix()                  { return ".soulpatch"; }
inline const char* getManifestWildcard()                { return "*.soulpatch"; }
inline const char* getManifestTopLevelPropertyName()    { return "soulPatchV1"; }

inline juce::var getManifestContentObject (const juce::var& topLevelObject)
{
    return topLevelObject[getManifestTopLevelPropertyName()];
}

/** Attempts to parse the JSON object from a manifest file, returning an error if
    something fails.
*/
inline juce::Result parseManifestFile (VirtualFile& manifestFile, juce::var& resultJSON)
{
    juce::String readError;
    auto content = loadVirtualFileAsString (manifestFile, readError);

    if (readError.isNotEmpty())
        return juce::Result::fail (readError);

    juce::var topLevelObject;
    auto result = juce::JSON::parse (content, topLevelObject);

    if (result.failed())
        return result;

    resultJSON = getManifestContentObject (topLevelObject);

    if (! resultJSON.isObject())
        return juce::Result::fail ("Expected an object called '" + juce::String (getManifestTopLevelPropertyName()) + "'");

    return juce::Result::ok();
}

/** When parsing relative paths from entries in the manifest JSON, this provides a
    handy way to convert those paths into VirtualFile objects.
*/
inline VirtualFile::Ptr getFileRelativeToManifest (VirtualFile& manifest, const juce::String& relativePath)
{
    if (auto parent = manifest.getParent())
        return parent->getChildFile (relativePath.toRawUTF8());

    return {};
}

/** Parses a manifest file and returns a list of the "view" files that it contains. */
inline std::vector<VirtualFile::Ptr> findViewFiles (VirtualFile& manifestFile)
{
    std::vector<VirtualFile::Ptr> views;
    juce::var manifestContent;
    auto result = parseManifestFile (manifestFile, manifestContent);

    if (result.wasOk())
    {
        auto viewList = manifestContent["view"];

        if (viewList.isArray())
        {
            for (auto& s : *viewList.getArray())
                if (auto f = getFileRelativeToManifest (manifestFile, s))
                    views.push_back (f);
        }
        else
        {
            if (auto f = getFileRelativeToManifest (manifestFile, viewList))
                views.push_back (f);
        }
    }

    return views;
}


} // namespace patch
} // namespace soul
