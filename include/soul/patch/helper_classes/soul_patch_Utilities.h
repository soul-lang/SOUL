/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

#pragma once

#include "../../soul_patch.h"
#include "../../3rdParty/choc/text/choc_UTF8.h"
#include "../../3rdParty/choc/text/choc_JSON.h"
#include "../../3rdParty/choc/text/choc_StringUtilities.h"

#if __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif

/*
    This file contains some helper classes and functions that are handy for JUCE-based apps
    working with the patch API.
*/

namespace soul
{
namespace patch
{

//==============================================================================
template <typename BaseClass, typename DerivedClass>
struct RefCountHelper  : public BaseClass
{
    RefCountHelper() = default;
    RefCountHelper (const RefCountHelper&) = delete;
    RefCountHelper (RefCountHelper&&) = delete;

    int addRef() noexcept override   { return ++refCount; }
    int release() noexcept override  { auto newCount = --refCount; if (newCount == 0) delete static_cast<DerivedClass*> (this); return newCount; }

    std::atomic<int> refCount { 1 };
};

//==============================================================================
/** A simple soul::patch::String implementation.
    To create one, use the various makeString() functions.
*/
struct StringImpl final  : public RefCountHelper<String, StringImpl>
{
    StringImpl (std::string t) : text (std::move (t)), rawPointer (text.c_str()) {}
    const char* getCharPointer() const override    { return rawPointer; }

    std::string text;
    const char* const rawPointer;
};

inline String* makeStringPtr (std::string s)                     { return new StringImpl (std::move (s)); }

inline String::Ptr makeString (std::string s)                    { return String::Ptr (makeStringPtr (std::move (s))); }
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
    size_t blockSize = fileSize > 0 ? static_cast<size_t> (fileSize) : 8192;
    std::string buffer;
    buffer.resize (blockSize);
    std::ostringstream result;
    uint64_t readPos = 0;

