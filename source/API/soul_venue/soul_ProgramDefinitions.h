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
#include "../../3rdParty/choc/containers/choc_Value.h"
#include "../../3rdParty/choc/audio/choc_MIDI.h"

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
    unknown,
    stream,
    value,
    event
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
/**
    A collection of properties needed by the compiler, linker and loaders when
    building SOUL programs.
    @see BuildBundle
*/
struct BuildSettings
{
    double       sampleRate         = 0;
    uint32_t     maxBlockSize       = 0;
    size_t       maxStateSize       = 0;
    int          optimisationLevel  = -1;
    int32_t      sessionID          = 0;
    std::string  mainProcessor;
};

/**
    Contains a complete set of all the sources and settings needed to compile and
    link a program.
*/
struct BuildBundle
{
    struct SourceFile
    {
        std::string filename, content;
    };

    std::vector<SourceFile> sourceFiles;
    BuildSettings settings;
    choc::value::Value customSettings;
};


} // namespace soul

#endif // SOUL_VENUE_MESSAGES_HEADER_INCLUDED
