/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#pragma once

#include "../API/soul_patch.h"
#include "../../../3rdParty/choc/text/choc_UTF8.h"
#include "../../../3rdParty/choc/text/choc_JSON.h"
#include "../../../3rdParty/choc/text/choc_StringUtilities.h"

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

inline String::Ptr makeString (std::string s)                    { return String::Ptr (new StringImpl (std::move (s))); }
inline String::Ptr makeString (const choc::value::ValueView& s)  { return makeString (s.isString() ? std::string (s.getString()) : std::string()); }

#ifdef JUCE_CORE_H_INCLUDED
inline String::Ptr makeString (const juce::String& s)   { return makeString (s.toStdString()); }
inline String::Ptr makeString (const juce::var& s)      { return makeString (s.toString()); }
#endif

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
inline std::string loadVirtualFileAsMemoryBlock (VirtualFile& f, std::string& error)
{
    auto fileSize = f.getSize();
    size_t blockSize = 13;//fileSize > 0 ? static_cast<size_t> (fileSize) : 8192;
    std::string buffer;
    buffer.resize (blockSize);
    std::ostringstream result;
    uint64_t readPos = 0;

    for (;;)
    {
        auto numRead = f.read (readPos, std::addressof (buffer[0]), (uint64_t) blockSize);

        if (numRead < 0)
        {
            error = "Failed to read from file: " + f.getAbsolutePath().toString<std::string>();
            return {};
        }

        if (fileSize == numRead && readPos == 0)
        {
            buffer.resize (static_cast<size_t> (numRead));
            return buffer;
        }

        result.write (buffer.data(), static_cast<std::streamsize> (numRead));

        if (static_cast<size_t> (numRead) < blockSize)
            return result.str();

        readPos += static_cast<size_t> (numRead);
    }
}

//==============================================================================
inline std::string loadVirtualFileAsString (VirtualFile& f, std::string& error)
{
    auto data = loadVirtualFileAsMemoryBlock (f, error);

    if (error.empty())
    {
        auto invalidUTF8 = choc::text::findInvalidUTF8Data (data.data(), data.size());

        if (invalidUTF8 == nullptr)
            return data;

        error = "Invalid UTF8 data";
    }

    return {};
}

/** When parsing relative paths from entries in the manifest JSON, this provides a
    handy way to convert those paths into VirtualFile objects.
*/
inline VirtualFile::Ptr getFileRelativeToManifest (VirtualFile& manifest, std::string_view relativePath)
{
    if (auto parent = manifest.getParent())
        return parent->getChildFile (std::string (relativePath).c_str());

    return {};
}

//==============================================================================
inline const char* getManifestSuffix()                  { return ".soulpatch"; }
inline const char* getManifestWildcard()                { return "*.soulpatch"; }
inline const char* getManifestTopLevelPropertyName()    { return "soulPatchV1"; }

//==============================================================================
inline choc::value::ValueView getManifestContentObject (const choc::value::ValueView& topLevelObject)
{
    return topLevelObject[getManifestTopLevelPropertyName()];
}

/** Attempts to parse the JSON object from a manifest file, returning an error if
    something fails.
*/
inline choc::value::Value parseManifestFile (VirtualFile& manifestFile, std::string& errorMessage)
{
    try
    {
        auto content = loadVirtualFileAsString (manifestFile, errorMessage);

        if (errorMessage.empty())
        {
            auto topLevelObject = choc::json::parse (content);
            auto contentObject = getManifestContentObject (topLevelObject);

            if (contentObject.isObject())
                return choc::value::Value (contentObject);

            errorMessage = "Expected an object called '" + std::string (getManifestTopLevelPropertyName()) + "'";
        }
    }
    catch (choc::json::ParseError error)
    {
        errorMessage = manifestFile.getAbsolutePath().toString<std::string>()
                        + ":" + std::to_string (error.line) + ":" + std::to_string (error.column) + ": " + error.message;
    }

    return {};
}

/** Parses a manifest file and returns a list of the "view" files that it contains. */
inline std::vector<VirtualFile::Ptr> findViewFiles (VirtualFile& manifestFile)
{
    std::vector<VirtualFile::Ptr> views;
    std::string error;
    auto manifestContent = parseManifestFile (manifestFile, error);

    if (error.empty())
    {
        auto viewList = manifestContent["view"];

        if (viewList.isArray())
        {
            for (auto s : viewList)
                if (auto f = getFileRelativeToManifest (manifestFile, s.getString()))
                    views.push_back (f);
        }
        else if (viewList.isString())
        {
            if (auto f = getFileRelativeToManifest (manifestFile, viewList.getString()))
                views.push_back (f);
        }
    }

    return views;
}

//==============================================================================
/** This looks at the annotation on an endpoint and parses out some common patch properties. */
struct PatchParameterProperties
{
    PatchParameterProperties (const std::string& endpointName,
                              const choc::value::ValueView& endpointAnnotation)
    {
        auto getString = [&] (const char* propName)  { return endpointAnnotation[propName].getWithDefault<std::string_view> ({}); };

        name = getString ("name");

        if (name.empty())
            name = endpointName;

        int defaultNumIntervals = 1000;
        auto textValue = getString ("text");

        if (! textValue.empty())
        {
            auto items = choc::text::splitString (choc::text::removeDoubleQuotes (std::string (textValue)), '|', false);

            if (items.size() > 1)
            {
                defaultNumIntervals = (int) items.size() - 1;
                maxValue = float (defaultNumIntervals);
            }
        }

        unit          = getString ("unit");
        group         = getString ("group");
        textValues    = getString ("text");
        minValue      = endpointAnnotation["min"].getWithDefault<float> (minValue);
        maxValue      = endpointAnnotation["max"].getWithDefault<float> (maxValue);
        step          = endpointAnnotation["step"].getWithDefault<float> (maxValue / static_cast<float> (defaultNumIntervals));
        initialValue  = endpointAnnotation["init"].getWithDefault<float> (minValue);
        rampFrames    = endpointAnnotation["rampFrames"].getWithDefault<uint32_t> (0);
        isAutomatable = endpointAnnotation["automatable"].getWithDefault<bool> (true);
        isBoolean     = endpointAnnotation["boolean"].getWithDefault<bool> (false);
        isHidden      = endpointAnnotation["hidden"].getWithDefault<bool> (false);
    }

    std::string name, unit, group, textValues;
    float minValue = 0, maxValue = 1.0f, step = 0, initialValue = 0;
    uint32_t rampFrames = 0;
    bool isAutomatable = false, isBoolean = false, isHidden = false;
};

#ifdef JUCE_CORE_H_INCLUDED

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

#endif

} // namespace patch
} // namespace soul