    for (;;)
    {
        auto numRead = f.read (readPos, std::addressof (buffer[0]), (uint64_t) blockSize);

        if (numRead < 0)
        {
            error = "Failed to read from file: " + String::Ptr (f.getAbsolutePath()).toString<std::string>();
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
        if (auto d = data.data()) // the nullptr check here avoids a sanitiser false-alarm
        {
            if (auto invalidUTF8 = choc::text::findInvalidUTF8Data (d, data.size()))
                error = "Invalid UTF8 data at offset " + std::to_string (invalidUTF8 - d);

            return data;
        }
    }

    return {};
}

/** When parsing relative paths from entries in the manifest JSON, this provides a
    handy way to convert those paths into VirtualFile objects.
*/
inline VirtualFile::Ptr getFileRelativeToManifest (VirtualFile& manifest, std::string_view relativePath)
{
    if (auto parent = VirtualFile::Ptr (manifest.getParent()))
        return VirtualFile::Ptr (parent->getChildFile (std::string (relativePath).c_str()));

    return {};
}

//==============================================================================
inline const char* getManifestSuffix()                  { return ".soulpatch"; }
inline const char* getManifestWildcard()                { return "*.soulpatch"; }
inline const char* getManifestTopLevelPropertyName()    { return "soulPatchV1"; }

//==============================================================================
struct PatchLoadError
{
    std::string message;
};

[[noreturn]] static inline void throwPatchLoadError (std::string message)
{
    throw PatchLoadError { std::move (message) };
}

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
        errorMessage = String::Ptr (manifestFile.getAbsolutePath()).toString<std::string>()
                        + ":" + std::to_string (error.line) + ":" + std::to_string (error.column) + ": error: " + error.message;
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

/** Returns a list of the "view" files that a patch's manifest contains. */
inline std::vector<VirtualFile::Ptr> findViewFiles (PatchInstance& instance)
{
    auto desc = soul::patch::Description::Ptr (instance.getDescription());

    if (desc->manifestFile != nullptr)
        return findViewFiles (*desc->manifestFile);

    return {};
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

//==============================================================================
template <typename ValueType>
inline uint32_t readRampLengthAnnotation (const ValueType& v)
{
    static constexpr int64_t maxRampLength = 0x7fffffff;

    if (v.getType().isPrimitive() && (v.getType().isFloatingPoint() || v.getType().isInteger()))
    {
        auto frames = v.getAsInt64();

        if (frames < 0)
            return 0;

        if (frames > maxRampLength)
            return (uint32_t) maxRampLength;

        return (uint32_t) frames;
    }

    return 1000;
}

template <typename EndpointDetailsType>
uint32_t readRampLengthForEndpoint (const EndpointDetailsType& endpoint)
{
    return readRampLengthAnnotation (endpoint.annotation.getValue ("rampFrames"));
}


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

//==============================================================================
static inline juce::var valueToVar (const choc::value::ValueView& value)
{
    try
    {
        if (value.isInt32())    return static_cast<int> (value.getInt32());
        if (value.isInt64())    return static_cast<juce::int64> (value.getInt64());
        if (value.isFloat32())  return value.getFloat32();
        if (value.isFloat64())  return value.getFloat64();
        if (value.isBool())     return value.getBool();
        if (value.isString())   return juce::String (std::string (value.getString()));

        juce::var result;

        if (value.isVector() || value.isArray())
        {
            auto num = value.size();
            auto a = result.getArray();

            for (uint32_t i = 0; i < num; ++i)
                a->add (valueToVar (value[i]));
        }
        else if (value.isObject())
        {
            auto* object = new juce::DynamicObject();
            result = object;
            auto num = value.size();

            for (uint32_t i = 0; i < num; ++i)
            {
                auto m = value.getObjectMemberAt(i);
                object->setProperty (m.name, valueToVar (m.value));
            }
        }

        return result;
    }
    catch (choc::value::Error) {}

    return juce::var::undefined();
}

static inline choc::value::Value varToValue (const choc::value::Type& targetType, const juce::var& value)
{
    struct ConvertFunctions
    {
        [[noreturn]] static void cannotConvertToTarget()     { throw choc::value::Error { "Cannot convert to the target type" }; }

        static choc::value::Value convert (const choc::value::Type& target, const juce::var& source)
        {
            if (source.isInt())
            {
                if (target.isInt32())   return choc::value::createInt32 (static_cast<int> (source));
                if (target.isInt64())   return choc::value::createInt64 (static_cast<juce::int64> (source));
                cannotConvertToTarget();
            }

            if (source.isDouble())
            {
                if (target.isFloat32())   return choc::value::createFloat32 (source);
                if (target.isFloat64())   return choc::value::createFloat64 (source);
                cannotConvertToTarget();
            }

            if (source.isBool())
            {
                if (target.isBool())
                    return choc::value::createBool (source);

                cannotConvertToTarget();
            }

            if (source.isString())
            {
                if (target.isString())
                    return choc::value::createString (source.toString().toStdString());

                cannotConvertToTarget();
            }

            if (source.isArray())
            {
                auto size = static_cast<uint32_t> (source.size());

                if (target.isVector() && target.getNumElements() == size)
                {
                    auto elementType = target.getElementType();

                    if (elementType.isInt32())    return choc::value::createVector (size, [&] (uint32_t i)  { return static_cast<int32_t> (convert (elementType, source[static_cast<int> (i)])); });
                    if (elementType.isInt64())    return choc::value::createVector (size, [&] (uint32_t i)  { return static_cast<int64_t> (convert (elementType, source[static_cast<int> (i)])); });
                    if (elementType.isFloat32())  return choc::value::createVector (size, [&] (uint32_t i)  { return static_cast<float>   (convert (elementType, source[static_cast<int> (i)])); });
                    if (elementType.isFloat64())  return choc::value::createVector (size, [&] (uint32_t i)  { return static_cast<double>  (convert (elementType, source[static_cast<int> (i)])); });
                    if (elementType.isBool())     return choc::value::createVector (size, [&] (uint32_t i)  { return static_cast<bool>    (convert (elementType, source[static_cast<int> (i)])); });
                }

                if (target.isArray() && target.getNumElements() == size)
                    return choc::value::createArray (size, [&] (uint32_t i) { return convert (target.getArrayElementType(i), source[static_cast<int> (i)]); });

                cannotConvertToTarget();
            }

            if (auto o = source.getDynamicObject())
            {
                if (target.isObject())
                {
                    auto& props = o->getProperties();
                    auto numMembers = target.getNumElements();

                    if (numMembers == static_cast<uint32_t> (props.size()))
                    {
                        auto result = choc::value::createObject (target.getObjectClassName());

                        for (uint32_t i = 0; i < numMembers; ++i)
                        {
                            auto& m = target.getObjectMember (i);

                            if (auto v = props.getVarPointer (juce::Identifier (juce::String::CharPointerType (m.name.data()),
                                                                                juce::String::CharPointerType (m.name.data() + m.name.length()))))
                                result.addMember (m.name, convert (m.type, *v));
                            else
                                cannotConvertToTarget();
                        }

                        return result;
                    }
                }
            }

            cannotConvertToTarget();
        }
    };

    return ConvertFunctions::convert (targetType, value);
}

#endif

} // namespace patch
} // namespace soul

#if __clang__
 #pragma clang diagnostic pop
#endif
