/*
     _____ _____ _____ __
    |   __|     |  |  |  |
    |__   |  |  |  |  |  |__
    |_____|_____|_____|_____|

    Copyright (c) 2018 - ROLI Ltd.
*/

/*
    This file contains various global definitions for data types that are
    needed in various parts of the codebase, both API and the libraries.
*/

#ifndef SOUL_VENUE_MESSAGES_HEADER_INCLUDED
#define SOUL_VENUE_MESSAGES_HEADER_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include "../3rdParty/choc/containers/choc_Value.h"
#include "../3rdParty/choc/audio/choc_MIDI.h"

namespace soul
{

//==============================================================================
enum class SessionState
{
    unknown,
    empty,
    loading,
    loaded,
    linking,
    linked,
    running
};

static constexpr const char* sessionStateToString (SessionState state)
{
    switch (state)
    {
        case SessionState::empty:      return "empty";
        case SessionState::loading:    return "loading";
        case SessionState::loaded:     return "loaded";
        case SessionState::linking:    return "linking";
        case SessionState::linked:     return "linked";
        case SessionState::running:    return "running";

        case SessionState::unknown:
        default:                       return nullptr;
    }
}

static inline SessionState stringToSessionState (std::string_view state)
{
    if (state == "empty")     return SessionState::empty;
    if (state == "loading")   return SessionState::loading;
    if (state == "loaded")    return SessionState::loaded;
    if (state == "linking")   return SessionState::linking;
    if (state == "linked")    return SessionState::linked;
    if (state == "running")   return SessionState::running;

    return SessionState::unknown;
}

enum class EndpointType
{
    unknown  = 0,
    stream   = 1,
    value    = 2,
    event    = 3
};

static constexpr const char* endpointTypeToString (EndpointType type)
{
    switch (type)
    {
        case EndpointType::stream:    return "stream";
        case EndpointType::value:     return "value";
        case EndpointType::event:     return "event";
        case EndpointType::unknown:
        default:                      return nullptr;
    }
}

static inline EndpointType stringToEndpointType (std::string_view type)
{
    if (type == "stream")   return EndpointType::stream;
    if (type == "value")    return EndpointType::value;
    if (type == "event")    return EndpointType::event;

    return EndpointType::unknown;
}

struct Endpoint
{
    std::string ID, name;
    EndpointType type;
    std::vector<choc::value::Type> valueTypes;
    choc::value::Value annotation;
};

//==============================================================================
/** Holds the properties that describe an external variable. */
struct ExternalVariable
{
    std::string name;
    choc::value::Type type;
    choc::value::Value annotation;
};


//==============================================================================
/** This holds a short MIDI message and a frame-based timestamp, and is used in
    various places where buffers of time-stamped MIDI messages are needed.
 */
struct MIDIEvent
{
    uint32_t frameIndex = 0;
    choc::midi::ShortMessage message;

    int32_t getPackedMIDIData() const
    {
        return static_cast<int32_t> ((message.data[0] << 16)
                                      | (message.data[1] << 8)
                                      | message.data[2]);
    }

    static MIDIEvent fromPackedMIDIData (uint32_t frame, int32_t packedData)
    {
        return { frame, { static_cast<uint8_t> (packedData >> 16),
                          static_cast<uint8_t> (packedData >> 8),
                          static_cast<uint8_t> (packedData) } };
    }
};


//==============================================================================
/** Helpers to create objects that can be passed into endpoints for the various
    timeline-related endpoints.
*/
struct TimelineEvents
{
    static choc::value::Value createTimeSigValue()
    {
        return choc::value::createObject ("TimeSignature",
                                          "numerator", choc::value::createInt32 (0),
                                          "denominator", choc::value::createInt32 (0));
    }

    static choc::value::Value createTempoValue()
    {
        return choc::value::createObject ("Tempo",
                                          "bpm", choc::value::createFloat32 (0));
    }

    static choc::value::Value createTransportValue()
    {
        return choc::value::createObject ("TransportState",
                                          "state", choc::value::createInt32 (0));
    }

    static choc::value::Value createPositionValue()
    {
        return choc::value::createObject ("Position",
                                          "currentFrame", choc::value::createInt64 (0),
                                          "currentQuarterNote", choc::value::createFloat64 (0),
                                          "lastBarStartQuarterNote", choc::value::createFloat64 (0));
    }

    static bool isTimeSig   (const choc::value::Type& type)   { return matchesType (type, createTimeSigValue().getType()); }
    static bool isTempo     (const choc::value::Type& type)   { return matchesType (type, createTempoValue().getType()); }
    static bool isTransport (const choc::value::Type& type)   { return matchesType (type, createTransportValue().getType()); }
    static bool isPosition  (const choc::value::Type& type)   { return matchesType (type, createPositionValue().getType()); }

private:
    static bool matchesType (const choc::value::Type& t1, const choc::value::Type& t2)
    {
        auto numElements = t1.getNumElements();

        if (numElements != t2.getNumElements())
            return false;

        if (! (t1.isObject() && t2.isObject() && endsWith (t1.getObjectClassName(), t2.getObjectClassName())))
            return false;

        for (uint32_t i = 0; i < numElements; ++i)
        {
            auto& m1 = t1.getObjectMember (i);
            auto& m2 = t2.getObjectMember (i);

            if (m1.type != m2.type || m1.name != m2.name)
                return false;
        }

        return true;
    }

    static bool endsWith (std::string_view text, std::string_view possibleEnd)
    {
        auto textLen = text.length();
        auto endLen = possibleEnd.length();

        return textLen >= endLen && text.substr (textLen - endLen) == possibleEnd;
    }
};


//==============================================================================
/** Simple class to hold a time-signature. */
struct TimeSignature
{
    uint16_t numerator = 0;     /**< The numerator is the top number in a time-signature, e.g. the 3 of 3/4. */
    uint16_t denominator = 0;   /**< The numerator is the bottom number in a time-signature, e.g. the 4 of 3/4. */

    bool operator== (TimeSignature other) const noexcept   { return numerator == other.numerator && denominator == other.denominator; }
    bool operator!= (TimeSignature other) const noexcept   { return ! operator== (other); }
};

//==============================================================================
/** Represents the state of a host which can play timeline-based material. */
enum class TransportState
{
    stopped    = 0,
    playing    = 1,
    recording  = 2
};

//==============================================================================
/** Represents a position along a timeline, in terms of frames and also (where
    appropriate) quarter notes.
*/
struct TimelinePosition
{
    /** A number of frames from the start of the timeline. */
    int64_t currentFrame = 0;

    /** The number of quarter-notes since the beginning of the timeline.
        A host may not have a meaningful value for this, so it may just be 0.
        Bear in mind that a timeline may contain multiple changes of tempo and
        time-signature, so this value will not necessarily keep increasing at
        a constant rate.
    */
    double currentQuarterNote = 0;

    /** The number of quarter-notes from the beginning of the timeline to the
        start of the current bar.
        A host may not have a meaningful value for this, so it may just be 0.
        You can subtract this from currentQuarterNote to find out how which
        quarter-note the position represents within the current bar.
    */
    double lastBarStartQuarterNote = 0;
};

//==============================================================================
/**
    A collection of properties needed by the compiler, linker and loaders when
    building SOUL programs.
    @see BuildBundle
*/
struct SourceFile
{
    std::string filename, content;
};

using SourceFiles = std::vector<SourceFile>;

struct BuildSettings
{
    double       sampleRate         = 0;
    uint32_t     maxBlockSize       = 0;
    uint64_t     maxStateSize       = 20 * 1024 * 1024;
    uint64_t     maxStackSize       = 20 * 1024 * 1024;
    int          optimisationLevel  = -1;
    int32_t      sessionID          = 0;
    std::string  mainProcessor;
    SourceFiles  overrideStandardLibrary;

    choc::value::Value customSettings;
};

/**
    Contains a complete set of all the sources and settings needed to compile and
    link a program.
*/
struct BuildBundle
{
    SourceFiles sourceFiles;
    BuildSettings settings;
};


} // namespace soul

#endif // SOUL_VENUE_MESSAGES_HEADER_INCLUDED
